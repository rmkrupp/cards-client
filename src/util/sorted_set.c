/* File: src/util/sorted_set.h
 * Part of cards-client <github.com/rmkrupp/cards-client>
 *
 * Copyright (C) 2024 Noah Santer <n.ed.santer@gmail.com>
 * Copyright (C) 2024 Rebecca Krupp <beka.krupp@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "util/sorted_set.h"
#include "util/strdup.h"

#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

/* a sorted set */
struct sorted_set {
    struct node ** next;
    size_t layers;
    size_t size;
};

/* a node in the skip list */
struct node {
    struct node ** next; /* this property must line up with sorted_set because
                          * we cast sorted_set into node in the add function
                          *
                          * only next is used from this, though.
                          */

    /* these three properties must stay in this order because we cast a pointer
     * to the display_key field to a struct sorted_set_lookup_result pointer
     */
    char * key;
    size_t length;
    void * data;
};

/* create an empty sorted set */
[[nodiscard]] struct sorted_set * sorted_set_create()
{
    struct sorted_set * sorted_set = malloc(sizeof(*sorted_set));
    *sorted_set = (struct sorted_set) {
        .next = malloc(sizeof(*sorted_set->next)),
        .layers = 1
    };
    sorted_set->next[0] = NULL;
    return sorted_set;
}

/* destroy this sorted set */
void sorted_set_destroy(struct sorted_set * sorted_set) [[gnu::nonnull(1)]]
{
    if (sorted_set->layers > 0) {
        struct node * node = sorted_set->next[0];
        while (node) {
            struct node * next = node->next[0];
            free(node->key);
            free(node->next);
            free(node);
            node = next;
        }
        free(sorted_set->next);
    }
    free(sorted_set);
}

/* destroy this sorted set without free'ing the keys */
void sorted_set_destroy_except_keys(
        struct sorted_set * sorted_set) [[gnu::nonnull(1)]]
{
    if (sorted_set->layers > 0) {
        struct node * node = sorted_set->next[0];
        while (node) {
            struct node * next = node->next[0];
            free(node->next);
            free(node);
            node = next;
        }
        free(sorted_set->next);
    }
    free(sorted_set);
}

/* return the number of keys added to this set */
size_t sorted_set_size(struct sorted_set * sorted_set) [[gnu::nonnull(1)]]
{
    return sorted_set->size;
}

/* returns 0 when equal, negative when a < b, positive when a > b */
static int key_compare(const struct node * a, const struct node * b)
{
    size_t length = a->length > b->length ? a->length : b->length;
    for (size_t i = 0; i < length; i++) {
        if (a->key[i] != b->key[i]) {
            return (int)a->key[i] - (int)b->key[i];
        }
    }
    return 0;
}

/* start at 1. do forever { if 50% chance: increase it, otherwise stop } */
static size_t random_level()
{
    size_t level = 1;
    for (;;) {
        uint32_t x = rand();
        /* RAND_MAX is at least (1<<15)-1 */
        for (size_t i = 0; i < 15; i++) {
            if (x & 1) {
                return level;
            }
            x >>= 1;
            level++;
        }
    }
    /* you know, if you win the lottery, this could overflow the size_t.
     * it would take a long time, though.
     */
}

/* add this key of length to the sorted set, associating it with data
 *
 * if length is zero, the key must be null terminated and its length is
 * calculated automatically.
 *
 * if the key is added (i.e. if it is not a duplicate of a key currently in the
 * set), the sorted_set takes ownership of this memory. do not free it after
 * calling this function, unless the memory is extracted via an
 * apply_and_destroy or transformation into a hash (and then from the hash.)
 *
 * note that this function operates on a char * because it is designed to be
 * called on the result of u8_normxfrm() being called on a uint8_t *. as far
 * as this function (and the sorted set) is concerned, key is just a block of
 * bytes of a given length where we can compare the contents of individual
 * bytes with <, ==, and > and get consistent results.
 *
 * returns SORTED_SET_ADD_KEY_UNIQUE if the key was not already in the set,
 * or SORTED_SET_ADD_KEY_DUPLICATE otherwise
 */
