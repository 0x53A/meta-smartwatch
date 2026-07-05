/*
 * nfcd-pn5xx-plugin: NciAdapter that talks directly to /dev/nq-nci
 *
 * Replaces the Android NFC HAL chain (hwbinder → nfc_nci_nxp.so) with
 * direct I/O to the PN553 kernel driver. Handles:
 *   - Power management via PN553 ioctls (VEN GPIO)
 *   - NXP RF antenna config injection after CORE_INIT
 *   - Byte-level I/O between libncicore and the NFC chip
 *
 * Copyright (C) 2026 AsteroidOS contributors
 * Licensed under BSD-3-Clause
 */

#include "pn5xx_adapter.h"

#include <nci_adapter_impl.h>
#include <nci_core.h>
#include <nci_hal.h>

#include <gutil_log.h>
#include <gutil_misc.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <pthread.h>

/* PN553 kernel driver ioctl definitions */
#define PN544_MAGIC 0xE9
#define PN544_SET_PWR _IOW(PN544_MAGIC, 0x01, long)

/* Default device node and config paths */
#define DEFAULT_DEV_NODE "/dev/nq-nci"
#define DEFAULT_RF_CONFIG "/etc/nfc/nxp-rf-config.bin"

/* Max NCI frame size */
#define NCI_MAX_FRAME 258

/* Config file format: [2-byte BE length][NCI cmd bytes]... [0x0000] */

typedef struct pn5xx_adapter {
    NciAdapter adapter;         /* parent class */
    NciHalIo hal_io;
    NciHalClient* hal_client;   /* provided by libncicore in start() */

    int fd;                     /* /dev/nq-nci file descriptor */
    gboolean power_on;
    gboolean need_power;
    gboolean power_switch_pending;
    gboolean core_initialized;

    /* Read thread */
    GThread* read_thread;
    volatile gboolean read_running;

    /* RF config injection */
    guint8* rf_config;          /* loaded binary blob */
    gsize rf_config_len;
    gsize rf_config_offset;     /* current position in blob */
    gboolean rf_config_sent;    /* TRUE after RF config injected */
    volatile gboolean configuring; /* read thread should not feed to ncicore */
    int config_pipe[2];         /* config responses: read thread → main */
    guint config_watch_id;      /* GLib watch on config_pipe[0] */
} Pn5xxAdapter;

typedef NciAdapterClass Pn5xxAdapterClass;

GType pn5xx_adapter_get_type(void) G_GNUC_INTERNAL;
G_DEFINE_TYPE(Pn5xxAdapter, pn5xx_adapter, NCI_TYPE_ADAPTER)

#define THIS_TYPE pn5xx_adapter_get_type()
#define THIS(obj) G_TYPE_CHECK_INSTANCE_CAST(obj, THIS_TYPE, Pn5xxAdapter)

/* ================================================================== */
/* Device I/O helpers                                                  */
/* ================================================================== */

static
gboolean
pn5xx_power_ioctl(
    int fd,
    long arg)
{
    if (ioctl(fd, PN544_SET_PWR, arg) < 0) {
        GERR("PN544_SET_PWR(%ld) failed: %s", arg, strerror(errno));
        return FALSE;
    }
    return TRUE;
}

static
gboolean
pn5xx_open_device(
    Pn5xxAdapter* self)
{
    if (self->fd >= 0) {
        return TRUE;
    }

    self->fd = open(DEFAULT_DEV_NODE, O_RDWR);
    if (self->fd < 0) {
        GERR("Failed to open %s: %s", DEFAULT_DEV_NODE, strerror(errno));
        return FALSE;
    }
    GDEBUG("Opened %s (fd=%d)", DEFAULT_DEV_NODE, self->fd);
    return TRUE;
}

static
void
pn5xx_close_device(
    Pn5xxAdapter* self)
{
    if (self->fd >= 0) {
        GDEBUG("Closing %s", DEFAULT_DEV_NODE);
        close(self->fd);
        self->fd = -1;
    }
}

