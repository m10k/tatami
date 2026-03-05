#include "common.h"
#include "layout.h"
#include "monitor.h"
#include "workspace.h"
#include "set.h"
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>

struct monitor {
	monitor_t id;
	crtc_t crtc;
	struct geom geom;
	workspace_t workspace;
	layout_t layout;

	void *data;

	struct {
		monitor_call_t *func;
		void *data;
	} callbacks[MONITOR_EVENT_LAST];
};

static struct set *_monitors = NULL;

static inline int __init(void)
{
	struct geom null_geom;
	int err;

	if (_monitors) {
		return 0;
	}

	if ((err = set_new(&_monitors)) < 0) {
		return err;
	}

	/* allocate a null monitor that is used to hide workspaces */
	null_geom.x = -1920;
	null_geom.w = 1920;
	null_geom.y = -1080;
	null_geom.h = 1080;

	return (int)monitor_new(0, null_geom);
}

static inline int __get_monitor(struct monitor **monitor, const monitor_t mid)
{
	int err;

	if (mid < 0) {
		return -EINVAL;
	}

	if ((err = set_get(_monitors, mid, (void**)monitor)) < 0) {
		return err;
	}

	if (!*monitor) {
		return -EBADF;
	}

	return 0;
}

monitor_t monitor_new(const crtc_t crtc, const struct geom geom)
{
	struct monitor *monitor;
	int err;

	if ((err = __init()) < 0) {
		return err;
	}

	if (!(monitor = calloc(1, sizeof(*monitor)))) {
		return -ENOMEM;
	}

	monitor->crtc = crtc;
	monitor->geom = geom;
	monitor->layout = LAYOUT_TATE;

	if ((err = set_nq(_monitors, monitor)) < 0) {
		free(monitor);
	} else {
		monitor->id = (monitor_t)err;
	}

	return (monitor_t)err;
}

int monitor_free(const monitor_t mid)
{
	struct monitor *monitor;
	int err;

	if ((err = set_unset(_monitors, mid, (void**)&monitor)) < 0) {
		return err;
	}

	if (!monitor) {
		return -EBADF;
	}

	/* signal event handlers */

	free(monitor);
	return 0;
}

int monitor_set_geometry(const monitor_t mid, const struct geom geom)
{
	struct monitor *monitor;
	int err;

	if ((err = __get_monitor(&monitor, mid)) < 0) {
		return err;
	}

	monitor->geom = geom;
	return 0;
}

int monitor_get_geometry(const monitor_t mid, struct geom *geom)
{
	struct monitor *monitor;
	int err;

	if ((err = __get_monitor(&monitor, mid)) < 0) {
		return err;
	}

	*geom = monitor->geom;
	return 0;
}

int monitor_set_data(const monitor_t mid, void *data)
{
	struct monitor *monitor;
	int err;

	if ((err = __get_monitor(&monitor, mid)) < 0) {
		return err;
	}

	monitor->data = data;
	return 0;
}

int monitor_get_data(const monitor_t mid, void **data)
{
	struct monitor *monitor;
	int err;

	if (!data) {
		return -EINVAL;
	}

	if ((err = __get_monitor(&monitor, mid)) < 0) {
		return err;
	}

	*data = monitor->data;
	return 0;
}

struct monitor_cmp_data_args {
	int (*cmp)(const monitor_t, void*, void*);
	void *data;
};

static int _monitor_cmp_data(struct monitor *monitor, struct monitor_cmp_data_args *args)
{
	return args->cmp(monitor->id, monitor->data, args->data);
}

static int _monitor_cmp_crtc(struct monitor *monitor, const crtc_t *crtc)
{
	return monitor->crtc - *crtc;
}

static int _monitor_contains_geom(struct monitor *monitor, const struct geom *geom)
{
	return geom_contains(monitor->geom, *geom) ? 0 : 1;
}

monitor_t monitor_search(int (*cmp)(const monitor_t, void*, void*), void *data)
{
	struct monitor_cmp_data_args args;

	args.cmp = cmp;
	args.data = data;

	return set_search(_monitors, (int(*)(void*, void*))_monitor_cmp_data, &args);
}

