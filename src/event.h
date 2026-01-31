#ifndef EVENT_H
#define EVENT_H

#include "client.h"
#include "common.h"
#include "monitor.h"
#include "workspace.h"
#include <sys/time.h>

enum event_type {
	EVENT_NONE = 0,
	EVENT_ROOT_GEOMETRY_CHANGE,
	EVENT_CONFIGURE_NOTIFY,
	EVENT_CONFIGURE_REQUEST,
	EVENT_DESTROY_NOTIFY,
	EVENT_ENTER_NOTIFY,
	EVENT_LEAVE_NOTIFY,
	EVENT_EXPOSE,
	EVENT_FOCUS_IN,
	EVENT_FOCUS_OUT,
	EVENT_KEY_PRESS,
	EVENT_MAPPING_NOTIFY,
	EVENT_MAP_REQUEST,
	EVENT_MOTION_NOTIFY,
	EVENT_PROPERTY_NOTIFY,
	EVENT_UNMAP_NOTIFY,
	EVENT_CRTC_CHANGE,
	EVENT_OUTPUT_CHANGE,
	EVENT_LAST
};

typedef enum event_type event_type_t;

struct event {
	event_type_t type;
	struct timeval time;

	/*
	 * The monitor, workspace, and client that were
	 * focused at the time when the event was received
	 */
	struct {
		monitor_t monitor;
		workspace_t workspace;
		client_t client;
	} focus;

	union {
		struct {
			struct geom geom;
			client_t client;
		} configure_request;

		struct {
			client_t client;
			struct geom geom;
		} configure_notify;

		struct {
			struct geom geom;
		} root_geom_change;

		struct {
			client_t client;
		} destroy_notify;

		struct {
			client_t client;
			monitor_t monitor;
			int mode;
			int detail;
		} enter_notify;

		struct {
			client_t client;
			monitor_t monitor;
			int mode;
			int detail;
		} leave_notify;

		struct {
			client_t client;
			int count;
		} expose;

		struct {
			client_t client;
		} focus_in;

		struct {
			client_t client;
		} focus_out;

		struct {
			client_t client;
		} map_request;

		struct {
			struct geom pointer;
			client_t client;
			monitor_t monitor;
		} motion_notify;

		struct {
			client_t client;
		} property_notify;

		struct {
			client_t client;
			int send_event;
		} unmap_notify;

		struct {
			crtc_t crtc;
			struct geom geom;
			int rotation;
			int mode;
		} crtc_change;

		struct {
			crtc_t crtc;
			int output;
			int connection;
		} output_change;
	} data;

	void *extra;
};

struct eventq {
	struct {
		int nq;
		int dq;
	} idx;

	struct event *q[64];
};

int event_new(struct event **event, event_type_t type);
int event_free(struct event **event);

int eventq_nq(struct eventq *eventq, struct event *event);
int eventq_dq(struct eventq *eventq, struct event **event);

#endif /* EVENT_H */
