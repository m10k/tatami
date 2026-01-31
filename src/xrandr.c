#include "common.h"
#include "log.h"
#include "set.h"
#include "xrandr.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

struct crtc {
	xrandr_crtc_t id;
	int outputs;
	int active_outputs;
	struct geom geom;
};

struct output {
	xrandr_output_t id;
	xrandr_crtc_t crtc;
	int connected;
};

struct xrandr {
	int event_base;
	int error_base;

	Display *display;
	Window root;

	struct set *outputs;
	struct set *crtcs;

	struct {
		xrandr_call_t *func;
		void *data;
	} callbacks[XRANDR_EVENT_LAST];
};

static struct xrandr _xrandr;

static struct crtc* add_crtc(struct xrandr *xrandr, const xrandr_crtc_t crtc_id)
{
	struct crtc *crtc;

	if ((crtc = calloc(1, sizeof(*crtc)))) {
		crtc->id = crtc_id;

		if (set_set(xrandr->crtcs, crtc_id, crtc) < 0) {
			free(crtc);
			crtc = NULL;
		}
	}

	return crtc;
}

static struct crtc* get_crtc(struct xrandr *xrandr, const xrandr_crtc_t crtc_id)
{
	struct crtc *crtc;

	if (set_get(xrandr->crtcs, crtc_id, (void**)&crtc) < 0) {
		crtc = add_crtc(xrandr, crtc_id);
	}

	return crtc;
}

static struct output* add_output(struct xrandr *xrandr, const xrandr_output_t output_id)
{
	struct output *output;

	if ((output = calloc(1, sizeof(*output)))) {
		output->id = output_id;

		if (set_set(xrandr->outputs, output_id, output) < 0) {
			free(output);
			output = NULL;
		}
	}

	return output;
}

static struct output* get_output(struct xrandr *xrandr, xrandr_output_t output_id)
{
	struct output *output;

	if (set_get(xrandr->outputs, output_id, (void*)&output) < 0) {
		output = add_output(xrandr, output_id);
	}

	return output;
}

static void invoke_handler(struct xrandr *xrandr,
                           const xrandr_event_t event,
                           const xrandr_crtc_t crtc,
                           const struct geom *geom)
{
	if (!xrandr || event < 0 || event >= XRANDR_EVENT_LAST) {
		/* FIXME: This should be logged */
		return;
	}

	if (xrandr->callbacks[event].func) {
		xrandr->callbacks[event].func(crtc, geom, xrandr->callbacks[event].data);
	}
}

static int update_crtc(struct xrandr *xrandr,
                       const xrandr_crtc_t crtc_id,
                       const struct geom geom)
{
	struct crtc *crtc;
	int crtc_changed;
	int was_unset;

	if (!(crtc = get_crtc(xrandr, crtc_id))) {
		return -ENOMEM;
	}

	was_unset = crtc->geom.w == 0 || crtc->geom.h == 0;
	crtc_changed = memcmp(&geom, &crtc->geom, sizeof(geom)) != 0;

	if (crtc_changed) {
		crtc->geom = geom;

		if (geom.w > 0 && geom.h > 0) {
			invoke_handler(xrandr, was_unset ? XRANDR_EVENT_MONITOR_ATTACHED :
			               XRANDR_EVENT_MONITOR_GEOMETRY_CHANGED, crtc_id, &geom);
		} else {
			invoke_handler(xrandr, XRANDR_EVENT_MONITOR_DETACHED, crtc_id, &geom);
		}
	}

	return crtc_changed;
}

static int count_crtc_outputs(struct output *output, const int idx, struct crtc *crtc)
{
	if (output->crtc == crtc->id) {
		crtc->outputs++;

		if (output->connected) {
			crtc->active_outputs++;
		}
	}

	return 0;
}

