#ifndef SYSTEM_INIT_H
#define SYSTEM_INIT_H

/*
 * system_init()
 * - Aggregates the "boot baseline" initialization steps.
 * - Keep this focused on the minimal, always-on system setup:
 *   logging, NVS, and the default event loop.
 * - Higher-level features (net/ble/wifi/etc.) should live in their
 *   own init modules and be called from app_main in the desired order.
 */
void system_init(void);

#endif

