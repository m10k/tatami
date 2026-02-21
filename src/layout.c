#include "common.h"
#include "layout.h"
#include <errno.h>
#include <stddef.h>

#define PADDING 8

struct layout {
	char *name;
	struct geom (*arrange)(const int, const int, const struct geom);
};

static struct geom tate(const int client_num,
                        const int num_clients,
                        const struct geom monitor)
{
	struct geom geom;

	geom.w = (monitor.w - (num_clients + 1) * PADDING) / num_clients;
	geom.h = monitor.h - 2 * PADDING;
	geom.x = monitor.x + PADDING + client_num * (geom.w + PADDING);
	geom.y = monitor.y + PADDING;

	return geom;
}

static struct geom yoko(const int client_num,
                        const int num_clients,
                        const struct geom monitor)
{
	struct geom geom;

	geom.w = monitor.w - 2 * PADDING;
	geom.h = (monitor.h - (num_clients + 1) * PADDING) / num_clients;
	geom.x = monitor.x + PADDING;
	geom.y = monitor.y + PADDING + client_num * (geom.h + PADDING);

	return geom;
}

static struct layout layout_tate = {
	.name = "縦",
	.arrange = tate,
};

static struct layout layout_yoko = {
	.name = "横",
	.arrange = yoko,
};

struct layout *layouts[] = {
	&layout_tate,
	&layout_yoko,
	NULL
};

int layout_arrange(const layout_t layout,
                   const int client_num,
                   const int num_clients,
                   const struct geom usable_area,
                   struct geom *output)
{
	if (layout < 0 || layout >= LAYOUT_LAST ||
	    client_num < 0 || client_num >= num_clients) {
		return -EINVAL;
	}

	*output = layouts[layout]->arrange(client_num, num_clients, usable_area);

	return 0;
}
