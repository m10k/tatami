#include "common.h"
#include "layout.h"
#include "log.h"
#include "monitor.h"
#include "set.h"
#include "wm.h"
#include "workspace.h"
#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct workspace {
	workspace_t id;
	char *name;

	client_t *clients;
	int num_clients;
	int focus;
	monitor_t viewer;

	struct {
		workspace_call_t *func;
		void *data;
	} callbacks[WORKSPACE_EVENT_LAST];
};

static struct set *_workspaces = NULL;

static inline int __init(void)
{
	int err;

	if (_workspaces) {
		return 0;
	}

	if ((err = set_new(&_workspaces)) < 0) {
		return err;
	}

	/* allocate null workspace */
	return (int)workspace_new();
}

static inline int __get_workspace(struct workspace **workspace, const workspace_t wid)
{
	int err;

	if (wid < 0) {
		return -EINVAL;
	}

	if ((err = set_get(_workspaces, wid, (void**)workspace)) < 0) {
		return err;
	}

	if (!*workspace) {
		return -EBADF;
	}

	return 0;
}

static inline void _set_callback(struct workspace *workspace,
                                 const workspace_event_t event,
                                 workspace_call_t *func,
                                 void *data)
{
	workspace->callbacks[event].func = func;
	workspace->callbacks[event].data = data;
}

static int workspace_hide(const workspace_t wid)
{
	struct workspace *workspace;
	int err;
	int i;

	if ((err = __get_workspace(&workspace, wid)) < 0) {
		return err;
	}

	for (i = 0; i < workspace->num_clients; i++) {
		wm_hide_client(workspace->clients[i]);
	}

	return 0;
}

static void _workspace_viewer_changed(const workspace_t wid, void *null, monitor_t *viewer)
{
	if (MONITOR_VALID(*viewer)) {
		workspace_hide(wid);
	}
}

static void _workspace_rearrange(const workspace_t wid, void *null, void *context)
{
	struct workspace *workspace;
	struct geom monitor_geom;
	layout_t layout;
	int i;
	int err;

	log_debug("WS", "Rearranging workspace %ld", wid);

	if ((err = __get_workspace(&workspace, wid)) < 0) {
		log_error("WS", "Cannot rearrange invalid workspace %ld: %s",
		          wid, strerror(-err));
		return;
	}

	if (!MONITOR_VALID(workspace->viewer)) {
		log_error("WM", "Cannot rearrange workspace %ld with invalid viewer",
		          wid);
		return;
	}

	if ((err = monitor_get_geometry(workspace->viewer, &monitor_geom)) < 0) {
		log_error("WM", "Cannot rearrange workspace %ld. Could not get "
		          "geometry of viewer %ld: %s", wid, workspace->viewer,
		          strerror(-err));
		return;
	}

	/* FIXME: Get viewer's layout algorithm */
	layout = LAYOUT_TATE;

	for (i = 0; i < workspace->num_clients; i++) {
		struct geom client_geom;

		err = layout_arrange(layout, i, workspace->num_clients, monitor_geom, &client_geom);
		if (err < 0) {
			log_error("WM", "Could not determine geometry for client: %s", strerror(-err));
			break;
		}

		wm_move_client(workspace->clients[i], client_geom);
	}

	log_debug("WS", "Rearranged %d clients", workspace->num_clients);
}

workspace_t workspace_new(void)
{
	struct workspace *space;
	int err;

	if ((err = __init()) < 0) {
		return err;
	}

	if (!(space = calloc(1, sizeof(*space)))) {
		return -ENOMEM;
	}

	if ((err = set_nq(_workspaces, space)) < 0) {
		free(space);
	} else {
		space->id = (workspace_t)err;

		_set_callback(space, WORKSPACE_EVENT_VIEWER_CHANGED,
		              (workspace_call_t*)_workspace_viewer_changed, NULL);
		_set_callback(space, WORKSPACE_EVENT_CLIENT_REORDERED,
		              (workspace_call_t*)_workspace_rearrange, NULL);
	}

	return (workspace_t)err;
}

int workspace_free(const workspace_t wid)
{
	struct workspace *workspace;
	int err;

	if ((err = set_unset(_workspaces, wid, (void**)&workspace)) < 0) {
		return err;
	}

	if (!workspace) {
		return -EBADF;
	}

	free(workspace->clients);
	free(workspace);

	return 0;
}

static int workspace_insert_client(struct workspace *workspace, const client_t cid)
{
	client_t *new_clients;
	int new_num_clients;

	if (workspace->num_clients == INT_MAX) {
		return -EOVERFLOW;
	}

	new_num_clients = workspace->num_clients + 1;
	if ((SIZE_MAX / sizeof(client_t)) < new_num_clients) {
		return -EOVERFLOW;
	}

	if (!(new_clients = realloc(workspace->clients,
	                            sizeof(client_t) * new_num_clients))) {
		return -ENOMEM;
	}

	new_clients[new_num_clients - 1] = cid;

	workspace->clients = new_clients;
	workspace->num_clients = new_num_clients;

	return 0;
}

int workspace_attach_client(const workspace_t wid, const client_t cid)
{
	struct workspace *workspace;
	int err;

	if (!WORKSPACE_VALID(wid) || !CLIENT_VALID(cid)) {
		return -EINVAL;
	}

	if ((err = __get_workspace(&workspace, wid)) < 0) {
		return err;
	}

	if ((err = workspace_insert_client(workspace, cid)) < 0) {
		return err;
	}

	if (!err) {
		workspace_notify(wid, WORKSPACE_EVENT_CLIENT_ATTACHED, (void*)(ptrdiff_t)cid);
		workspace_notify(wid, WORKSPACE_EVENT_CLIENT_REORDERED, NULL);
	}

	client_set_workspace(cid, wid);
	log_debug("WS", "Attached client %ld to workspace %ld\n", cid, wid);

	return err;
}

