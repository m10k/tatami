#include "client.h"
#include "common.h"
#include "set.h"
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

struct client {
	client_t id;

	struct geom geom;
	struct geom pointer;

	workspace_t workspace;

	void *data;

	struct {
		client_call_t *func;
		void *data;
	} callbacks[CLIENT_EVENT_LAST];
};

static struct set *_clients = NULL;

static inline int __init(void)
{
	return _clients ? 0 : set_new(&_clients);
}

static inline int __get_client(struct client **client, const client_t cid)
{
	int err;

	if (cid < 0) {
		return -EINVAL;
	}

	if ((err = set_get(_clients, cid, (void**)client)) < 0) {
		return err;
	}

	if (!*client) {
		return -EBADF;
	}

	return 0;
}

client_t client_new(void)
{
	struct client *client;
	int err;

	if ((err = __init()) < 0) {
		return err;
	}

	if (!(client = calloc(1, sizeof(*client)))) {
		return -ENOMEM;
	}

	client->pointer.x = client->pointer.y = -1;
	client->pointer.w = client->pointer.h = 1;

	if ((err = set_nq(_clients, client)) < 0) {
		free(client);
	} else {
		client->id = err;
	}

	return err;
}

int client_free(const client_t cid)
{
	struct client *client;
	int err;

	if ((err = set_unset(_clients, cid, (void**)&client)) < 0) {
		return err;
	}

	if (!client) {
		return -EBADF;
	}

	free(client);
	return 0;
}

int client_set_geometry(const client_t cid,
                        const struct geom geom)
{
	struct client *client;
	int err;

	if ((err = __get_client(&client, cid)) < 0) {
		return err;
	}

	if (memcmp(&client->geom, &geom, sizeof(geom)) == 0) {
		return -EALREADY;
	}

	client->geom = geom;
	return 0;
}

int client_get_geometry(const client_t cid,
                        struct geom *geom)
{
	struct client *client;
	int err;

	if (!geom) {
		return -EINVAL;
	}

	if ((err = __get_client(&client, cid)) < 0) {
		return err;
	}

	*geom = client->geom;
	return 0;
}

int client_set_pointer(const client_t cid,
		       const struct geom pointer)
{
	struct client *client;
	int err;

	if ((err = __get_client(&client, cid)) < 0) {
		return err;
	}

	if (memcmp(&client->pointer, &pointer, sizeof(pointer)) == 0) {
		return -EALREADY;
	}

	client->pointer = pointer;
	return 0;
}

int client_get_pointer(const client_t cid,
		       struct geom *pointer)
{
	struct client *client;
	int err;

	if ((err = __get_client(&client, cid)) < 0) {
		return err;
	}

	*pointer = client->pointer;
	return 0;
}

int client_set_data(const client_t cid, void *data)
{
	struct client *client;
	int err;

	if ((err = __get_client(&client, cid)) < 0) {
		return err;
	}

	client->data = data;
	return 0;
}

int client_get_data(const client_t cid, void **data)
{
	struct client *client;
	int err;

	if (!data) {
		return -EINVAL;
	}

	if ((err = __get_client(&client, cid)) < 0) {
		return err;
	}

	*data = client->data;
	return 0;
}

struct client_cmp_data_args {
	int (*cmp)(const client_t, void*, void*);
	void *data;
};

static int _client_cmp_data(struct client *client, struct client_cmp_data_args *args)
{
	return args->cmp(client->id, client->data, args->data);
}

int client_set_callback(const client_t cid,
                        const client_event_t event,
                        client_call_t *func,
                        void *data)
{
	struct client *client;
	int err;

	if (event < 0 || event >= CLIENT_EVENT_LAST) {
		return -EINVAL;
	}

	if ((err = __get_client(&client, cid)) < 0) {
		return err;
	}

	client->callbacks[event].func = func;
	client->callbacks[event].data = data;

	return 0;
}

int client_notify(const client_t cid, const client_event_t event, void *context)
{
	struct client *client;
	int err;

	if (event < 0 || event >= CLIENT_EVENT_LAST) {
		return -EINVAL;
	}

	if ((err = __get_client(&client, cid)) < 0) {
		return err;
	}

	if (client->callbacks[event].func) {
		client->callbacks[event].func(cid, client->callbacks[event].data, context);
	}

	return 0;
}

client_t client_search(int (*cmp)(const client_t, void*, void*), void *data)
{
	struct client_cmp_data_args args;

	args.cmp = cmp;
	args.data = data;

	return set_search(_clients, (int(*)(void*, void*))_client_cmp_data, &args);
}

int client_set_workspace(const client_t cid, const workspace_t workspace)
{
	struct client *client;
	int err;

	if ((err = __get_client(&client, cid)) < 0) {
		return err;
	}

	client->workspace = workspace;
	return 0;
}

workspace_t client_get_workspace(const client_t cid)
{
	struct client *client;
	int err;

	if ((err = __get_client(&client, cid)) < 0) {
		return err;
	}

	return client->workspace;
}
