#include "array.h"
#include "cmd.h"
#include "common.h"
#include "log.h"
#include "event.h"
#include "dispatcher.h"
#include "client.h"
#include "monitor.h"
#include "set.h"
#include "unix.h"
#include "wm.h"
#include "xrandr.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <sys/epoll.h>

#define CMD_SOCKET_PATH "/tmp/tatami.sock"

struct client_data {
	Window window;
	int mapped;
};

struct map_request_data {
	Window window;
	XWindowAttributes attrs;
};

struct wm {
	Display *display;
	unsigned screen;
	Window root;

	monitor_t focused_monitor;

	struct dispatcher *dispatcher;

	struct eventq eventq;

	struct xrandr *xrandr;
	int cmdsock;
};

static struct wm _wm;

static int _xevent_to_configure_request(XConfigureRequestEvent *xevent, struct event *event);
static int _xevent_to_configure_notify (XConfigureEvent *xevent,        struct event *event);
static int _xevent_to_destroy_notify   (XDestroyWindowEvent *xevent,    struct event *event);
static int _xevent_to_enter_notify     (XCrossingEvent *xevent,         struct event *event);
static int _xevent_to_leave_notify     (XCrossingEvent *xevent,         struct event *event);
static int _xevent_to_expose           (XExposeEvent *xevent,           struct event *event);
static int _xevent_to_focus_in         (XFocusInEvent *xevent,          struct event *event);
static int _xevent_to_focus_out        (XFocusOutEvent *xevent,         struct event *event);
static int _xevent_to_mapping_notify   (XMappingEvent *xevent,          struct event *event);
static int _xevent_to_map_request      (XMapRequestEvent *xevent,       struct event *event);
static int _xevent_to_motion_notify    (XMotionEvent *xevent,           struct event *event);
static int _xevent_to_property_notify  (XPropertyEvent *xevent,         struct event *event);
static int _xevent_to_unmap_notify     (XUnmapEvent *xevent,            struct event *event);

static int _event_configure_request_handler(struct event *event);
static int _event_configure_notify_handler (struct event *event);
static int _event_destroy_notify_handler   (struct event *event);
static int _event_enter_notify_handler     (struct event *event);
static int _event_leave_notify_handler     (struct event *event);
static int _event_expose_handler           (struct event *event);
static int _event_focus_in_handler         (struct event *event);
static int _event_focus_out_handler        (struct event *event);
static int _event_mapping_notify_handler   (struct event *event);
static int _event_map_request_handler      (struct event *event);
static int _event_motion_notify_handler    (struct event *event);
static int _event_property_notify_handler  (struct event *event);
static int _event_unmap_notify_handler     (struct event *event);

static int xerror_nop    (Display *display, XErrorEvent *event);
static int xerror_handler(Display *display, XErrorEvent *event);

typedef int (event_conv_t)(XEvent*, struct event*);
typedef int (event_handler_t)(struct event*);

static const char *_xevent_names[] = {
	[ConfigureRequest] = "ConfigureRequest",
	[ConfigureNotify]  = "ConfigureNotify",
	[DestroyNotify]    = "DestroyNotify",
	[EnterNotify]      = "EnterNotify",
	[LeaveNotify]      = "LeaveNotify",
	[Expose]           = "Expose",
	[FocusIn]          = "FocusIn",
	[FocusOut]         = "FocusOut",
	[MappingNotify]    = "MappingNotify",
	[MapRequest]       = "MapRequest",
	[MotionNotify]     = "MotionNotify",
	[PropertyNotify]   = "PropertyNotify",
	[UnmapNotify]      = "UnmapNotify",
};

