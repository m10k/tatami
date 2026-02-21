#ifndef WM_H
#define WM_H

#include "common.h"

int wm_init(void);
int wm_run(void);

monitor_t wm_get_focused_monitor(void);

int wm_move_client(const client_t client, const struct geom pos);
int wm_show_client(const client_t client);
int wm_hide_client(const client_t client);

#endif /* WM_H */