static int update_crtc_outputs(struct crtc *crtc, const int idx, struct xrandr *xrandr)
{
	int was_attached;

	was_attached = crtc->active_outputs > 0;
	crtc->outputs = 0;
	crtc->active_outputs = 0;

	set_foreach(_xrandr.outputs,
	            (int(*)(void*, const int, void*))count_crtc_outputs,
	            crtc);

	if (was_attached && crtc->active_outputs == 0) {
		invoke_handler(xrandr, XRANDR_EVENT_MONITOR_DETACHED, crtc->id, &crtc->geom);
	} else if (!was_attached && crtc->active_outputs > 0) {
		invoke_handler(xrandr, XRANDR_EVENT_MONITOR_ATTACHED, crtc->id, &crtc->geom);
	}

	return 0;
}

static void update_associations(struct xrandr *xrandr)
{
	set_foreach(xrandr->crtcs,
	            (int(*)(void*, const int, void*))update_crtc_outputs,
	            xrandr);
}

static int update_output(struct xrandr *xrandr,
                         const xrandr_output_t output_id,
                         const xrandr_crtc_t crtc_id,
                         int connected)
{
	struct output *output;
	int output_changed;

	if (!(output = get_output(xrandr, output_id))) {
		return -ENOMEM;
	}

	output_changed = output->crtc != crtc_id ||
	                 output->connected != connected;
	output->crtc = crtc_id;
	output->connected = connected;

	if (output_changed) {
		update_associations(xrandr);
	}

	return 0;
}

static void detect_configuration(struct xrandr *xrandr)
{
	XRRScreenResources *resources;
	int i;

	resources = XRRGetScreenResources(xrandr->display, xrandr->root);
	if (!resources) {
		return;
	}

	for (i = 0; i < resources->ncrtc; i++) {
		xrandr_crtc_t crtc_id;
		XRRCrtcInfo *crtc_info;
		struct geom geom;

		crtc_id = resources->crtcs[i];
		crtc_info = XRRGetCrtcInfo(xrandr->display, resources, crtc_id);

		if (!crtc_info) {
			continue;
		}

		geom.x = crtc_info->x;
		geom.y = crtc_info->y;

		if (crtc_info->rotation == RR_Rotate_90 ||
		    crtc_info->rotation == RR_Rotate_270) {
			geom.w = crtc_info->height;
			geom.h = crtc_info->width;
		} else {
			geom.w = crtc_info->width;
			geom.h = crtc_info->height;
		}

		update_crtc(xrandr, crtc_id, geom);
		XRRFreeCrtcInfo(crtc_info);
	}

	for (i = 0; i < resources->noutput; i++) {
		xrandr_output_t output_id;
		XRROutputInfo *output_info;

		output_id = resources->outputs[i];
		output_info = XRRGetOutputInfo(xrandr->display, resources, output_id);

		if (!output_info) {
			continue;
		}

		update_output(xrandr, output_id, output_info->crtc,
		              output_info->connection == RR_Connected);
		XRRFreeOutputInfo(output_info);
	}

	update_associations(xrandr);

	XRRFreeScreenResources(resources);
}

int xrandr_new(struct xrandr **xrandr, Display *display, Window root)
{
	struct xrandr *ctx;
	int err;
	int error_base;
	int event_base;

	if (!XRRQueryExtension(display, &event_base, &error_base)) {
		return -ENOTSUP;
	}

	if (!(ctx = calloc(1, sizeof(*ctx)))) {
		return -ENOMEM;
	}

	ctx->event_base = event_base;
	ctx->error_base = error_base;
	ctx->display = display;
	ctx->root = root;

	log_debug("XRANDR", "EventBase = %d, ErrorBase = %d\n", event_base, error_base);

	if (!(err = set_new(&ctx->crtcs)) &&
	    !(err = set_new(&ctx->outputs))) {

	}

	if (!err) {
		*xrandr = ctx;
	} else {
		set_free(&ctx->crtcs);
		set_free(&ctx->outputs);
	}

	return err;
}

