#include "dispatcher.h"
#include <errno.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <unistd.h>

struct dispatcher {
	int fd;
	int running;
};

struct event_handler {
	int fd;
	dispatcher_call_t *func;
	void *data;
};

int dispatcher_new(struct dispatcher **disp)
{
	struct dispatcher *dispatcher;
	int err;

	err = -ENOMEM;
	dispatcher = calloc(1, sizeof(*dispatcher));

	if (dispatcher) {
		dispatcher->fd = epoll_create1(EPOLL_CLOEXEC);
		err = -errno;

		if (dispatcher->fd >= 0) {
			err = 0;
		}
	}

	if (!err) {
		*disp = dispatcher;
	} else if (dispatcher) {
		free(dispatcher);
	}

	return err;
}

int dispatcher_free(struct dispatcher **disp)
{
	if (!disp) {
		return -EINVAL;
	}

	if (!*disp) {
		return -ENOENT;
	}

	if ((*disp)->fd >= 0) {
		close((*disp)->fd);
	}
	free(*disp);
	*disp = NULL;

	return 0;
}

int dispatcher_watch_fd(struct dispatcher *dispatcher, int fd,
                        dispatcher_call_t *func, void *data)
{
	struct event_handler *handler;
	struct epoll_event ev;
	int err;

	if (!dispatcher || fd < 0 || !func) {
		return -EINVAL;
	}

	if (dispatcher->fd < 0) {
		return -EBADFD;
	}

	handler = calloc(1, sizeof(*handler));
	if (!handler) {
		return -ENOMEM;
	}

	err = 0;
	handler->fd = fd;
	handler->func = func;
	handler->data = data;

	ev.events = EPOLLIN | EPOLLERR | EPOLLHUP;
	ev.data.ptr = handler;

	if (epoll_ctl(dispatcher->fd, EPOLL_CTL_ADD, fd, &ev) < 0) {
		err = -errno;
		free(handler);
	}

	return err;
}

int dispatcher_run(struct dispatcher *dispatcher)
{
	struct epoll_event events[8];
	int n;
	int err;

	if (!dispatcher) {
		return -EINVAL;
	}

	err = 0;
	n = epoll_wait(dispatcher->fd, events, sizeof(events) / sizeof(events[0]), -1);

	if (n < 0) {
		return -errno;
	}

	while (--n >= 0) {
		struct event_handler *handler;
		dispatcher_event_t event_mask;

		handler = (struct event_handler*)events[n].data.ptr;
		event_mask = 0;

		if (events[n].events & EPOLLIN) {
			event_mask |= DISPATCHER_EVENT_IN;
		}
		if (events[n].events & EPOLLERR) {
			event_mask |= DISPATCHER_EVENT_ERROR;
		}
		if (events[n].events & EPOLLHUP) {
			event_mask |= DISPATCHER_EVENT_CLOSE;
		}

		handler->func(handler->fd, event_mask, handler->data);
	}

	return err;
}