static
gboolean
pn5xx_power_on(
    Pn5xxAdapter* self)
{
    if (!pn5xx_open_device(self)) {
        return FALSE;
    }

    /* Power cycle: off, wait, on (driver adds 100ms VEN delay) */
    if (!pn5xx_power_ioctl(self->fd, 0)) {
        return FALSE;
    }
    g_usleep(100000); /* 100ms with VEN low */
    if (!pn5xx_power_ioctl(self->fd, 1)) {
        return FALSE;
    }
    g_usleep(100000); /* 100ms extra after VEN high */

    GINFO("NFC chip powered on");
    return TRUE;
}

static
void
pn5xx_power_off(
    Pn5xxAdapter* self)
{
    if (self->fd >= 0) {
        pn5xx_power_ioctl(self->fd, 0);
        GINFO("NFC chip powered off");
    }
}

/* ================================================================== */
/* NCI write to device                                                 */
/* ================================================================== */

static
gboolean
pn5xx_write_to_chip(
    Pn5xxAdapter* self,
    const void* data,
    gsize len)
{
    ssize_t written = write(self->fd, data, len);

    if (written < 0) {
        GERR("NCI write failed: %s", strerror(errno));
        return FALSE;
    }
    if ((gsize) written != len) {
        GERR("NCI write short: %d/%d", (int) written, (int) len);
        return FALSE;
    }
    return TRUE;
}

/* ================================================================== */
/* Read thread                                                         */
/* ================================================================== */

typedef struct pn5xx_read_data {
    Pn5xxAdapter* self;
    guint8 buf[NCI_MAX_FRAME];
    gsize len;
} Pn5xxReadData;

static
gboolean
pn5xx_read_dispatch(
    gpointer user_data)
{
    Pn5xxReadData* rd = user_data;
    Pn5xxAdapter* self = rd->self;
    NciHalClient* client = self->hal_client;

    if (client && !self->configuring) {
        client->fn->read(client, rd->buf, rd->len);
    }
    g_slice_free(Pn5xxReadData, rd);
    return G_SOURCE_REMOVE;
}

static
gpointer
pn5xx_read_thread_func(
    gpointer user_data)
{
    Pn5xxAdapter* self = user_data;
    guint8 buf[NCI_MAX_FRAME];

    GDEBUG("Read thread started");
    while (self->read_running) {
        /* Blocking read — driver waits for IRQ */
        ssize_t n = read(self->fd, buf, sizeof(buf));

        if (!self->read_running) {
            break;
        }
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            GERR("NCI read error: %s", strerror(errno));
            break;
        }
        if (n < 3) {
            continue; /* Too short for NCI header */
        }

        /* Parse actual length from NCI header */
        gsize nci_len = 3 + buf[2];

        if (nci_len > (gsize) n) {
            nci_len = n;
        }

        if (self->configuring) {
            /* During RF config phase, send response through pipe */
            if (self->config_pipe[1] >= 0) {
                /* Write length prefix + data */
                guint16 plen = (guint16) nci_len;

                (void) write(self->config_pipe[1], &plen, 2);
                (void) write(self->config_pipe[1], buf, nci_len);
            }
        } else {
            /* Normal operation: dispatch to main loop for libncicore */
            Pn5xxReadData* rd = g_slice_new(Pn5xxReadData);

            rd->self = self;
            memcpy(rd->buf, buf, nci_len);
            rd->len = nci_len;
            g_idle_add(pn5xx_read_dispatch, rd);
        }
    }
    GDEBUG("Read thread exiting");
    return NULL;
}

static
void
pn5xx_start_read_thread(
    Pn5xxAdapter* self)
{
    if (!self->read_thread) {
        self->read_running = TRUE;
        self->read_thread = g_thread_new("pn5xx-read",
            pn5xx_read_thread_func, self);
    }
}

static
void
pn5xx_stop_read_thread(
    Pn5xxAdapter* self)
{
    if (self->read_thread) {
        self->read_running = FALSE;
        /*
         * The read thread is blocked in read() waiting for IRQ.
         * Power off the chip to unblock it (triggers IRQ or error).
         */
        if (self->fd >= 0) {
            pn5xx_power_ioctl(self->fd, 0);
            g_usleep(10000);
            pn5xx_power_ioctl(self->fd, 1);
        }
        g_thread_join(self->read_thread);
        self->read_thread = NULL;
    }
}