static const struct {
	event_type_t type;
	event_conv_t *conv;
} _xevent_converters[] = {
	[ConfigureRequest] = { EVENT_CONFIGURE_REQUEST, (event_conv_t*)_xevent_to_configure_request },
	[ConfigureNotify]  = { EVENT_CONFIGURE_NOTIFY,  (event_conv_t*)_xevent_to_configure_notify  },
	[DestroyNotify]    = { EVENT_DESTROY_NOTIFY,    (event_conv_t*)_xevent_to_destroy_notify    },
	[EnterNotify]      = { EVENT_ENTER_NOTIFY,      (event_conv_t*)_xevent_to_enter_notify      },
	[LeaveNotify]      = { EVENT_LEAVE_NOTIFY,      (event_conv_t*)_xevent_to_leave_notify      },
	[Expose]           = { EVENT_EXPOSE,            (event_conv_t*)_xevent_to_expose            },
	[FocusIn]          = { EVENT_FOCUS_IN,          (event_conv_t*)_xevent_to_focus_in          },
	[FocusOut]         = { EVENT_FOCUS_OUT,         (event_conv_t*)_xevent_to_focus_out         },
	[MappingNotify]    = { EVENT_MAPPING_NOTIFY,    (event_conv_t*)_xevent_to_mapping_notify    },
	[MapRequest]       = { EVENT_MAP_REQUEST,       (event_conv_t*)_xevent_to_map_request       },
	[MotionNotify]     = { EVENT_MOTION_NOTIFY,     (event_conv_t*)_xevent_to_motion_notify     },
	[PropertyNotify]   = { EVENT_PROPERTY_NOTIFY,   (event_conv_t*)_xevent_to_property_notify   },
	[UnmapNotify]      = { EVENT_UNMAP_NOTIFY,      (event_conv_t*)_xevent_to_unmap_notify      },
};

static event_handler_t * const _event_handlers[] = {
	[EVENT_CONFIGURE_REQUEST] = _event_configure_request_handler,
	[EVENT_CONFIGURE_NOTIFY]  = _event_configure_notify_handler,
	[EVENT_DESTROY_NOTIFY]    = _event_destroy_notify_handler,
	[EVENT_ENTER_NOTIFY]      = _event_enter_notify_handler,
	[EVENT_LEAVE_NOTIFY]      = _event_leave_notify_handler,
	[EVENT_EXPOSE]            = _event_expose_handler,
	[EVENT_FOCUS_IN]          = _event_focus_in_handler,
	[EVENT_FOCUS_OUT]         = _event_focus_out_handler,
	[EVENT_MAPPING_NOTIFY]    = _event_mapping_notify_handler,
	[EVENT_MAP_REQUEST]       = _event_map_request_handler,
	[EVENT_MOTION_NOTIFY]     = _event_motion_notify_handler,
	[EVENT_PROPERTY_NOTIFY]   = _event_property_notify_handler,
	[EVENT_UNMAP_NOTIFY]      = _event_unmap_notify_handler,
};

static int _client_cmp_window(const client_t cid, struct client_data *data, Window *window)
{
	return data->window == *window ? 0 : 1;
}

static client_t client_of_window(Window window)
{
	return client_search((int(*)(const client_t, void*, void*))_client_cmp_window, &window);
}

static int xevent_to_event(XEvent *xevent, struct event **dst)
{
	int err;
	event_type_t type;
	struct event *event;

	event = NULL;

	if (xevent->type < SIZEOF_ARRAY(_xevent_converters)) {
		if (_xevent_converters[xevent->type].conv) {
			log_debug("PRE", "Converting %s event\n", _xevent_names[xevent->type]);

			type = _xevent_converters[xevent->type].type;
			err = event_new(&event, type);

			if (!err) {
				err = _xevent_converters[xevent->type].conv(xevent, event);
			}
		} else {
			err = -ENOSYS;
		}
	} else {
		err = xrandr_event_convert(_wm.xrandr, xevent, &event);
	}

	if (!err) {
		*dst = event;
	} else {
		log_debug("PRE", "Could not convert event of type 0x%x\n", xevent->type);

		if (event) {
			event_free(&event);
		}
	}

	return err;
}

static int _xevent_to_configure_request(XConfigureRequestEvent *xevent, struct event *event)
{
	int err;

	err = 0;

	event->data.configure_request.client = client_of_window(xevent->window);
	event->data.configure_request.geom.x = xevent->x;
	event->data.configure_request.geom.y = xevent->y;
	event->data.configure_request.geom.w = xevent->width;
	event->data.configure_request.geom.h = xevent->height;

	if (!CLIENT_VALID(event->data.configure_request.client)) {
		XConfigureRequestEvent *copy;

		if (!(copy = calloc(1, sizeof(*copy)))) {
			err = -ENOMEM;
		} else {
			memcpy(copy, xevent, sizeof(*copy));
			event->extra = copy;
		}
	}

	return err;
}

static int _xevent_to_configure_notify(XConfigureEvent *xevent, struct event *event)
{
	int err;

	err = 0;
	event->data.configure_notify.client = client_of_window(xevent->window);

	if (CLIENT_VALID(event->data.configure_notify.client)) {
		event->data.configure_notify.geom.x = xevent->x;
		event->data.configure_notify.geom.y = xevent->y;
		event->data.configure_notify.geom.w = xevent->width;
		event->data.configure_notify.geom.h = xevent->height;
	} else {
		/* not a managed top-level window / nothing to do */
		err = -ENOENT;
	}

	return err;
}

