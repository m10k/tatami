#ifndef XRANDR_H
#define XRANDR_H

#include "event.h"
#include <X11/Xlib.h>

typedef enum {
	XRANDR_EVENT_MONITOR_ATTACHED = 0,
	XRANDR_EVENT_MONITOR_DETACHED,
	XRANDR_EVENT_MONITOR_GEOMETRY_CHANGED,
	XRANDR_EVENT_LAST
} xrandr_event_t;

typedef XID xrandr_output_t;
typedef XID xrandr_crtc_t;

typedef void (xrandr_call_t)(const xrandr_crtc_t, const struct geom*, void*);

struct xrandr;

int xrandr_new(struct xrandr **xrandr,
               Display *display,
               Window root);

int xrandr_set_callback(struct xrandr *xrandr,
                        xrandr_event_t event,
                        xrandr_call_t *callback,
                        void *data);

int xrandr_init(struct xrandr *xrandr);
int xrandr_handle_event(struct xrandr *xrandr, struct event *event);
int xrandr_event_convert(struct xrandr *xrandr, XEvent *xevent, struct event **event);

#endif /* XRANDR_H */