/* ================================================================== */
/* NXP RF config injection                                             */
/* ================================================================== */

static
gboolean
pn5xx_load_rf_config(
    Pn5xxAdapter* self)
{
    gsize len = 0;
    GError* error = NULL;

    if (self->rf_config) {
        return TRUE; /* Already loaded */
    }

    if (!g_file_get_contents(DEFAULT_RF_CONFIG, (gchar**) &self->rf_config,
            &len, &error)) {
        GWARN("No RF config at %s: %s", DEFAULT_RF_CONFIG,
            error ? error->message : "unknown error");
        g_error_free(error);
        /* Not fatal — chip works without RF tuning, just poorly */
        return FALSE;
    }

    self->rf_config_len = len;
    GINFO("Loaded RF config from %s (%d bytes)", DEFAULT_RF_CONFIG, (int) len);
    return TRUE;
}

static gboolean pn5xx_config_send_next(Pn5xxAdapter* self);

static
gboolean
pn5xx_config_pipe_readable(
    GIOChannel* source,
    GIOCondition condition,
    gpointer user_data)
{
    Pn5xxAdapter* self = user_data;
    guint16 plen = 0;
    guint8 buf[NCI_MAX_FRAME];

    /* Read length prefix */
    if (read(self->config_pipe[0], &plen, 2) != 2 || plen == 0) {
        GERR("Config pipe read error");
        goto done;
    }

    /* Read response */
    if (read(self->config_pipe[0], buf, plen) != (ssize_t) plen) {
        GERR("Config pipe response read error");
        goto done;
    }

    /* Check NCI response status (byte 3 for most responses) */
    if (plen >= 4) {
        guint8 status = buf[3];

        GDEBUG("RF config response: status=0x%02x", status);
        if (status != 0x00 && status != 0x05) {
            /* 0x05 = SYNTAX_ERROR is non-critical (some NXP params unsupported) */
            GWARN("RF config command failed: status=0x%02x", status);
        }
    }

    /* Send next config command or finish */
    if (!pn5xx_config_send_next(self)) {
        goto done;
    }
    return G_SOURCE_CONTINUE;

done:
    /* Config complete — clean up and let libncicore proceed */
    GINFO("RF config injection complete");
    self->configuring = FALSE;
    self->config_watch_id = 0;
    if (self->config_pipe[0] >= 0) {
        close(self->config_pipe[0]);
        self->config_pipe[0] = -1;
    }
    if (self->config_pipe[1] >= 0) {
        close(self->config_pipe[1]);
        self->config_pipe[1] = -1;
    }
    /* Resume normal NCI operation */
    nci_core_set_state(self->adapter.nci, NCI_RFST_IDLE);
    return G_SOURCE_REMOVE;
}

static
gboolean
pn5xx_config_send_next(
    Pn5xxAdapter* self)
{
    if (!self->rf_config || self->rf_config_offset >= self->rf_config_len) {
        return FALSE;
    }

    /* Read 2-byte big-endian length */
    const guint8* p = self->rf_config + self->rf_config_offset;
    guint16 cmd_len = (p[0] << 8) | p[1];

    if (cmd_len == 0) {
        return FALSE; /* End marker */
    }

    self->rf_config_offset += 2;
    if (self->rf_config_offset + cmd_len > self->rf_config_len) {
        GERR("RF config truncated at offset %d", (int) self->rf_config_offset);
        return FALSE;
    }

    const guint8* cmd = self->rf_config + self->rf_config_offset;

    GDEBUG("Sending RF config command (%d bytes)", cmd_len);
    pn5xx_write_to_chip(self, cmd, cmd_len);
    self->rf_config_offset += cmd_len;
    return TRUE;
}