static int _xevent_to_destroy_notify(XDestroyWindowEvent *xevent, struct event *event)
{
	event->data.destroy_notify.client = client_of_window(xevent->window);

	return CLIENT_VALID(event->data.destroy_notify.client) ? 0 : -ENOENT;
}

static int _xevent_to_enter_notify(XCrossingEvent *xevent, struct event *event)
{
	event->data.enter_notify.mode    = xevent->mode;
	event->data.enter_notify.detail  = xevent->detail;
	event->data.enter_notify.client  = client_of_window(xevent->window);
	event->data.enter_notify.monitor = monitor_at_xy(xevent->x_root, xevent->y_root);

	return CLIENT_VALID(event->data.enter_notify.client) ? 0 : -ENOENT;
}

static int _xevent_to_leave_notify(XCrossingEvent *xevent, struct event *event)
{
	event->data.leave_notify.mode    = xevent->mode;
	event->data.leave_notify.detail  = xevent->detail;
	event->data.leave_notify.client  = client_of_window(xevent->window);
	event->data.leave_notify.monitor = monitor_at_xy(xevent->x_root, xevent->y_root);

	return CLIENT_VALID(event->data.leave_notify.client) ? 0 : -ENOENT;
}

static int _xevent_to_expose(XExposeEvent *xevent, struct event *event)
{
	event->data.expose.count   = xevent->count;
	event->data.expose.client  = client_of_window(xevent->window);

	return CLIENT_VALID(event->data.expose.client) ? 0 : -ENOENT;
}

static int _xevent_to_focus_in(XFocusInEvent *xevent, struct event *event)
{
	event->data.focus_in.client = client_of_window(xevent->window);

	return CLIENT_VALID(event->data.focus_in.client) ? 0 : -ENOENT;
}

static int _xevent_to_focus_out(XFocusOutEvent *xevent, struct event *event)
{
	event->data.focus_out.client = client_of_window(xevent->window);

	return CLIENT_VALID(event->data.focus_out.client) ? 0 : -ENOENT;
}

static int _xevent_to_map_request(XMapRequestEvent *xevent, struct event *event)
{
	struct map_request_data *data;
	int err;

	err = 0;
	event->data.map_request.client = client_of_window(xevent->window);

	if (!(data = calloc(1, sizeof(*data)))) {
		err = -ENOMEM;
	} else if (!XGetWindowAttributes(_wm.display, xevent->window, &data->attrs)) {
		free(data);
		data = NULL;
		err = -EIO;
	} else {
		data->window = xevent->window;
		event->extra = data;
	}

	return err;
}

static int _xevent_to_mapping_notify(XMappingEvent *xevent, struct event *event)
{
	XMappingEvent *copy;

	if (!(copy = malloc(sizeof(*copy)))) {
		return -ENOMEM;
	}

	memcpy(copy, xevent, sizeof(*copy));
	event->extra = copy;

	/* FIXME: We're not handling keyboard inputs, so we don't need this? */

	return 0;
}

static int _xevent_to_motion_notify(XMotionEvent *xevent, struct event *event)
{
	event->data.motion_notify.pointer.x = xevent->x_root;
	event->data.motion_notify.pointer.y = xevent->y_root;
	event->data.motion_notify.pointer.w = 1;
	event->data.motion_notify.pointer.h = 1;

	event->data.motion_notify.client = client_of_window(xevent->window);
	event->data.motion_notify.monitor = monitor_at_xy(xevent->x_root, xevent->y_root);

	return (event->data.motion_notify.client < 0 &&
	        event->data.motion_notify.monitor < 0) ? -ENOENT : 0;
}

static int _xevent_to_property_notify(XPropertyEvent *xevent, struct event *event)
{
	XPropertyEvent *copy;
	int err;

	err = 0;
	event->data.property_notify.client = client_of_window(xevent->window);

	if (CLIENT_VALID(event->data.property_notify.client)) {
		err = -ENOENT;
	} else if (!(copy = malloc(sizeof(*copy)))) {
		err = -ENOMEM;
	} else {
		memcpy(copy, xevent, sizeof(*copy));
		event->extra = copy;
	}

	return err;
}

static int _xevent_to_unmap_notify(XUnmapEvent *xevent, struct event *event)
{
	event->data.unmap_notify.client = client_of_window(xevent->window);

	return CLIENT_VALID(event->data.unmap_notify.client) ? 0 : -ENOENT;
}