int xrandr_set_callback(struct xrandr *xrandr,
                        xrandr_event_t event,
                        xrandr_call_t *callback,
                        void *data)
{
	if (!xrandr || event < 0 || event >= XRANDR_EVENT_LAST) {
		return -EINVAL;
	}

	xrandr->callbacks[event].func = callback;
	xrandr->callbacks[event].data = data;

	return 0;
}

int xrandr_init(struct xrandr *xrandr)
{
	detect_configuration(xrandr);

	XRRSelectInput(xrandr->display, xrandr->root,
	               RRCrtcChangeNotifyMask |
	               RROutputChangeNotifyMask |
	               RRScreenChangeNotifyMask);

	return 0;
}

static int handle_crtc_change_event(struct xrandr *xrandr, struct event *event)
{
        struct geom geom;

        if (!event->data.crtc_change.crtc) {
                return -EBADF;
        }

        geom = event->data.crtc_change.geom;

        if (event->data.crtc_change.rotation == RR_Rotate_90 ||
            event->data.crtc_change.rotation == RR_Rotate_270) {
                geom.w = event->data.crtc_change.geom.h;
                geom.h = event->data.crtc_change.geom.w;
        }

        return update_crtc(xrandr, event->data.crtc_change.crtc, geom);
}

static int handle_output_change_event(struct xrandr *xrandr, struct event *event)
{
        if (!event->data.output_change.output) {
                return -EBADF;
        }

        return update_output(xrandr, event->data.output_change.output,
                             event->data.output_change.crtc,
                             event->data.output_change.connection == RR_Connected);
}

int xrandr_handle_event(struct xrandr *xrandr, struct event *event)
{
	int err;

	switch (event->type) {
	case EVENT_CRTC_CHANGE:
		err = handle_crtc_change_event(xrandr, event);
		break;

	case EVENT_OUTPUT_CHANGE:
		err = handle_output_change_event(xrandr, event);
		break;

	default:
		log_info("XRandR", "Not handling non-XRandR event type 0x%0x\n", event->type);
		err = 0;
		break;
	}

	return err;
}

int xrandr_event_convert(struct xrandr *xrandr, XEvent *xevent, struct event **dst)
{
	int err;
	struct event *event;

	err = -EPROTONOSUPPORT;

	switch (xevent->type - xrandr->event_base) {
	case RRScreenChangeNotify:
		err = -ENOSYS;
		break;

	case RRNotify:
		switch (((XRRNotifyEvent*)xevent)->subtype) {
		case RRNotify_CrtcChange: {
			XRRCrtcChangeNotifyEvent *xrrevent;

			xrrevent = (XRRCrtcChangeNotifyEvent*)xevent;

			err = event_new(&event, EVENT_CRTC_CHANGE);

			if (!err) {
				event->data.crtc_change.crtc     = xrrevent->crtc;
				event->data.crtc_change.geom.x   = xrrevent->x;
				event->data.crtc_change.geom.y   = xrrevent->y;
				event->data.crtc_change.geom.w   = xrrevent->width;
				event->data.crtc_change.geom.h   = xrrevent->height;
				event->data.crtc_change.mode     = xrrevent->mode;
				event->data.crtc_change.rotation = xrrevent->rotation;
			}

			break;
		}

		case RRNotify_OutputChange: {
			XRROutputChangeNotifyEvent *xrrevent;

			xrrevent = (XRROutputChangeNotifyEvent*)xevent;

			err = event_new(&event, EVENT_OUTPUT_CHANGE);

			if (!err) {
				event->data.output_change.crtc       = xrrevent->crtc;
				event->data.output_change.output     = xrrevent->output;
				event->data.output_change.connection = xrrevent->connection;
			}

			break;
		}

		case RRNotify_OutputProperty:
		case RRNotify_ProviderChange:
		case RRNotify_ProviderProperty:
		case RRNotify_ResourceChange:
		case RRNotify_Lease:
			/* these are not used, so not implemented */
			err = -ENOSYS;
			break;
		}
		break;

	default:
		/* not an XRandR event */
		break;
	}

	if (!err) {
		*dst = event;
	}

	return err;
}