enum sorted_set_add_key_result sorted_set_add_key(
        struct sorted_set * sorted_set,
        char * key,
        size_t length,
        void * data
    ) [[gnu::nonnull(1, 2)]]
{
    if (length == 0) {
        length = strlen(key);
    }

    /* pick a random height for the new nodde */
    size_t new_level = random_level();

    /* trailing nodes */
    struct node ** update = malloc(sizeof(*update) * sorted_set->layers);
    for (size_t i = new_level; i < sorted_set->layers; i++) {
        update[i] = NULL;
    }

    /* locate where new node should go */
    size_t layer = sorted_set->layers - 1;
    struct node * node = (struct node *)sorted_set;

    for (;;) {
        while (node->next[layer]) {
            int compare = key_compare(
                    &(struct node) { .key = key, .length = length },
                    node->next[layer]
                );
            if (compare == 0) {
                /* key is a duplicate */
                free(update);
                return SORTED_SET_ADD_KEY_DUPLICATE;
            }

            if (compare > 0) {
                node = node->next[layer];
            }

            if (compare < 0) {
                break;
            }
        }

        update[layer] = node;

        if (layer == 0) {
            break;
        }
        layer--;
    }

    /* at this point we know the key isn't a duplicate */
    sorted_set->size++;

    struct node * new_node = malloc(sizeof(*new_node));
    *new_node = (struct node) {
        .key = key,
        .length = length,
        .data = data,
        .next = malloc(sizeof(*new_node->next) * new_level)
    };

    /* update the nexts of update[] and set new_node's nexts */
    if (new_level <= sorted_set->layers) {
        for (size_t i = 0; i < new_level; i++) {
            new_node->next[i] = update[i]->next[i];
            update[i]->next[i] = new_node;
        }
    } else {
        for (size_t i = 0; i < sorted_set->layers; i++) {
            new_node->next[i] = update[i]->next[i];
            update[i]->next[i] = new_node;
        }

        sorted_set->next = realloc(
                sorted_set->next,
                sizeof(*sorted_set->next) * new_level
            );

        for (size_t i = sorted_set->layers; i < new_level; i++) {
            sorted_set->next[i] = new_node;
            new_node->next[i] = NULL;
        }

        sorted_set->layers = new_level;
    }

    free(update);

    return SORTED_SET_ADD_KEY_UNIQUE;
}

/* add a copy of each of these keys to the sorted_set
 *
 * n_keys is the number of keys
 *
 * if lengths is not NULL, it must be the same length as keys and contain
 * the lengths of each key; otherwise, the keys are assumed to be
 * null-terminated
 *
 * if data is not NULL, it must be the same length as keys and contain the
 * data to be associated with them; otherwise, they all have NULL data
 */
void sorted_set_add_keys_copy(
        struct sorted_set * sorted_set,
        const char ** keys,
        size_t * lengths,
        void ** data,
        size_t n_keys
    ) [[gnu::nonnull(1, 2)]]
{
    for (size_t i = 0; i < n_keys; i++) {
        size_t length;
        if (lengths) {
            length = lengths[i];
        } else {
            length = strlen(keys[i]);
        }
        sorted_set_add_key(
                sorted_set,
                util_strndup(keys[i], length),
                length,
                data ? data[i] : NULL
            );
    }
}

/* like sorted_set_add_key, but make a copy of the key first */
enum sorted_set_add_key_result sorted_set_add_key_copy(
        struct sorted_set * sorted_set,
        const char * key,
        size_t length,
        void * data
    ) [[gnu::nonnull(1, 2)]]
{
    if (length) {
        return sorted_set_add_key(
                sorted_set, util_strndup(key, length), length, data);
    } else {
        return sorted_set_add_key(sorted_set, strdup(key), length, data);
    }
}

/* apply this function to every key in sorted order
 *
 * the ptr passed to sorted_set_apply is passed to the callback as well
 *
 * this does an in-order traversal. we preallocate a stack of size
 * sorted_set->depth_hint + 1 and also update that depth_hint after traversal
 * to the confirmed value.
 *
 * TODO could add a "stop traversal" return from fn
 */
void sorted_set_apply(
        struct sorted_set * sorted_set,
        void (*fn)(
            const char * key,
            size_t length,
            void * data,
            void * ptr
        ),
        void * ptr
    ) [[gnu::nonnull(1, 2)]]
{
    if (sorted_set->layers == 0) {
        return;
    }

    struct node * node = sorted_set->next[0];
    while (node) {
        fn(node->key, node->length, node->data, ptr);
        node = node->next[0];
    }
}

/* apply this function to every key in sorted order while destroying the
 * sorted set.
 *
 * the value of the key is passed as a non-const to the callback and must
 * either be retained or free'd as (unlike sorted_set_destroy) this function
 * does not free it when destroying the sorted_set.
 *
 * the ptr passed to sorted_set_apply is passed to the callback as well.
 *
 * TODO could add a "stop traversal" return from fn
 */
void sorted_set_apply_and_destroy(
        struct sorted_set * sorted_set,
        void (*fn)(
            char * key,
            size_t length,
            void * data,
            void * ptr
        ),
        void * ptr
    ) [[gnu::nonnull(1, 2)]]
{
    if (sorted_set->layers == 0) {
        return;
    }

    struct node * node = sorted_set->next[0];
    while (node) {
        fn(node->key, node->length, node->data, ptr);

        struct node * next = node->next[0];
        free(node->next);
        free(node);

        node = next;
    }

    free(sorted_set->next);
    free(sorted_set);
}

