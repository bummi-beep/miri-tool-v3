#include "pinmap.h"

/*
 * Pin map derived from the provided schematic image.
 * Note: IO35/IO36/IO37 are marked "DON'T USE" on the board (N8R8).
 */
static const pinmap_t g_pinmap = {
    /* Console / debug (UART0) */
    .uart0_rx = 44, /* RXD0 */
    .uart0_tx = 43, /* TXD0 */

    /* Target CAN */
    .target_can_rx = 1, /* IO1 */
    .target_can_tx = 2, /* IO2 */

    /* Target UART */
    .target_txd = 4, /* IO4 */
    .target_rxd = 5, /* IO5 */

    /* Target reset / boot */
    .stm32_reset = 3,   /* IO3 */
    .target_reset = 48, /* IO48 */
    .bootmode = 47,     /* IO47 */

    /* SDMMC */
    .sd_cmd = 6,             /* IO6 */
    .sd_clk = 7,             /* IO7 */
    .sd_d0 = 15,             /* IO15 */
    .sd_d1 = 16,             /* IO16 */
    .sd_d2 = 17,             /* IO17 */
    .sd_d3 = 18,             /* IO18 */
    .sd_vcc_target_val = 8,  /* IO8 */

    /* HHT UART (UART1) */
    .hht_rx = 9,  /* IO9 */
    .hht_tx = 10, /* IO10 */
    .hht_rts = 11, /* IO11 */
    .hht_cts = 12, /* IO12 */

    /* hht_uart_init_3000 은 cmd_txd/cmd_rxd 만 사용 (IO13/14). 값 동기화용 */
    .hht3000_tx = 13,
    .hht3000_rx = 14,
    .hht3000_rts = -1,
    .hht3000_cts = -1,

    /* Command UART: ESP32 IO13(TXD)→STM32 RX(PA10), IO14(RXD)←STM32 TX(PA9) */
    .cmd_txd = 13, /* IO13 */
    .cmd_rxd = 14, /* IO14 */

    /* USB */
    .usb1_d_plus = 19,  /* IO19 */
    .usb1_d_minus = 20, /* IO20 */

    /* SWD/JTAG */
    .swdio_in_out_select = 21,  /* IO21 */
    .target_swdio_tms_in = 38,  /* IO38 */
    .target_swclk_tck = 39,     /* IO39 */
    .swdio_tms_out = 40,        /* IO40 */
    .target_swo_tdo = 41,       /* IO41 */
    .target_tdi = 42,           /* IO42 */
};

const pinmap_t *pinmap_get(void) {
    return &g_pinmap;
}

