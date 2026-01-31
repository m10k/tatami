#ifndef ARRAY_H
#define ARRAY_H

#define ARRAY_DONT_FREE    ((int(*)(void**))0)
#define ARRAY_GENERIC_FREE ((int(*)(void**))1)

int array_add(void ***array, void **items, const int num_items);
int array_remove(void ***array, const int idx, int (*dealloc)(void**));
int array_free(void ***array, int (*dealloc)(void**));
int array_foreach(void ***array, int (*func)(void*, void*), void *data);
int array_len(void **array);

#endif /* ARRAY_H */