static int set_client_state(const client_t client, const long state)
{
	static long atom = -1;
	struct client_data *data;
	long prop[2];
	int err;

	if ((err = client_get_data(client, (void**)&data)) < 0) {
		return err;
	}

	if (!data) {
		return -EBADF;
	}

	if (atom < 0) {
		atom = XInternAtom(_wm.display, "WM_STATE", False);
	}

	prop[0] = state;
	prop[1] = None;

	XChangeProperty(_wm.display, data->window, atom, atom, 32,
			PropModeReplace, (unsigned char*)prop, 2);

	return 0;
}

static int _event_configure_request_handler(struct event *event)
{
	if (!CLIENT_VALID(event->data.configure_request.client)) {
		/* the client is not handled by us, so we will allow the request */
		XConfigureRequestEvent *request = (XConfigureRequestEvent*)event->extra;
		XWindowChanges changes;
		unsigned int value_mask;

		changes.x = request->x;
		changes.y = request->y;
		changes.width = request->width;
		changes.height = request->height;
		changes.border_width = 0;
		changes.sibling = request->above;
		changes.stack_mode = request->detail;
		value_mask = request->value_mask | CWBorderWidth;

		XConfigureWindow(_wm.display, request->window, value_mask, &changes);
	} else {
		/*
		 * The client is handled by us; clients don't get to choose their size,
		 * so the request is ignored.
		 */
		set_client_state(event->data.configure_request.client, NormalState);
	}

	XSync(_wm.display, False);

	return 0;
}

static int _event_configure_notify_handler(struct event *event)
{
	/*
	 * This event is received after a client has been reconfigured
	 * (size/position change)
	 */

	if (CLIENT_VALID(event->data.configure_notify.client)) {
		client_set_geometry(event->data.configure_notify.client,
				    event->data.configure_notify.geom);
		client_notify(event->data.configure_notify.client,
			      CLIENT_EVENT_GEOMETRY_CHANGED,
			      &event->data.configure_notify.geom);
	}

	/* root window changes are handled via XRandR */

	return 0;
}

static int _event_destroy_notify_handler(struct event *event)
{
	client_t client;

	/* This event is received after a client has been destroyed */

	client = event->data.destroy_notify.client;

	if (CLIENT_VALID(client)) {
		client_notify(client, CLIENT_EVENT_DETACHED, NULL);
		client_free(client);
	}

	return 0;
}

static int _event_enter_notify_handler(struct event *event)
{
	/* The pointer has been moved over the client in this event */

	if (CLIENT_VALID(event->data.enter_notify.client)) {
		client_notify(event->data.enter_notify.client,
		              CLIENT_EVENT_FOCUS_GAINED, NULL);
	}

	return 0;
}

static int _event_leave_notify_handler(struct event *event)
{
	/*
	 * The pointer has left a client. Usually we don't have to do
	 * anything here, but there is a chance that the pointer has
	 * left a client without entering a new one (e.g. the pointer
	 * has entered the root window).
	 */

	if (CLIENT_VALID(event->data.leave_notify.client)) {
		client_notify(event->data.leave_notify.client,
		              CLIENT_EVENT_FOCUS_LOST, NULL);
	}

	return 0;
}

static int _event_expose_handler(struct event *event)
{
	/* We probably don't need to handle this */
	return -ENOSYS;
}

static int _event_focus_in_handler(struct event *event)
{
	/* TODO: Make this the new focused client */

	if (CLIENT_VALID(event->data.focus_in.client)) {
		/* TODO: set_current_client(event->data.focus_in.client); */
	}

	return 0;
}

static int _event_focus_out_handler(struct event *event)
{
	if (CLIENT_VALID(event->data.focus_out.client)) {
		client_notify(event->data.focus_out.client,
		              CLIENT_EVENT_FOCUS_LOST, NULL);
	}

	return 0;
}

static int _event_mapping_notify_handler(struct event *event)
{
	/*
	 * We don't handle keyboard events, so we're not interested in
	 * keyboard layout changes
	 */
	return 0;
}

static void _client_detached(const client_t cid, void *data, void *context)
{
	workspace_t workspace;

	workspace = client_get_workspace(cid);

	if (WORKSPACE_VALID(workspace)) {
		workspace_detach_client(workspace, cid);
	}
}

static void _client_geometry_changed(const client_t cid, void *data,
				     void *context)
{
	/* TODO: Scale the saved pointer */
}

