/*
 * nfcd-pn5xx-plugin: Direct NCI plugin for NXP PN5xx NFC controllers.
 *
 * Talks directly to /dev/nq-nci (PN553 kernel driver) without going
 * through the Android NFC HAL. Handles power management via ioctls
 * and injects NXP RF antenna tuning config after NCI CORE_INIT.
 *
 * Copyright (C) 2026 AsteroidOS contributors
 * Licensed under BSD-3-Clause (same as nfcd/libncicore)
 */

#include "pn5xx_adapter.h"

#include <nfc_plugin_impl.h>
#include <nfc_manager.h>

#include <gutil_log.h>
#include <stdio.h>

typedef NfcPluginClass Pn5xxNfcPluginClass;
typedef struct pn5xx_nfc_plugin {
    NfcPlugin parent;
    NfcManager* manager;
    NfcAdapter* adapter;
} Pn5xxNfcPlugin;

G_DEFINE_TYPE(Pn5xxNfcPlugin, pn5xx_nfc_plugin, NFC_TYPE_PLUGIN)

static
gboolean
pn5xx_nfc_plugin_start(
    NfcPlugin* plugin,
    NfcManager* manager)
{
    Pn5xxNfcPlugin* self = (Pn5xxNfcPlugin*) plugin;

    fprintf(stderr, "[pn5xx] start() called\n");
    GINFO("Starting PN5xx NFC plugin");
    self->manager = manager;
    self->adapter = pn5xx_adapter_new();
    fprintf(stderr, "[pn5xx] adapter_new returned %p\n", (void*) self->adapter);
    if (self->adapter) {
        const char* name = nfc_manager_add_adapter(manager, self->adapter);
        fprintf(stderr, "[pn5xx] adapter registered as '%s'\n", name ? name : "(null)");
        /* Request power — mce will override if display is off */
        nfc_manager_request_power(manager, TRUE);
        return TRUE;
    }
    return FALSE;
}

static
void
pn5xx_nfc_plugin_stop(
    NfcPlugin* plugin)
{
    Pn5xxNfcPlugin* self = (Pn5xxNfcPlugin*) plugin;

    GINFO("Stopping PN5xx NFC plugin");
    if (self->adapter) {
        nfc_manager_remove_adapter(self->manager, self->adapter->name);
        nfc_adapter_unref(self->adapter);
        self->adapter = NULL;
    }
    self->manager = NULL;
}

static
void
pn5xx_nfc_plugin_init(
    Pn5xxNfcPlugin* self)
{
}

static
void
pn5xx_nfc_plugin_class_init(
    Pn5xxNfcPluginClass* klass)
{
    klass->start = pn5xx_nfc_plugin_start;
    klass->stop = pn5xx_nfc_plugin_stop;
}

static
NfcPlugin*
pn5xx_nfc_plugin_create(void)
{
    return g_object_new(pn5xx_nfc_plugin_get_type(), NULL);
}

/* Log module */
GLogModule pn5xx_log = {
    .name = "pn5xx",
    .parent = NULL,
    .max_level = GLOG_LEVEL_MAX,
    .level = GLOG_LEVEL_VERBOSE,
    .flags = 0
};

static GLogModule* pn5xx_logs[] = {
    &pn5xx_log,
    NULL
};

NFC_PLUGIN_DEFINE2(pn5xx, "PN5xx direct NCI",
    pn5xx_nfc_plugin_create, pn5xx_logs, 0)