static
void
pn5xx_inject_rf_config(
    Pn5xxAdapter* self)
{
    if (!pn5xx_load_rf_config(self)) {
        /* No config file — proceed without RF tuning */
        nci_core_set_state(self->adapter.nci, NCI_RFST_IDLE);
        return;
    }

    self->rf_config_offset = 0;

    /* Create pipe for config responses (read thread → main loop) */
    if (pipe(self->config_pipe) < 0) {
        GERR("pipe() failed: %s", strerror(errno));
        nci_core_set_state(self->adapter.nci, NCI_RFST_IDLE);
        return;
    }

    /* Watch the read end in the GLib main loop */
    GIOChannel* channel = g_io_channel_unix_new(self->config_pipe[0]);

    g_io_channel_set_encoding(channel, NULL, NULL);
    g_io_channel_set_buffered(channel, FALSE);
    self->config_watch_id = g_io_add_watch(channel, G_IO_IN,
        pn5xx_config_pipe_readable, self);
    g_io_channel_unref(channel);

    /* Enter config mode and send first command */
    self->configuring = TRUE;
    if (!pn5xx_config_send_next(self)) {
        /* No commands in config */
        self->configuring = FALSE;
        close(self->config_pipe[0]);
        close(self->config_pipe[1]);
        self->config_pipe[0] = self->config_pipe[1] = -1;
        nci_core_set_state(self->adapter.nci, NCI_RFST_IDLE);
    }
}

/* ================================================================== */
/* NciHalIo implementation                                             */
/* ================================================================== */

static
Pn5xxAdapter*
pn5xx_adapter_from_hal_io(
    NciHalIo* io)
{
    return (Pn5xxAdapter*)((guint8*) io -
        G_STRUCT_OFFSET(Pn5xxAdapter, hal_io));
}

static
gboolean
pn5xx_hal_io_start(
    NciHalIo* io,
    NciHalClient* client)
{
    Pn5xxAdapter* self = pn5xx_adapter_from_hal_io(io);

    GDEBUG("HAL I/O start");
    self->hal_client = client;
    return TRUE;
}

static
void
pn5xx_hal_io_stop(
    NciHalIo* io)
{
    Pn5xxAdapter* self = pn5xx_adapter_from_hal_io(io);

    GDEBUG("HAL I/O stop");
    self->hal_client = NULL;
}

/* ================================================================== */
/* Pre-init: synchronous CORE_RESET + CORE_INIT + RF config            */
/* Done before read thread starts, before libncicore takes over        */
/* ================================================================== */

static
gboolean
pn5xx_nci_transact(
    Pn5xxAdapter* self,
    const guint8* cmd,
    gsize cmd_len,
    guint8* rsp,
    gsize rsp_size,
    gsize* rsp_len)
{
    /* Write command */
    if (!pn5xx_write_to_chip(self, cmd, cmd_len)) {
        return FALSE;
    }

    /* Blocking read for response (driver waits for IRQ) */
    ssize_t n = read(self->fd, rsp, rsp_size);

    if (n < 3) {
        GERR("Pre-init read failed: %s", n < 0 ? strerror(errno) : "short");
        return FALSE;
    }
    if (rsp_len) {
        *rsp_len = 3 + rsp[2];
    }
    return TRUE;
}

static
void
pn5xx_do_pre_init(
    Pn5xxAdapter* self)
{
    guint8 rsp[NCI_MAX_FRAME];
    gsize rsp_len;

    /* CORE_RESET (reset configuration) */
    static const guint8 core_reset[] = { 0x20, 0x00, 0x01, 0x00 };

    GINFO("Pre-init: CORE_RESET");
    if (!pn5xx_nci_transact(self, core_reset, sizeof(core_reset),
            rsp, sizeof(rsp), &rsp_len)) {
        GERR("CORE_RESET failed");
        return;
    }
    /* Read the CORE_RESET_NTF that follows RSP */
    (void) read(self->fd, rsp, sizeof(rsp));

    /* CORE_INIT (NCI 2.0 format with feature enable bytes) */
    static const guint8 core_init[] = { 0x20, 0x01, 0x02, 0x00, 0x00 };

    GINFO("Pre-init: CORE_INIT");
    if (!pn5xx_nci_transact(self, core_init, sizeof(core_init),
            rsp, sizeof(rsp), &rsp_len)) {
        GERR("CORE_INIT failed");
        return;
    }

    /* NXP RF config */
    if (pn5xx_load_rf_config(self)) {
        GINFO("Pre-init: sending %d bytes of RF config", (int) self->rf_config_len);
        self->rf_config_offset = 0;
        int cmd_count = 0;

        while (self->rf_config_offset + 2 <= self->rf_config_len) {
            const guint8* p = self->rf_config + self->rf_config_offset;
            guint16 cmd_len = (p[0] << 8) | p[1];

            if (cmd_len == 0) {
                break;
            }

            self->rf_config_offset += 2;
            if (self->rf_config_offset + cmd_len > self->rf_config_len) {
                break;
            }

            const guint8* cmd = self->rf_config + self->rf_config_offset;

            if (!pn5xx_nci_transact(self, cmd, cmd_len,
                    rsp, sizeof(rsp), &rsp_len)) {
                GWARN("RF config command %d failed", cmd_count);
            }
            self->rf_config_offset += cmd_len;
            cmd_count++;
        }
        GINFO("Pre-init: sent %d RF config commands", cmd_count);
    }

    GINFO("Pre-init complete");
}

