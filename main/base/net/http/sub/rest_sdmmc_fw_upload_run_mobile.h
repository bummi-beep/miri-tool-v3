#ifndef REST_SDMMC_FW_UPLOAD_RUN_MOBILE_H
#define REST_SDMMC_FW_UPLOAD_RUN_MOBILE_H

#include <esp_err.h>
#include <esp_http_server.h>

esp_err_t sdmmc_fw_upload_run_post_handler(httpd_req_t *req);

#endif
