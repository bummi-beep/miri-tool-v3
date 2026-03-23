#ifndef REST_FAT_GET_LIST_H
#define REST_FAT_GET_LIST_H

#include <esp_err.h>
#include <esp_http_server.h>

esp_err_t fat_get_list_get_handler(httpd_req_t *req);

#endif
