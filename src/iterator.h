// Copyright 2018-2021 VMware, Inc.
// SPDX-License-Identifier: Apache-2.0

#ifndef __ITERATOR_H
#define __ITERATOR_H

#include "util.h"

/*
 * DO NOT CHANGE definitions of data types without being absolutely certain
 * because they are reflected on disk.
 * All legitimate (e.g. non-error) data types must be 0-based and dense
 * because they are used as array indexes.
 *
 * Order of types should be "newest to oldest" (points are newer than ranges)
 */
typedef enum data_type {
   data_type_point = 0,
   data_type_range = 1,
   NUM_DATA_TYPES,
   data_type_invalid,
} data_type;

typedef struct iterator iterator;

// FIXME: [nsarmicanic 2020-07-16] Might be better to return type instead of void
typedef void (*iterator_get_curr_fn)(iterator *itor, slice *key, slice *data, data_type *type);
typedef platform_status (*iterator_at_end_fn)  (iterator *itor, bool *at_end);
typedef platform_status (*iterator_advance_fn) (iterator *itor);
typedef void  (*iterator_print_fn)   (iterator *itor);

typedef struct iterator_ops {
   /* Callers should not modify data pointed to by *key or *data */
   iterator_get_curr_fn get_curr;
   iterator_at_end_fn   at_end;
   iterator_advance_fn  advance;
   iterator_print_fn    print;
} iterator_ops;

// To sub-class iterator, make an iterator your first field
struct iterator {
   const iterator_ops *ops;
};

static inline void
iterator_get_curr(iterator *itor, slice *key, slice *data, data_type *type)
{
   itor->ops->get_curr(itor, key, data, type);
}

static inline platform_status
iterator_at_end(iterator *itor, bool *at_end)
{
   return itor->ops->at_end(itor, at_end);
}

static inline platform_status
iterator_advance(iterator *itor)
{
   return itor->ops->advance(itor);
}

static inline void
iterator_print(iterator *itor)
{
   return itor->ops->print(itor);
}

#endif // __ITERATOR_H
