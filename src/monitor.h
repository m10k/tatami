#ifndef MONITOR_H
#define MONITOR_H

#include "common.h"

typedef enum {
	MONITOR_EVENT_ATTACHED = 0,
	MONITOR_EVENT_DETACHED,
	MONITOR_EVENT_LAST
} monitor_event_t;

typedef unsigned crtc_t;

typedef void (monitor_call_t)(const monitor_t, void*, void*);

#define MONITOR_VALID(m) ((m) >= 0)

monitor_t monitor_new(const crtc_t crtc, const struct geom geom);
int monitor_free(const monitor_t mid);

int monitor_set_geometry(const monitor_t mod, const struct geom geom);
int monitor_get_geometry(const monitor_t mid, struct geom *geom);

workspace_t monitor_get_workspace(const monitor_t monitor);

monitor_t monitor_search(int (*cmp)(const monitor_t, void*, void*), void *data);
monitor_t monitor_of_crtc(const crtc_t crtc);
monitor_t monitor_at(const struct geom pos);
monitor_t monitor_at_xy(const int x, const int y);

int monitor_set_callback(const monitor_t mid,
                         const monitor_event_t event,
                         monitor_call_t *func,
                         void *data);
int monitor_notify(const monitor_t mid,
                   const monitor_event_t event,
                   void *context);
int monitor_foreach(int (*func)(const monitor_t, void*), void *context);

#endif /* MONITOR_H */
