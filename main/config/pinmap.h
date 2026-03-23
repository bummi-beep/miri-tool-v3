#ifndef PINMAP_H
#define PINMAP_H

typedef struct {
    /* Console / debug */
    int uart0_rx;
    int uart0_tx;

    /* Target CAN */
    int target_can_rx;
    int target_can_tx;

    /* Target UART */
    int target_txd;
    int target_rxd;

    /* Target reset / boot */
    int stm32_reset;
    int target_reset;
    int bootmode;

    /* SDMMC */
    int sd_cmd;
    int sd_clk;
    int sd_d0;
    int sd_d1;
    int sd_d2;
    int sd_d3;
    int sd_vcc_target_val;

    /* HHT UART (WB100 등: UART1) */
    int hht_rx;
    int hht_tx;
    int hht_rts;
    int hht_cts;

    /* HHT 3000 전용 핀 (WB100과 동일 UART1, 배타 사용 시 TX/RX만 4/5로 사용) */
    int hht3000_rx;
    int hht3000_tx;
    int hht3000_rts;
    int hht3000_cts;

    /* Command UART */
    int cmd_txd;
    int cmd_rxd;

    /* USB */
    int usb1_d_plus;
    int usb1_d_minus;

    /* SWD/JTAG */
    int swdio_in_out_select;
    int target_swdio_tms_in;
    int target_swclk_tck;
    int swdio_tms_out;
    int target_swo_tdo;
    int target_tdi;
} pinmap_t;

const pinmap_t *pinmap_get(void);

#endif

