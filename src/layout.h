#ifndef LAYOUT_H
#define LAYOUT_H

#include "common.h"

typedef enum {
	LAYOUT_TATE,
	LAYOUT_YOKO,
	LAYOUT_LAST
} layout_t;

#define LAYOUT_VALID(l) ((l) >= 0 && (l) < LAYOUT_LAST)

int layout_arrange(const layout_t layout,
                   const int client,
                   const int num_clients,
                   const struct geom usable_area,
                   struct geom *output);

#endif /* LAYOUT_H */
