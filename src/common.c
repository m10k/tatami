#include "array.h"
#include "common.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>

#ifndef MIN
#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))
#endif /* !defined(MIN) */

#ifndef MAX
#define MAX(X,Y) ((X) > (Y) ? (X) : (Y))
#endif /* !defined(MAX) */

int geom_dupe(struct geom **dst, const struct geom src)
{
	struct geom *geom;

	if (!dst) {
		return -EINVAL;
	}

	geom = malloc(sizeof(*geom));
	if (!geom) {
		return -ENOMEM;
	}

	*geom = src;
	*dst = geom;

	return 0;
}

int geom_difference(const struct geom first, const struct geom second, struct geom ***diffs)
{
	struct geom **results;
	struct geom *dupe;
	int err;

	if (!diffs || *diffs) {
		return -EINVAL;
	}

	results = NULL;
	err = 0;

	if (second.x >= first.x + first.w ||
	    second.y >= first.y + first.h ||
	    first.x >= second.x + second.w ||
	    first.y >= second.y + second.h) {
		err = geom_dupe(&dupe, first);
		if (!err) {
			err = array_add((void***)&results, (void**)&dupe, 1);
			if (err < 0) {
				free(dupe);
			}
		}
	} else {
		struct geom d[4];
		int i;

		memset(&d, 0, sizeof(d));

		if (second.y > first.y && second.y < first.y + first.h) {
			d[0].h = second.y - first.y;
		}

		if (second.y + second.h > first.y && second.y + second.h < first.y + first.h) {
			d[1].h = (first.y + first.h) - (second.y + second.h);
		}

		if (second.x > first.x && second.x < first.x + first.w) {
			d[2].w = second.x - first.x;
		}

		if (second.x + second.w > first.x && second.x + second.w < first.x + first.w) {
			d[3].w = (first.x + first.w) - (second.x + second.w);
		}

		d[0].w = first.w;
		d[0].x = first.x;
		d[0].y = first.y;

		d[1].w = first.w;
		d[1].x = first.x;
		d[1].y = first.y + first.h - d[1].h;

		d[2].h = first.h - d[0].h - d[1].h;
		d[2].x = first.x;
		d[2].y = first.y + d[0].h;

		d[3].h = first.h - d[0].h - d[1].h;
		d[3].x = first.x + first.w - d[3].w;
		d[3].y = first.y + d[0].h;

		/* Add non-empty areas to results */
		for (i = 0; i < (sizeof(d) / sizeof(d[0])) && !err; i++) {
			if (d[i].w * d[i].h <= 0) {
				continue;
			}

			err = geom_dupe(&dupe, d[i]);
			if (!err) {
				err = array_add((void***)&results, (void**)&dupe, 1);
				if (err < 0) {
					free(dupe);
				}
			}
		}
	}

	if (!err) {
		*diffs = results;
	} else if (results) {
		array_free((void***)&results, ARRAY_GENERIC_FREE);
	}

	return err;
}

int geom_intersects(const struct geom first, const struct geom second)
{
	int top;
	int bottom;
	int left;
	int right;
	int width;
	int height;

	/*
	 *           left
	 *           |   right
	 *           |   |
	 *           v   v
	 *    +----------+
	 *    |          |
	 *    |      +---+------+ <-- top
	 *    |      |   |      |
	 *    +------+---+      | <-- bottom
	 *           |          |
	 *           +----------+
	 *
	 * This function calculated the area of the intersection of
	 * the two rectangles. If the area is zero, the rectangles
	 * do not intersect.
	 */

	top    = MAX(first.y,
	             second.y);
	bottom = MIN(first.y  + first.h,
	             second.y + second.h);
	left   = MAX(first.x,
	             second.x);
	right  = MIN(first.x  + first.w,
	             second.x + second.w);

	width  = MAX(0, right - left);
	height = MAX(0, bottom - top);

	return(width * height);
}

int geom_contains(const struct geom first, const struct geom second)
{
	return (first.x <= second.x &&
	        (first.x + first.w) >= (second.x + second.w) &&
	        first.y <= second.y &&
	        (first.y + first.h) >= (second.y + second.h));
}