static void _client_pointer_changed(const client_t cid, void *data,
				    void *context)
{
	/* TODO: Nothing to do? */
}

static void _client_focus_gained(const client_t cid, void *data,
				 void *context)
{
	struct client_data *cdata;

	if (client_get_data(cid, (void**)&cdata) < 0 || !cdata) {
		return;
	}

	XSetInputFocus(_wm.display, cdata->window, RevertToPointerRoot, CurrentTime);
}

static void _client_focus_lost(const client_t cid, void *data,
			       void *context)
{

}

static void _client_shown(const client_t cid, void *data, void *context)
{

}

static void _client_hidden(const client_t cid, void *data, void *context)
{

}

monitor_t wm_get_focused_monitor(void)
{
	return _wm.focused_monitor;
}

static void _client_attached(const client_t cid, void *data, void *context)
{
	monitor_t focused_monitor;
	workspace_t focused_workspace;
	int err;

	client_call_t * const callbacks[CLIENT_EVENT_LAST] = {
		_client_attached,
		_client_detached,
		_client_geometry_changed,
		_client_pointer_changed,
		_client_focus_gained,
		_client_focus_lost,
		_client_shown,
		_client_hidden
	};
	client_event_t event;

	for (event = CLIENT_EVENT_ATTACHED; event < CLIENT_EVENT_LAST; event++) {
		client_set_callback(cid, event, callbacks[event], data);
	}

	focused_monitor = wm_get_focused_monitor();
	focused_workspace = monitor_get_workspace(focused_monitor);

	if (!WORKSPACE_VALID(focused_workspace)) {
		log_error("WM", "No workspace to attach client %ld to", cid);
		return;
	}

	if ((err = workspace_attach_client(focused_workspace, cid)) < 0) {
		log_error("WM", "workspace_attach_client: %s", strerror(-err));
	}
}

static int attach_client(Window window)
{
	client_t client;
	struct client_data *data;
	int err;

	if (!(data = calloc(1, sizeof(*data)))) {
		return -ENOMEM;
	}

	err = 0;
	data->window = window;

	if ((client = client_new()) < 0) {
		err = (int)client;
	} else {
		client_set_data(client, data);
		client_set_callback(client, CLIENT_EVENT_ATTACHED,
		                    (client_call_t*)_client_attached, NULL);
		client_notify(client, CLIENT_EVENT_ATTACHED, NULL);
	}

	if (err) {
		free(data);
		client_free(client);
	}

	return 0;
}

static int _event_map_request_handler(struct event *event)
{
	struct map_request_data *data;
	int err;

	err = 0;

	if (!CLIENT_VALID(event->data.map_request.client)) {
		data = (struct map_request_data*)event->extra;

		if (!data->attrs.override_redirect) {
			log_info("WM", "Attaching client for window 0x%lx\n", data->window);
			err = attach_client(data->window);
		}
	}

	return err;
}

static int _event_motion_notify_handler(struct event *event)
{
	/*
	 * If the client is handled by the us, update the last
	 * known pointer position.
	 */

	if (CLIENT_VALID(event->data.motion_notify.client)) {
		client_set_pointer(event->data.motion_notify.client,
				   event->data.motion_notify.pointer);
		client_notify(event->data.motion_notify.client,
			      CLIENT_EVENT_POINTER_CHANGED,
			      &event->data.motion_notify.pointer);
	}

	return 0;
}

static int _event_property_notify_handler(struct event *event)
{
	/* TODO: Detect what property changed and react appropriately */
	return -ENOSYS;
}

static int _event_unmap_notify_handler(struct event *event)
{
	client_t client;

	client = event->data.unmap_notify.client;

	if (CLIENT_VALID(client)) {
		log_info("WM", "Detaching client %ld", client);

		XGrabServer(_wm.display);
		XSync(_wm.display, False);

		XSetErrorHandler(xerror_nop);

		set_client_state(client, WithdrawnState);

		if (!event->data.unmap_notify.send_event) {
			client_notify(client, CLIENT_EVENT_DETACHED, NULL);
			client_free(client);
		}

		XSync(_wm.display, False);
		XSetErrorHandler(xerror_handler);
		XUngrabServer(_wm.display);
	}

	return 0;
}

