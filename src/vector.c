#include <stdio.h>
#include <stdlib.h>

#include "vector.h"

Vector* vector_create(void)
{
    Vector *v = malloc(sizeof(Vector));
    v->capacity = VECTOR_INIT_CAPACITY;
    v->total = 0;
    v->items = malloc(sizeof(void *) * v->capacity);
    return v;
}

int vector_total(Vector *v)
{
    return v->total;
}

static void vector_resize(Vector *v, int capacity)
{
    void **items = realloc(v->items, sizeof(void *) * capacity);
    if (items) {
        v->items = items;
        v->capacity = capacity;
    }
}

void vector_add(Vector *v, void *item)
{
    if (v->capacity == v->total)
        vector_resize(v, v->capacity * 2);
    v->items[v->total++] = item;
}

void vector_set(Vector *v, int index, void *item)
{
    if (index >= 0 && index < v->total)
        v->items[index] = item;
}

void *vector_get(Vector *v, int index)
{
    if (index >= 0 && index < v->total)
        return v->items[index];
    return NULL;
}

void* vector_delete(Vector *v, int index)
{
    void *ret = NULL;
    int i = 0;
    if (index < 0 || index >= v->total)
        return ret;

    ret = v->items[index];
    v->items[index] = NULL;

    for (i = index; i < v->total - 1; i++) {
        v->items[i] = v->items[i + 1];
        v->items[i + 1] = NULL;
    }

    v->total--;

    if (v->total > 0 && v->total == v->capacity / 4)
        vector_resize(v, v->capacity / 2);
    return ret;
}

void* vector_delete_first_equal(Vector *v, void *pattern, cmp *cmp_func)
{
    int i = 0;
    int index = -1;

    for (i = 0; i < v->total; i++) {
        if (cmp_func(v->items[i], pattern) == 0){
            index = i;
            break;
        }
    }
    return vector_delete(v, index);
}

void* vector_get_first_equal(Vector *v, void *pattern, cmp *cmp_func)
{
    int i = 0;
    int index = -1;

    for (i = 0; i < v->total; i++) {
        if (cmp_func(v->items[i], pattern) == 0){
            index = i;
            break;
        }
    }
    return vector_get(v, index);
}

int vector_is_contains(Vector *v, void *pattern, cmp *cmp_func)
{
    int i = 0;
    for (i = 0; i < v->total; i++) {
        if (cmp_func(v->items[i], pattern) == 0){
            return 1;
        }
    }
    return 0;
}

void vector_foreach(Vector *v, void *data, actions *func)
{
    int i = 0;
    for (i = 0; i < v->total; ++i) {
        fflush(stdout);
        func(v->items[i], data);
    }
}

void vector_free(Vector *v)
{
    free(v->items);
}