static
void
pn5xx_send_rf_config_sync(
    Pn5xxAdapter* self)
{
    /*
     * Send NXP RF config commands synchronously. The read thread is running
     * and will feed responses to libncicore, which will see unexpected
     * CORE_SET_CONFIG_RSP responses. Since libncicore is in IDLE state and
     * not waiting for any specific response at this point, these should be
     * harmlessly ignored (or logged as warnings).
     *
     * We send the config right before RF_DISCOVER_MAP because at that point
     * CORE_RESET + CORE_INIT have already completed.
     */
    if (!pn5xx_load_rf_config(self)) {
        return;
    }

    GINFO("Injecting NXP RF config (%d bytes)", (int) self->rf_config_len);
    self->rf_config_offset = 0;

    int cmd_count = 0;

    while (self->rf_config_offset + 2 <= self->rf_config_len) {
        const guint8* p = self->rf_config + self->rf_config_offset;
        guint16 cmd_len = (p[0] << 8) | p[1];

        if (cmd_len == 0) {
            break; /* End marker */
        }

        self->rf_config_offset += 2;
        if (self->rf_config_offset + cmd_len > self->rf_config_len) {
            break;
        }

        const guint8* cmd = self->rf_config + self->rf_config_offset;

        pn5xx_write_to_chip(self, cmd, cmd_len);
        self->rf_config_offset += cmd_len;
        cmd_count++;

        /* Brief pause to let chip process and send response */
        g_usleep(10000); /* 10ms */
    }
    GINFO("Sent %d RF config commands", cmd_count);
}