void cmdsock_read(int fd, dispatcher_event_t events, void *data)
{
	struct cmd cmd;
	int rxbytes;
	int err;

	rxbytes = unix_socket_read(fd, (unsigned char*)&cmd, sizeof(cmd));

	if (rxbytes < 0) {
		err = -errno;
		log_error("CMD", "Could not read command from client %d: %s", fd, strerror(-err));
	} else if (rxbytes == 0) {
		log_info("CMD", "Connection %d closed", fd);
		unix_socket_close(fd);
	} else if (rxbytes != sizeof(cmd)) {
		log_error("CMD", "Received command from %d with invalid length %d (expected %d)",
		          fd, rxbytes, sizeof(cmd));
	} else {
		log_info("CMD", "Received command %d from %d\n", cmd.opcode, fd);
	}
}

void cmdsock_accept(int fd, dispatcher_event_t events, void *data)
{
	struct wm *wm;
	int client;
	int err;

	wm = (struct wm*)data;

	if ((client = unix_socket_accept(fd)) < 0) {
		log_error("CMD", "Could not accept connection: %s", strerror(-client));
	} else {
		err = dispatcher_watch_fd(wm->dispatcher, client, cmdsock_read, wm);

		if (err < 0) {
			log_error("CMD", "Could not add client %d to dispatcher: %s",
			          client, strerror(-err));

			close(client);
		}
	}
}

/*
 * enqueue_xevents(): Receive and convert XEvents, and enqueue them into the event queue
 */
void enqueue_xevents(int fd, dispatcher_event_t events, void *data)
{
	struct wm *wm;

	wm = (struct wm*)data;

	while (XEventsQueued(wm->display, QueuedAfterFlush) > 0) {
		struct event *event;
		XEvent xev;
		int err;

		if (XNextEvent(wm->display, &xev) != 0) {
			continue;
		}

		if ((err = xevent_to_event(&xev, &event)) < 0) {
			if (err != -ENOSYS) {
				log_error("PRE", "Could not convert event: %s\n", strerror(-err));
			}

			continue;
		}

		if ((err = eventq_nq(&wm->eventq, event)) < 0) {
			log_error("PRE", "Could not enqueue event: %s\n", strerror(-err));
			event_free(&event);
			continue;
		}
	}
}

static int xerror_startup(Display *display, XErrorEvent *event)
{
	log_critical("WM", "Error during initialization. Is another WM running?");
	exit(1);

	return -1;
}

static int xerror_nop(Display *display, XErrorEvent *event)
{
	log_info("WM", "Ignoring XErrorEvent %p\n", (void*)event);
	return 0;
}

static int xerror_handler(Display *display, XErrorEvent *event)
{
	/* TODO: Print error to stderr */
	return 0;
}

static void _monitor_detached(const monitor_t mid, struct wm *wm)
{
	/* TODO: Do something */
}

static void _monitor_attached(const monitor_t mid, struct wm *wm)
{
	workspace_t unviewed;

	monitor_set_callback(mid, MONITOR_EVENT_DETACHED,
	                     (monitor_call_t*)_monitor_detached, wm);

	unviewed = workspace_get_unviewed();
	if (!WORKSPACE_VALID(unviewed)) {
		unviewed = workspace_new();

		if (!WORKSPACE_VALID(unviewed)) {
			log_error("WM", "Could not find a workspace for monitor %ld", mid);
			return;
		}
	}

	monitor_set_workspace(mid, unviewed);

	if (!MONITOR_VALID(wm->focused_monitor)) {
		/* FIXME: set focused monitor */
		wm->focused_monitor = mid;
	}
}

static int get_nonoverlapping_regions(const monitor_t monitor, struct geom ***regions)
{
	struct geom monitor_geom;
	int num_regions;
	int err;

	if (!regions || !*regions) {
		return -EINVAL;
	}

	err = monitor_get_geometry(monitor, &monitor_geom);

	if (err < 0) {
		return err;
	}

	/*
	 * When this function is called for the first monitor, `regions' contains
	 * the geometry of the new CRTC. This function takes the region from the
	 * head of the `regions' array and computes the sub-regions that do not
	 * overlap with the current monitor using `geom_difference()'. If the
	 * regions do not overlap, the array returned by this function contains
	 * only the original region, otherwise it contains the non-overlapping
	 * sub-regions. We add all regions that were computed to the `regions'
	 * array and do not check those regions again in this call.
	 */

	/* Check only the regions that were already in the array */
	num_regions = array_len((void**)*regions);
	while (num_regions > 0) {
		struct geom region;
		struct geom **nonoverlapping;
		int num_nonoverlapping;

		region = *(*regions)[0];
		array_remove((void***)regions, 0, ARRAY_GENERIC_FREE);
		num_regions--;

		log_info("WM", "Determining overlap: %dx%d@%dx%d vs %dx%d@%dx%d\n",
		         region.w, region.h, region.x, region.y,
		         monitor_geom.w, monitor_geom.h, monitor_geom.x, monitor_geom.y);

		err = geom_difference(region, monitor_geom, &nonoverlapping);
		if (err < 0) {
			return err;
		}

		num_nonoverlapping = array_len((void**)nonoverlapping);
		err = array_add((void***)regions, (void*)nonoverlapping, num_nonoverlapping);

		array_free((void***)&nonoverlapping, ARRAY_DONT_FREE);
	}

	return err;
}