/* find this key in the sorted set and return a const pointer to it, or NULL
 * if it's not in the set
 *
 * this function does not take ownership of key
 *
 * see sorted_set_add for why this is a char * and not a uint8_t *
 */
const struct sorted_set_lookup_result * sorted_set_lookup(
        struct sorted_set * sorted_set,
        const char * key,
        size_t length
    ) [[gnu::nonnull(1, 2)]]
{
    size_t layer = sorted_set->layers - 1;
    struct node * node = (struct node *)sorted_set;

    for (;;) {
        while (node->next[layer]) {
            int compare = key_compare(
                    /* casting away the const is fine, key_compare doesn't
                     * modify the node
                     */
                    &(struct node) { .key = (char *)key, .length = length },
                    node->next[layer]
                );
            if (compare == 0) {
                return (const struct sorted_set_lookup_result *)
                    &node->next[layer]->key;
            }

            if (compare > 0) {
                node = node->next[layer];
            }

            if (compare < 0) {
                break;
            }
        }

        if (layer == 0) {
            break;
        }
        layer--;
    }

    return NULL;
}

/* remove this key of length from the sorted set, returning the keys data
 * field, or NULL if the key is not in the set
 */
/*
void * sorted_set_remove_key(
        struct sorted_set * sorted_set,
        const uint8_t * key,
        size_t length
    ) [[gnu::nonnull(1, 2)]]
{
    size_t layer = sorted_set->layers - 1;
    struct node * node = (struct node *)sorted_set;

    struct node ** update = malloc(sizeof(*update) * sorted_set->layers);

    for (;;) {
        while (node->next[layer]) {
            int compare = key_compare(
                    &(struct node) { .key = (char *)key, .length = length },
                    node->next[layer]
                );
            if (compare == 0) {
                for (size_t i = 0; i < layer; i++) {
                    update[i]->next[i] = node->next[layer]->next[i];
                }
                free(update);
                void * data = node->next[layer]->data;
                free(node->next);
                free(node->key);
                free(node);
                return data;
            }

            if (compare > 0) {
                node = node->next[layer];
            }

            if (compare < 0) {
                break;
            }
        }

        update[layer] = node;

        if (layer == 0) {
            break;
        }
        layer--;
    }

    return NULL;
}
*/

/* flatten this set into an array
 *
 * returns the array of pointers into the set (thus the array is not valid
 * after the sorted_set has been destroyed.)
 *
 * if n_keys is not null, store the size of the resulting array into it (this
 * is the same value as sorted_set_size()
 */
[[nodiscard]] const char ** sorted_set_flatten_keys(
        const struct sorted_set * sorted_set,
        size_t * n_keys_out
    ) [[gnu::nonnull(1)]]
{
    struct node * node = sorted_set->next[0];
    const char ** keys = malloc(sizeof(*keys) * sorted_set->size);
    for (size_t i = 0; i < sorted_set->size; i++) {
        keys[i] = node->key;
        node = node->next[0];
    }
    if (n_keys_out) {
        *n_keys_out = sorted_set->size;
    }
    return keys;
}

/* returns the set difference a \ b as a new sorted_set */
[[nodiscard]] struct sorted_set * sorted_set_difference(
        const struct sorted_set * a,
        const struct sorted_set * b
    ) [[gnu::nonnull(1, 2)]]
{
    struct node * a_node = a->next[0],
                * b_node = b->next[0];

    struct sorted_set * out = sorted_set_create();

    while (a_node && b_node) {
        int result = key_compare(a_node, b_node) == 0;
        if (result == 0) {
            /* in a and b, not in result */
            a_node = a_node->next[0];
            b_node = b_node->next[0];
        } else if (result < 0) {
            /* in a and not in b, in result */
            sorted_set_add_key_copy(
                    out, a_node->key, a_node->length, a_node->data);
        } else {
            /* potentially in b, look at next b node */
            b_node = b_node->next[0];
        }
    }

    while (a_node) {
        /* the remaining a nodes are all in result */
        sorted_set_add_key_copy(
                out, a_node->key, a_node->length, a_node->data);
        a_node = a_node->next[0];
    }

    return out;
}

/* a sorted_set_maker
 *
 * this allows insertion of pre-sorted keys into a sorted_set in O(1) time
 * when the number of keys is known ahead of time
 */
struct sorted_set_maker {
    struct sorted_set * sorted_set; /* the embedded sorted_set */
    struct node * next; /* where will the next added key go? */
};

/* create a sorted_set_maker that will make a sorted sorted with this number
 * of keys
 *
 * it is an error to call this with n_keys == 0
 */
