#ifndef REST_COMMON_FORMAT_H
#define REST_COMMON_FORMAT_H

#include <esp_err.h>
#include <esp_http_server.h>

esp_err_t format_sdmmc_get_handler(httpd_req_t *req);
esp_err_t format_fat_get_handler(httpd_req_t *req);

#endif