static int determine_geometries_for_crtc(const struct geom crtc, struct geom ***result)
{
	struct geom **geoms;
	struct geom *dupe;
	int err;

	if (!result) {
		return -EINVAL;
	}

	geoms = NULL;

	log_info("WM", "Determining geometries for CRTC with geom %dx%d@%dx%d\n",
	         crtc.w, crtc.h, crtc.x, crtc.y);

	err = geom_dupe(&dupe, crtc);
	if (err < 0) {
		return err;
	}

	err = array_add((void***)&geoms, (void**)&dupe, 1);
	if (err < 0) {
		free(dupe);
		return err;
	}

	err = monitor_foreach((int(*)(const monitor_t, void*))get_nonoverlapping_regions,
	                      &geoms);
	log_info("WM", "monitor_foreach() returned %d\n", err);

	if (!err) {
		*result = geoms;
	} else {
		array_free((void***)&geoms, ARRAY_GENERIC_FREE);
	}

	return err;
}

static void monitor_attached_handler(const xrandr_crtc_t crtc,
                                     const struct geom *geom,
                                     struct wm *wm)
{
	struct geom **regions;
	int err;

	log_info("WM", "CRTC 0x%lx was attached\n", crtc);

	/*
	 * The connected CRTC may overlap with other monitors. Say, we have
	 * the following situation, where the rectangle on the right is the
	 * new CRTC.
	 *
	 *  (first monitor)
	 *  +-----------+
	 *  |           |
	 *  |      +----+-----+ (new CRTC)
	 *  |      |    |     |
	 *  +------+----+     |
	 *         |          |
	 *         +----------+
	 *
	 * Becasue the CRTC overlaps with the first monitor, creating a
	 * monitor with the exact geometry of the CRTC would cause windows
	 * displayed on the two monitors to overlap. Thus, the overlapping
	 * part of the new CRTC is not allocated. Instead two monitors are
	 * allocated for the non-overlapping region of the new CRTC.
	 *
	 *  (first monitor)
	 *  +-----------+
	 *  |           |
	 *  |      +----+-----+ (new CRTC)
	 *  |      |    |  A  |
	 *  +------+----+-----+
	 *         |    B     |
	 *         +----------+
	 *
	 * If multiple CRTCs overlap, this may cause even more, smaller
	 * monitors to be allocated.
	 */

	err = determine_geometries_for_crtc(*geom, &regions);

	if (!err) {
		int num_regions;
		int i;

		num_regions = array_len((void**)regions);
		log_info("WM", "Allocating %d monitors for CRTC 0x%lx\n", num_regions, crtc);

		for (i = 0; i < num_regions; i++) {
			monitor_t monitor;

			log_info("WM", "Allocating monitor with geom %dx%d@%dx%d\n",
			         regions[i]->w, regions[i]->h, regions[i]->x, regions[i]->y);

			if ((monitor = monitor_new(crtc, *regions[i])) < 0) {
				log_error("WM", "Could not create monitor for CRTC 0x%lx: %s\n",
				          crtc, strerror(-monitor));
				break;
			}

			monitor_set_callback(monitor, MONITOR_EVENT_ATTACHED,
			                     (monitor_call_t*)_monitor_attached, wm);
			monitor_notify(monitor, MONITOR_EVENT_ATTACHED, NULL);
		}

		array_free((void***)&regions, ARRAY_GENERIC_FREE);
	}
}

static void monitor_detached_handler(const xrandr_crtc_t crtc,
                              const struct geom *geom,
                              struct wm *wm)
{
	monitor_t monitor;

	log_info("WM", "CRTC 0x%lx was detached\n", crtc);

	if ((monitor = monitor_of_crtc(crtc)) < 0) {
		log_error("WM", "Monitor of CRTC 0x%lx is not attached: %s\n",
		          crtc, strerror(-monitor));
		return;
	}

	/* TODO: Detach monitor */
	monitor_free(monitor);
}

