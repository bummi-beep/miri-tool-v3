#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include <stddef.h>
#include <stdbool.h>

void tcp_server_start(void);
void tcp_server_set_pending_file(const char *name, const char *type);
void tcp_server_set_expected_size(size_t size);
bool tcp_server_has_pending(void);

#endif
