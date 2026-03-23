#ifndef REST_SDMMC_FW_UPLOAD_RAW_H
#define REST_SDMMC_FW_UPLOAD_RAW_H

#include <esp_http_server.h>

esp_err_t sdmmc_fw_upload_raw_post_handler(httpd_req_t *req);

#endif
