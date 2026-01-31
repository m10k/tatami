#ifndef COMMON_H
#define COMMON_H

struct geom {
	int x;
	int y;
	int w;
	int h;
};

typedef int client_t;
typedef int workspace_t;
typedef int monitor_t;

#define SIZEOF_ARRAY(a) (sizeof(a) / (sizeof((a)[0])))

int geom_dupe(struct geom **dst, const struct geom src);

int geom_intersection(const struct geom left,
                      const struct geom right);
int geom_contains(const struct geom left,
                  const struct geom right);
int geom_contains_xy(const struct geom geom,
                     const int x, const int y);
int geom_difference(const struct geom left,
                    const struct geom right,
                    struct geom ***diffs);

#endif /* COMMON_H */
