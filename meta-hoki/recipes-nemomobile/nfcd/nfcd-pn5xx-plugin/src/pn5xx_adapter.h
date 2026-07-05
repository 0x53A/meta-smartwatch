/*
 * Copyright (C) 2026 AsteroidOS contributors
 * Licensed under BSD-3-Clause
 */

#ifndef PN5XX_ADAPTER_H
#define PN5XX_ADAPTER_H

#include <nfc_adapter.h>

NfcAdapter*
pn5xx_adapter_new(void);

/* Logging */
extern GLogModule pn5xx_log;
#define GLOG_MODULE_NAME pn5xx_log

#endif /* PN5XX_ADAPTER_H */