static
gboolean
pn5xx_hal_io_write(
    NciHalIo* io,
    const GUtilData* chunks,
    guint count,
    NciHalClientFunc complete)
{
    Pn5xxAdapter* self = pn5xx_adapter_from_hal_io(io);
    gboolean ok = FALSE;

    /* Collect the data to inspect the NCI command */
    const guint8* data = NULL;
    gsize total = 0;
    guint8* tmp_buf = NULL;

    if (count == 1) {
        data = chunks[0].bytes;
        total = chunks[0].size;
    } else if (count > 1) {
        guint i;

        for (i = 0; i < count; i++) {
            total += chunks[i].size;
        }
        tmp_buf = g_malloc(total);
        gsize off = 0;

        for (i = 0; i < count; i++) {
            memcpy(tmp_buf + off, chunks[i].bytes, chunks[i].size);
            off += chunks[i].size;
        }
        data = tmp_buf;
    }

    if (data && total >= 2) {
        GDEBUG("HAL write: %02x %02x (%d bytes) rf_sent=%d",
            data[0], data[1], (int) total, self->rf_config_sent);
        /*
         * Intercept RF_DISCOVER_MAP_CMD (0x21 0x00) and inject RF config
         * right before it. At this point CORE_INIT is done and the chip
         * is ready for config commands.
         *
         * We set configuring=TRUE so the read thread sends responses to
         * our config_pipe instead of libncicore. Then we do synchronous
         * write+read for each config command. Finally we unpause.
         */
        if (!self->rf_config_sent && data[0] == 0x21 && data[1] == 0x00) {
            self->rf_config_sent = TRUE;

            if (pn5xx_load_rf_config(self)) {
                guint8 rsp[NCI_MAX_FRAME];

                GINFO("Injecting RF config before RF_DISCOVER_MAP");

                /*
                 * Tell read thread to skip (not consume data).
                 * The read thread checks configuring flag and
                 * will send to config_pipe if set. But since we
                 * don't set up the pipe, it will just loop.
                 * Actually, set configuring=TRUE so read thread
                 * skips the read() entirely by using a short sleep.
                 *
                 * Simpler: just use pn5xx_nci_transact which does
                 * synchronous write+read. The read thread is blocked
                 * on read() — when our write triggers a chip response,
                 * either we or the read thread gets it. To avoid the
                 * race, temporarily stop the read thread.
                 */
                /*
                 * The read thread is blocked on read() waiting for IRQ.
                 * We need exclusive fd access for config I/O.
                 * Close and reopen the fd — this will make the read
                 * thread's read() return with an error, causing it to exit.
                 */
                int old_fd = self->fd;

                self->read_running = FALSE;
                self->fd = open(DEFAULT_DEV_NODE, O_RDWR);
                close(old_fd); /* Unblocks read thread */
                g_thread_join(self->read_thread);
                self->read_thread = NULL;

                self->rf_config_offset = 0;
                int cmd_count = 0;

                while (self->rf_config_offset + 2 <= self->rf_config_len) {
                    const guint8* p = self->rf_config + self->rf_config_offset;
                    guint16 cmd_len = (p[0] << 8) | p[1];

                    if (cmd_len == 0) break;
                    self->rf_config_offset += 2;
                    if (self->rf_config_offset + cmd_len > self->rf_config_len) break;

                    const guint8* cmd = self->rf_config + self->rf_config_offset;
                    gsize rsp_len;

                    pn5xx_nci_transact(self, cmd, cmd_len,
                        rsp, sizeof(rsp), &rsp_len);
                    self->rf_config_offset += cmd_len;
                    cmd_count++;
                }
                GINFO("Injected %d RF config commands", cmd_count);

                /* Restart read thread */
                pn5xx_start_read_thread(self);
            }
        }

        ok = pn5xx_write_to_chip(self, data, total);
    }

    g_free(tmp_buf);

    if (complete) {
        complete(self->hal_client, ok);
    }
    return ok;
}

static
void
pn5xx_hal_io_cancel_write(
    NciHalIo* io)
{
    /* Writes are synchronous, nothing to cancel */
}

/* ================================================================== */
/* NCI state machine hooks                                             */
/* ================================================================== */

static
void
pn5xx_adapter_current_state_changed(
    NciAdapter* adapter)
{
    Pn5xxAdapter* self = THIS(adapter);
    NciCore* nci = adapter->nci;

    /* Call parent handler */
    NCI_ADAPTER_CLASS(pn5xx_adapter_parent_class)->
        current_state_changed(adapter);

    GDEBUG("NCI state: %d -> %d (next=%d)",
        nci->current_state, nci->current_state, nci->next_state);

    if (self->power_on && self->need_power && !self->configuring) {
        if (nci->current_state == NCI_RFST_IDLE &&
            nci->next_state == NCI_RFST_IDLE) {
            if (!self->core_initialized) {
                self->core_initialized = TRUE;
                GINFO("NCI core initialized, injecting RF config");
                pn5xx_inject_rf_config(self);
            }
        }
    }
}

static
void
pn5xx_adapter_next_state_changed(
    NciAdapter* adapter)
{
    /* Call parent handler */
    NCI_ADAPTER_CLASS(pn5xx_adapter_parent_class)->
        next_state_changed(adapter);

    pn5xx_adapter_current_state_changed(adapter);
}

/* ================================================================== */
/* NfcAdapter power management                                         */
/* ================================================================== */

