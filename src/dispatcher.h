#ifndef DISPATCHER_H
#define DISPATCHER_H

struct dispatcher;

typedef enum {
	DISPATCHER_EVENT_IN    = (1 << 0),
	DISPATCHER_EVENT_ERROR = (1 << 1),
	DISPATCHER_EVENT_CLOSE = (1 << 2),
} dispatcher_event_t;

typedef void (dispatcher_call_t)(int, dispatcher_event_t, void*);

int dispatcher_new(struct dispatcher **disp);
int dispatcher_free(struct dispatcher **disp);

int dispatcher_watch_fd(struct dispatcher *disp, int fd, dispatcher_call_t *func, void *data);
int dispatcher_run(struct dispatcher *disp);

#endif /* DISPATCHER_H */
