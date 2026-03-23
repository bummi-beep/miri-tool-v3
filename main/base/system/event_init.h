#ifndef EVENT_INIT_H
#define EVENT_INIT_H

/*
 * event_init()
 * - Creates the default ESP-IDF event loop.
 * - Keep it in the boot baseline so other modules can register handlers
 *   without worrying about initialization order.
 */
void event_init(void);

#endif