static
gboolean
pn5xx_adapter_submit_power_request(
    NfcAdapter* adapter,
    gboolean on)
{
    Pn5xxAdapter* self = THIS(adapter);
    NciCore* nci = self->adapter.nci;

    self->need_power = on;

    if (on) {
        if (self->power_on) {
            GDEBUG("Already powered on");
            nci_core_set_state(nci, NCI_RFST_IDLE);
            return FALSE; /* No pending operation */
        }

        if (!pn5xx_power_on(self)) {
            GERR("Power on failed");
            return FALSE;
        }

        /* Send RF config synchronously before libncicore starts */
        pn5xx_do_pre_init(self);
        pn5xx_start_read_thread(self);
        self->power_on = TRUE;
        self->core_initialized = FALSE;
        self->rf_config_sent = TRUE; /* Already done in pre-init */
        self->power_switch_pending = TRUE;

        nci_core_set_state(nci, NCI_RFST_IDLE);
        nfc_adapter_power_notify(adapter, TRUE, TRUE);
        self->power_switch_pending = FALSE;
    } else {
        if (!self->power_on) {
            GDEBUG("Already powered off");
            return FALSE;
        }

        /* Drive state machine to idle before powering off */
        if (nci->current_state >= NCI_RFST_IDLE) {
            nci_core_set_state(nci, NCI_STATE_STOP);
        }

        self->power_switch_pending = TRUE;
        pn5xx_stop_read_thread(self);
        pn5xx_power_off(self);
        self->power_on = FALSE;
        self->core_initialized = FALSE;

        nfc_adapter_power_notify(adapter, FALSE, TRUE);
        self->power_switch_pending = FALSE;
    }
    return FALSE; /* Operation completed synchronously */
}

static
void
pn5xx_adapter_cancel_power_request(
    NfcAdapter* adapter)
{
    Pn5xxAdapter* self = THIS(adapter);

    self->need_power = self->power_on;
    self->power_switch_pending = FALSE;
}

/* ================================================================== */
/* GObject lifecycle                                                   */
/* ================================================================== */

static
void
pn5xx_adapter_init(
    Pn5xxAdapter* self)
{
    static const NciHalIoFunctions hal_io_functions = {
        .start = pn5xx_hal_io_start,
        .stop = pn5xx_hal_io_stop,
        .write = pn5xx_hal_io_write,
        .cancel_write = pn5xx_hal_io_cancel_write
    };

    self->fd = -1;
    self->config_pipe[0] = -1;
    self->config_pipe[1] = -1;
    self->hal_io.fn = &hal_io_functions;
    nci_adapter_init_base(&self->adapter, &self->hal_io);
}

static
void
pn5xx_adapter_finalize(
    GObject* object)
{
    Pn5xxAdapter* self = THIS(object);

    pn5xx_stop_read_thread(self);
    pn5xx_power_off(self);
    pn5xx_close_device(self);

    if (self->config_watch_id) {
        g_source_remove(self->config_watch_id);
    }
    if (self->config_pipe[0] >= 0) {
        close(self->config_pipe[0]);
    }
    if (self->config_pipe[1] >= 0) {
        close(self->config_pipe[1]);
    }

    g_free(self->rf_config);
    nci_adapter_finalize_core(&self->adapter);
    G_OBJECT_CLASS(pn5xx_adapter_parent_class)->finalize(object);
}

static
void
pn5xx_adapter_class_init(
    Pn5xxAdapterClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    NfcAdapterClass* nfc_adapter_class = NFC_ADAPTER_CLASS(klass);

    /* NCI adapter state change hooks */
    klass->current_state_changed = pn5xx_adapter_current_state_changed;
    klass->next_state_changed = pn5xx_adapter_next_state_changed;

    /* NfcAdapter power management */
    nfc_adapter_class->submit_power_request =
        pn5xx_adapter_submit_power_request;
    nfc_adapter_class->cancel_power_request =
        pn5xx_adapter_cancel_power_request;

    object_class->finalize = pn5xx_adapter_finalize;
}

/* ================================================================== */
/* Public API                                                          */
/* ================================================================== */

NfcAdapter*
pn5xx_adapter_new(void)
{
    /* Verify device exists before creating adapter */
    if (access(DEFAULT_DEV_NODE, R_OK | W_OK) != 0) {
        GERR("%s not accessible: %s", DEFAULT_DEV_NODE, strerror(errno));
        return NULL;
    }

    Pn5xxAdapter* self = g_object_new(THIS_TYPE, NULL);

    GINFO("Created PN5xx NFC adapter (%s)", DEFAULT_DEV_NODE);
    return NFC_ADAPTER(self);
}