static int workspace_remove_client(struct workspace *workspace, const client_t client)
{
	client_t *new_clients;
	int new_num_clients;

	int src_idx;
	int dst_idx;
	int rem_idx;

	if (workspace->num_clients <= 0) {
		return -ENOENT;
	}

	new_num_clients = workspace->num_clients - 1;

	if (!(new_clients = malloc(new_num_clients * sizeof(client_t)))) {
		return -ENOMEM;
	}

	for (src_idx = dst_idx = 0, rem_idx = -1; src_idx < workspace->num_clients; src_idx++) {
		if (workspace->clients[src_idx] == client) {
			rem_idx = src_idx;
			continue;
		}

		new_clients[dst_idx] = workspace->clients[src_idx];
		dst_idx++;
	}

	if (rem_idx < 0) {
		free(new_clients);
		return -ENOENT;
	}

#define IS_FIRST(idx)                    ((idx) == 0)
#define IS_LAST(idx, size)               (((idx) + 1) == size)
#define IS_CASE1(focused, removed)       (focused > removed)
#define IS_CASE3(focused, removed, size) ((focused) == (removed) && IS_LAST(removed, size))

	if (IS_CASE1(workspace->focus, rem_idx) ||
	    IS_CASE3(workspace->focus, rem_idx, workspace->num_clients)) {
		workspace->focus--;
	}

	free(workspace->clients);
	workspace->clients = new_clients;
	workspace->num_clients = new_num_clients;

#undef IS_FIRST
#undef IS_LAST
#undef IS_CASE1
#undef IS_CASE3

	return 0;
}

int workspace_detach_client(const workspace_t wid, const client_t cid)
{
	struct workspace *workspace;
	int err;

	if (!CLIENT_VALID(cid)) {
		return -EINVAL;
	}

	if ((err = __get_workspace(&workspace, wid)) < 0) {
		return err;
	}

	err = workspace_remove_client(workspace, cid);

	if (!err) {
		workspace_notify(wid, WORKSPACE_EVENT_CLIENT_REORDERED, NULL);
		workspace_notify(wid, WORKSPACE_EVENT_CLIENT_DETACHED, (void*)(ptrdiff_t)cid);
	}

	return err;
}

int workspace_set_callback(const workspace_t wid,
                           const workspace_event_t event,
                           workspace_call_t *func,
                           void *data)
{
	struct workspace *workspace;
	int err;

	if (event < 0 || event >= WORKSPACE_EVENT_LAST) {
		return -EINVAL;
	}

	if ((err = __get_workspace(&workspace, wid)) < 0) {
		return err;
	}

	_set_callback(workspace, event, func, data);

	return 0;
}

int workspace_notify(const workspace_t wid, const workspace_event_t event, void *context)
{
	struct workspace *workspace;
	int err;

	if (event < 0 || event >= WORKSPACE_EVENT_LAST) {
		return -EINVAL;
	}

	if ((err = __get_workspace(&workspace, wid)) < 0) {
		return err;
	}

	if (workspace->callbacks[event].func) {
		workspace->callbacks[event].func(wid, workspace->callbacks[event].data, context);
	}

	return 0;
}

int workspace_set_viewer(const workspace_t wid, const monitor_t viewer)
{
	struct workspace *workspace;
	int err;

	if ((err = __get_workspace(&workspace, wid)) < 0) {
		return err;
	}

	if (workspace->viewer == viewer) {
		return -EALREADY;
	}

	workspace->viewer = viewer;
	workspace_notify(wid, WORKSPACE_EVENT_VIEWER_CHANGED, (void*)&viewer);

	return 0;
}

monitor_t workspace_get_viewer(const workspace_t wid)
{
	struct workspace *workspace;
	int err;

	if ((err = __get_workspace(&workspace, wid)) < 0) {
		return err;
	}

	return workspace->viewer;
}
static int _workspace_is_unviewed(struct workspace *workspace, void *data)
{
	return !MONITOR_VALID(workspace->viewer);
}

workspace_t workspace_get_unviewed(void)
{
	return set_search(_workspaces, (int(*)(void*, void*))_workspace_is_unviewed, NULL);
}

int workspace_get_focus(const workspace_t wid)
{
	struct workspace *workspace;
	int err;

	if ((err = __get_workspace(&workspace, wid)) < 0) {
		return err;
	}

	return workspace->focus;
}

int workspace_set_focus(const workspace_t wid, const int focus)
{
	struct workspace *workspace;
	int err;
	int effective_focus;

	if ((err = __get_workspace(&workspace, wid)) < 0) {
		return err;
	}

	if (workspace->num_clients == 0) {
		return -ERANGE;
	}

	/* ensure that the new focus is within the bounds of the array */
	effective_focus = focus;
	while (effective_focus < 0) {
		effective_focus += workspace->num_clients;
	}
	effective_focus = effective_focus % workspace->num_clients;

	if (effective_focus != workspace->focus) {
		client_t old_client;
		client_t new_client;

		old_client = workspace->clients[workspace->focus];
		workspace->focus = effective_focus;
		new_client = workspace->clients[workspace->focus];

		client_notify(old_client, CLIENT_EVENT_FOCUS_LOST, NULL);
		workspace_notify(wid, WORKSPACE_EVENT_FOCUS_CHANGED, NULL);
		client_notify(new_client, CLIENT_EVENT_FOCUS_GAINED, NULL);
	}

	return 0;
}
