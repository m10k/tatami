#include "event.h"
#include "log.h"
#include <errno.h>
#include <stdlib.h>
#include <sys/time.h>

#define QLEN 64
#define QMASK (QLEN - 1)

int event_new(struct event **event, event_type_t type)
{
	struct event *ev;
	int err;

	if (!event) {
		return -EINVAL;
	}

	if (!(ev = calloc(1, sizeof(*ev)))) {
		return -ENOMEM;
	}

	if (gettimeofday(&ev->time, NULL) < 0) {
		err = -errno;
		free(ev);
	} else {
		err = 0;
		ev->type = type;
		*event = ev;
	}

	return err;
}

int event_free(struct event **event)
{
	if (!event) {
		return -EINVAL;
	}

	if (!*event) {
		return -EALREADY;
	}

	if ((*event)->extra) {
		free((*event)->extra);
		(*event)->extra = NULL;
	}

	free(*event);
	*event = NULL;

	return 0;
}

int eventq_nq(struct eventq *eventq, struct event *event)
{
	if (!eventq || !event) {
		return -EINVAL;
	}

	if (eventq->q[eventq->idx.nq]) {
		return -EBUSY;
	}

	eventq->q[eventq->idx.nq] = event;
	eventq->idx.nq = (eventq->idx.nq + 1) & QMASK;

	return 0;
}

int eventq_dq(struct eventq *eventq, struct event **event)
{
	if (!eventq || !event) {
		return -EINVAL;
	}

	if (!eventq->q[eventq->idx.dq]) {
		return -ENOENT;
	}

	*event = eventq->q[eventq->idx.dq];
	eventq->q[eventq->idx.dq] = NULL;
	eventq->idx.dq = (eventq->idx.dq + 1) & QMASK;

	return 0;
}
