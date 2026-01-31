#ifndef CLIENT_H
#define CLIENT_H

#include "common.h"

typedef enum {
	CLIENT_EVENT_ATTACHED = 0,
	CLIENT_EVENT_DETACHED,
	CLIENT_EVENT_GEOMETRY_CHANGED,
	CLIENT_EVENT_POINTER_CHANGED,
	CLIENT_EVENT_FOCUS_GAINED,
	CLIENT_EVENT_FOCUS_LOST,
	CLIENT_EVENT_SHOWN,
	CLIENT_EVENT_HIDDEN,
	CLIENT_EVENT_LAST
} client_event_t;

typedef int client_t;
typedef void (client_call_t)(const client_t, void*, void*);

#define CLIENT_VALID(c) ((c) >= 0)

client_t client_new(void);
int client_free(const client_t cid);

int client_set_geometry(const client_t cid,
                        const struct geom geom);
int client_get_geometry(const client_t cid,
                        struct geom *geom);
int client_set_pointer(const client_t cid,
                       const struct geom pointer);
int client_get_pointer(const client_t cid,
                       struct geom *pointer);

int client_set_data(const client_t cid, void *data);
int client_get_data(const client_t cid, void **data);

int client_set_callback(const client_t cid,
                        const client_event_t event,
                        client_call_t *callback,
                        void *data);
int client_notify(const client_t cid,
                  const client_event_t event,
                  void *context);

client_t client_search(int (*cmp)(const client_t, void*, void*), void *data);

#endif /* CLIENT_H */
