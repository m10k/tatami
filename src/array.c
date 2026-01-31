#include "array.h"
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

static int _generic_dealloc(void **ptr)
{
	if (!ptr) {
		return -EINVAL;
	}

	free(*ptr);
	*ptr = NULL;
	return 0;
}

int array_len(void **array)
{
	int len;

	if (!array) {
		return -EINVAL;
	}

	for (len = 0; array[len]; len++);

	return len;
}

int array_add(void ***array, void **items, const int num_items)
{
	void **arr;
	int len;
	int new_len;

	if (!array) {
		return -EINVAL;
	}

	len = *array ? array_len(*array) : 0;

	if (INT_MAX - len <= num_items) {
		return -EOVERFLOW;
	}

	new_len = len + num_items;
	if (!(arr = realloc(*array, (new_len + 1) * sizeof(*arr)))) {
		return -ENOMEM;
	}

	memmove(arr + len, items, num_items * sizeof(*items));
	arr[new_len] = NULL;

	*array = arr;
	return new_len;
}

int array_remove(void ***array, const int idx, int (*dealloc)(void**))
{
	int len;
	int n;

	if (!array) {
		return -EINVAL;
	}

	len = array_len(*array);
	if (idx >= len) {
		return -ERANGE;
	}

	if (dealloc != ARRAY_DONT_FREE) {
		dealloc(&((*array)[idx]));
	}

	for (n = idx; n < len; n++) {
		(*array)[n] = (*array)[n+1];
	}

	return 0;
}

int array_free(void ***array, int (*dealloc)(void **))
{
	int i;

	if (!array) {
		return -EINVAL;
	}

	if (dealloc == ARRAY_GENERIC_FREE) {
		dealloc = _generic_dealloc;
	}

	if (*array) {
		if (dealloc != ARRAY_DONT_FREE) {
			for (i = 0; (*array)[i]; i++) {
				dealloc(&(*array)[i]);
			}
		}

		free(*array);
		*array = NULL;
	}

	return 0;
}

int array_foreach(void ***array, int (*func)(void*, void*), void *data)
{
	int err;
	int i;

	if (!array || !func) {
		return -EINVAL;
	}

	if (!*array) {
		return -ENOENT;
	}

	for (i = 0, err = 0; (*array)[i]; i++) {
		if ((err = func((*array)[i], data)) < 0) {
			break;
		}
	}

	return err;
}