[[nodiscard]] struct sorted_set_maker * sorted_set_maker_create(
        size_t n_keys)
{
    assert(n_keys > 0);

    struct sorted_set_maker * sorted_set_maker =
        malloc(sizeof(*sorted_set_maker));

    *sorted_set_maker = (struct sorted_set_maker) {
        .sorted_set = malloc(sizeof(*sorted_set_maker->sorted_set))
    };

    struct sorted_set * sorted_set = sorted_set_maker->sorted_set;

    size_t layers = 0;
    for (size_t n = n_keys; n > 1; n /= 2) {
        layers++;
    }

    size_t * landmarks = malloc(sizeof(*landmarks) * layers);
    size_t i = layers - 1;
    for (size_t n = n_keys / 2; n > 1; n /= 2) {
        landmarks[i] = n;
        i--;
    }

    sorted_set->layers = layers;
    sorted_set->next = malloc(sizeof(*sorted_set->next) * layers);
    sorted_set->size = 0;

    struct node ** update = malloc(sizeof(*update) * layers);
    for (size_t i = 0; i < layers; i++) {
        update[i] = (struct node *)sorted_set;
    }

    for (size_t i = 0; i < n_keys; i++) {
        size_t layer = 0;
        for (size_t j = layers - 1; j > 0; j--) {
            if (i % landmarks[j] == 0) {
                layer = j;
                break;
            }
        }
        layer++;

        struct node * node = malloc(sizeof(*node));
        *node = (struct node) {
            .next = malloc(sizeof(*node->next) * (layer))
        };

        for (size_t j = 0; j < layer; j++) {
            update[j]->next[j] = node;
            update[j] = node;
        }
    }

    for (size_t i = 0; i < layers; i++) {
        update[i]->next[i] = NULL;
    }

    sorted_set_maker->next = sorted_set->next[0];
    free(update);
    free(landmarks);
    return sorted_set_maker;
}

/* returns true if the number of keys added to this sorted_set_maker is equal
 * to the number of keys preallocated on its creation
 */
bool sorted_set_maker_complete(
        const struct sorted_set_maker * sorted_set_maker) [[gnu::nonnull(1)]]
{
    return !sorted_set_maker->next;
}

/* finalize this sorted_set_maker, destroying it and returning the sorted_set
 * that was made
 *
 * this must be called after a number of keys have been added to the maker
 * equal to the number that were preallocated.
 */
struct sorted_set * sorted_set_maker_finalize(
        struct sorted_set_maker * sorted_set_maker) [[gnu::nonnull(1)]]
{
    assert(sorted_set_maker_complete(sorted_set_maker));
    struct sorted_set * sorted_set = sorted_set_maker->sorted_set;
    free(sorted_set_maker);
    return sorted_set;
}

/* destroy this sorted_set_maker and any partially-constructed set inside it,
 * and free any keys
 */
void sorted_set_maker_destroy(
        struct sorted_set_maker * sorted_set_maker) [[gnu::nonnull(1)]]
{
    struct node * node = sorted_set_maker->sorted_set->next[0];
    while (node != sorted_set_maker->next) {
        struct node * next = node->next[0];
        free(node->next);
        free(node->key);
        free(node);
        node = next;
    }
    while (node) {
        struct node * next = node->next[0];
        free(node->next);
        free(node);
        node = next;
    }
    free(sorted_set_maker->sorted_set->next);
    free(sorted_set_maker->sorted_set);
    free(sorted_set_maker);
}

/* destroy this sorted_set_maker and any partially-constructed set inside it,
 * but do not free any keys
 */
void sorted_set_maker_destroy_except_keys(
        struct sorted_set_maker * sorted_set_maker) [[gnu::nonnull(1)]]
{
    struct node * node = sorted_set_maker->sorted_set->next[0];
    while (node) {
        struct node * next = node->next[0];
        free(node->next);
        free(node);
        node = next;
    }
    free(sorted_set_maker->sorted_set->next);
    free(sorted_set_maker->sorted_set);
    free(sorted_set_maker);
}

/* add this key to this sorted_set_maker
 *
 * this takes ownership of key
 *
 * see sorted_set_add_key for why key is a char * and not a uint8_t *
 *
 * returns true if the sorted_set_maker is now complete
 *
 * it is an error to call this on a complete sorted_set_maker
 */
bool sorted_set_maker_add_key(
        struct sorted_set_maker * sorted_set_maker,
        char * key,
        size_t length,
        void * data
    ) [[gnu::nonnull(1, 2)]]
{
    sorted_set_maker->next->key = key;
    sorted_set_maker->next->length = length;
    sorted_set_maker->next->data = data;
    sorted_set_maker->next = sorted_set_maker->next->next[0];
    return !sorted_set_maker->next;
}
