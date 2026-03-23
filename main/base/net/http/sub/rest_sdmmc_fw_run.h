#ifndef REST_SDMMC_FW_RUN_H
#define REST_SDMMC_FW_RUN_H

#include <esp_err.h>
#include <esp_http_server.h>

esp_err_t sdmmc_fw_run_post_handler(httpd_req_t *req);

#endif