monitor_t monitor_of_crtc(const crtc_t crtc)
{
	return set_search(_monitors, (int(*)(void*, void*))_monitor_cmp_crtc, (void*)&crtc);
}

monitor_t monitor_at(const struct geom pos)
{
	return set_search(_monitors, (int(*)(void*, void*))_monitor_contains_geom, (void*)&pos);
}

monitor_t monitor_at_xy(const int x, const int y)
{
	struct geom pos;

	pos.x = x;
	pos.y = y;
	pos.w = pos.h = 0;

	return monitor_at(pos);
}

int monitor_set_callback(const monitor_t mid,
                         const monitor_event_t event,
                         monitor_call_t *func,
                         void *data)
{
	struct monitor *monitor;
	int err;

	if (event < 0 || event >= MONITOR_EVENT_LAST) {
		return -EINVAL;
	}

	if ((err = __get_monitor(&monitor, mid)) < 0) {
		return err;
	}

	monitor->callbacks[event].func = func;
	monitor->callbacks[event].data = data;

	return 0;
}

int monitor_notify(const monitor_t mid,
                   const monitor_event_t event,
                   void *context)
{
	struct monitor *monitor;
	int err;

	if ((err = __get_monitor(&monitor, mid)) < 0) {
		return err;
	}

	if (monitor->callbacks[event].func) {
		monitor->callbacks[event].func(mid, monitor->callbacks[event].data, context);
	}

	return 0;
}

struct monitor_foreach_args {
	int (*func)(const monitor_t, void*);
	void *context;
};

static int _monitor_foreach_call(struct monitor *monitor, const int idx,
                                 struct monitor_foreach_args *args)
{
	return args->func(monitor->id, args->context);
}

int monitor_foreach(int (*func)(const monitor_t, void*), void *context)
{
	struct monitor_foreach_args args;

	if (!_monitors) {
		/* Nothing to do before the first monitor was connected */
		return 0;
	}

	args.func = func;
	args.context = context;

	return set_foreach(_monitors,
	                   (int(*)(void*, const int, void*))_monitor_foreach_call,
	                   &args);
}

int monitor_set_workspace(const monitor_t mid, const workspace_t wid)
{
	struct monitor *monitor;
	struct monitor *old_monitor;
	monitor_t old_mid;
	workspace_t old_wid;
	int err;

	/*
	 * Assign the workspace with id `wid` to the monitor with id `mid`. The
	 * workspace may already be assigned to another monitor, and the monitor
	 * may already have another workspace assigned. In this case, we perform
	 * a swap.
	 */

	if ((err = __get_monitor(&monitor, mid)) < 0) {
		return err;
	}

	if (monitor->workspace == wid) {
		return -EALREADY;
	}

	old_wid = monitor->workspace;
	old_mid = workspace_get_viewer(wid);

	if ((err = __get_monitor(&old_monitor, old_mid)) >= 0) {
		old_monitor->workspace = old_wid;
		monitor_notify(old_mid, MONITOR_EVENT_WORKSPACE_CHANGED, (void*)&old_wid);
		workspace_set_viewer(old_wid, old_mid);
	}

	monitor->workspace = wid;
	workspace_set_viewer(wid, mid);
	monitor_notify(mid, MONITOR_EVENT_WORKSPACE_CHANGED, (void*)&wid);

	return 0;
}

workspace_t monitor_get_workspace(const monitor_t mid)
{
	struct monitor *monitor;
	int err;

	if ((err = __get_monitor(&monitor, mid)) < 0) {
		return err;
	}

	return monitor->workspace;
}

layout_t monitor_get_layout(const monitor_t mid)
{
	struct monitor *monitor;
	int err;

	if ((err = __get_monitor(&monitor, mid)) < 0) {
		return err;
	}

	return monitor->layout;
}

int monitor_set_layout(const monitor_t mid, const layout_t layout)
{
	struct monitor *monitor;
	int err;

	if (layout < 0 || layout >= LAYOUT_LAST) {
		return -EINVAL;
	}

	if ((err = __get_monitor(&monitor, mid)) < 0) {
		return err;
	}

	monitor->layout = layout;
	monitor_notify(mid, MONITOR_EVENT_LAYOUT_CHANGED, (void*)&layout);

	return 0;
}