static void monitor_geometry_changed_handler(const xrandr_crtc_t crtc,
                                             const struct geom *geom,
                                             struct wm *wm)
{
	monitor_t monitor;

	log_info("WM", "CRTC 0x%lx was resized\n", crtc);

	if ((monitor = monitor_of_crtc(crtc)) < 0) {
		log_error("WM", "Could not find monitor of CRTC 0x%lx: %s\n",
		          crtc, strerror(-monitor));
		return;
	}

	log_info("WM", "CRTC 0x%lx changed geometry to [%dx%d @%d,%d]\n",
	         crtc, geom->w, geom->h, geom->x, geom->y);
	monitor_set_geometry(monitor, *geom);
}

int wm_init(void)
{
	int err;

	memset(&_wm, 0, sizeof(_wm));

	_wm.display = XOpenDisplay(NULL);
	if (!_wm.display) {
		return -EIO;
	}

	_wm.screen = DefaultScreen(_wm.display);
	_wm.root = RootWindow(_wm.display, _wm.screen);
	_wm.focused_monitor = (monitor_t)-1;

	if ((err = xrandr_new(&_wm.xrandr, _wm.display, _wm.root)) < 0) {
		return err;
	}

	XSetErrorHandler(xerror_startup);
	XSync(_wm.display, False);

	err = dispatcher_new(&_wm.dispatcher);
	if (err < 0) {
		/* FIXME: Clean up */
	}

	if ((_wm.cmdsock = unix_server_new(CMD_SOCKET_PATH)) < 0) {
		return _wm.cmdsock;
	}

	err = dispatcher_watch_fd(_wm.dispatcher, _wm.cmdsock, cmdsock_accept, &_wm);

	err = dispatcher_watch_fd(_wm.dispatcher, ConnectionNumber(_wm.display),
	                          enqueue_xevents, &_wm);

	XSelectInput(_wm.display, _wm.root,
	             SubstructureRedirectMask |
	             SubstructureNotifyMask |
	             PointerMotionMask |
	             EnterWindowMask |
	             LeaveWindowMask |
	             StructureNotifyMask |
	             PropertyChangeMask);

	XSync(_wm.display, False);

	XSetErrorHandler(xerror_handler);
	XSync(_wm.display, False);

	xrandr_set_callback(_wm.xrandr, XRANDR_EVENT_MONITOR_ATTACHED,
	                    (xrandr_call_t*)monitor_attached_handler, &_wm);
	xrandr_set_callback(_wm.xrandr, XRANDR_EVENT_MONITOR_DETACHED,
	                    (xrandr_call_t*)monitor_detached_handler, &_wm);
	xrandr_set_callback(_wm.xrandr, XRANDR_EVENT_MONITOR_GEOMETRY_CHANGED,
	                    (xrandr_call_t*)monitor_geometry_changed_handler, &_wm);

	xrandr_init(_wm.xrandr);

	return err;
}

int wm_run(void)
{
	int err;

	err = 0;

	while (1) {
		struct event *event;

		/* handle all events that have arrived since the last iteration */
		err = dispatcher_run(_wm.dispatcher);

		while (eventq_dq(&_wm.eventq, &event) >= 0) {
			if (event->type < SIZEOF_ARRAY(_event_handlers)) {
				_event_handlers[event->type](event);
			} else {
				xrandr_handle_event(_wm.xrandr, event);
			}

			event_free(&event);
		}

		/* TODO: Redraw what needs to be redrawn */
	}

	return err;
}

int wm_move_client(const client_t client, const struct geom pos)
{
	struct client_data *data;
	int err;

	if ((err = client_get_data(client, (void**)&data)) < 0) {
		return err;
	}

	XMoveResizeWindow(_wm.display, data->window, pos.x, pos.y, pos.w, pos.h);

	if (!data->mapped) {
		XMapWindow(_wm.display, data->window);
		data->mapped = 1;
	}

	return 0;
}

int wm_show_client(const client_t client)
{
	struct client_data *data;
	int err;

	if ((err = client_get_data(client, (void**)&data)) < 0) {
		return err;
	}

	XMapWindow(_wm.display, data->window);
	return 0;
}

int wm_hide_client(const client_t client)
{
	struct geom client_geom;
	int err;

	if ((err = client_get_geometry(client, &client_geom)) < 0) {
		return err;
	}

	/* Move the client outside of the visible area */
	client_geom.x = -2 * client_geom.w;
	client_geom.y = -2 * client_geom.h;

	return wm_move_client(client, client_geom);
}
