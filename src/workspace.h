#ifndef WORKSPACE_H
#define WORKSPACE_H

#include "common.h"
#include "client.h"

typedef enum {
	WORKSPACE_EVENT_CLIENT_ATTACHED = 0,
	WORKSPACE_EVENT_CLIENT_DETACHED,
	WORKSPACE_EVENT_CLIENT_REORDERED,
	WORKSPACE_EVENT_VIEWER_SET,
	WORKSPACE_EVENT_VIEWER_UNSET,
	WORKSPACE_EVENT_VIEWER_CHANGED,
	WORKSPACE_EVENT_LAST
} workspace_event_t;

typedef void (workspace_call_t)(const workspace_t, void*, void*);

#define WORKSPACE_VALID(w) ((w) >= 0)

workspace_t workspace_new(void);
workspace_t workspace_get_unviewed(void);

int workspace_free(const workspace_t wid);

int workspace_attach_client(const workspace_t wid,
                            const client_t cid);
int workspace_detach_client(const workspace_t wid,
                            const client_t cid);

int workspace_set_callback(const workspace_t wid,
                           const workspace_event_t event,
                           workspace_call_t *callback,
                           void *data);

int workspace_notify(const workspace_t wid,
                     const workspace_event_t event,
                     void *context);

int workspace_foreach(int(*func)(const workspace_t, void*), void *context);

int workspace_set_viewer(const workspace_t wid, const monitor_t viewer);
monitor_t workspace_get_viewer(const workspace_t wid);

#endif /* WORKSPACE_H */
