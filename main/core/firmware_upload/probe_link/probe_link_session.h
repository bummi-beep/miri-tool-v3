#ifndef PROBE_LINK_SESSION_H
#define PROBE_LINK_SESSION_H

/*
 * Probe link session layer
 * - High-level SWD/JTAG sequence (scan, attach, erase, write, reset)
 * - Built on top of probe_link_transport/protocol
 */

#include <stdbool.h>
#include <esp_err.h>

typedef enum {
    PROBE_LINK_IFACE_SWD = 0,
    PROBE_LINK_IFACE_JTAG = 1,
} probe_link_iface_t;

typedef struct {
    probe_link_iface_t iface;
    int target_index;
    bool reset_after;
} probe_link_session_cfg_t;

esp_err_t probe_link_session_run(const probe_link_session_cfg_t *cfg);

#endif
