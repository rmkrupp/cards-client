/* File: include/util/sorted_set.h
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
#ifndef UTIL_SORTED_SET_H
#define UTIL_SORTED_SET_H

#include <stddef.h>

/* a sorted set */
struct sorted_set;

/* the result of a lookup on a set */
struct sorted_set_lookup_result {
    const char * key;
    size_t length;
    void * data;
};

/* create an empty sorted set */
[[nodiscard]] struct sorted_set * sorted_set_create();

/* destroy this sorted set */
void sorted_set_destroy(struct sorted_set * sorted_set) [[gnu::nonnull(1)]];

/* destroy this sorted set without free'ing the keys */
void sorted_set_destroy_except_keys(
        struct sorted_set * sorted_set) [[gnu::nonnull(1)]];

/* return the number of keys added to this set */
size_t sorted_set_size(struct sorted_set * sorted_set) [[gnu::nonnull(1)]];

/* the result of sorted_set_add_key() */
enum sorted_set_add_key_result {
    SORTED_SET_ADD_KEY_UNIQUE, /* the key was added because it was not already
                                * present
                                */
    SORTED_SET_ADD_KEY_DUPLICATE /* the key was not added because it was
                                  * already present
                                  */
};

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
    ) [[gnu::nonnull(1, 2)]];

/* remove this key of length from the sorted set, returning the keys data
 * field, or NULL if the key is not in the set
 */
/*
void * sorted_set_remove_key(
        struct sorted_set * sorted_set,
        const char * key,
        size_t length
    ) [[gnu::nonnull(1, 2)]];
*/

/* apply this function to every key in sorted order
 *
 * the ptr passed to sorted_set_apply is passed to the callback as well
 */
void sorted_set_apply(
        struct sorted_set * sorted_set,
        void (*fn)(const char * key, size_t length, void * data, void * ptr),
        void * ptr
    ) [[gnu::nonnull(1, 2)]];

/* apply this function to every key in sorted order while destroying the
 * sorted set.
 *
 * the value of the key is passed as a non-const to the callback and must
 * either be retained or free'd as (unlike sorted_set_destroy) this function
 * does not free it when destroying the sorted_set.
 *
 * the ptr passed to sorted_set_apply is passed to the callback as well
 */
void sorted_set_apply_and_destroy(
        struct sorted_set * sorted_set,
        void (*fn)(char * key, size_t length, void * data, void * ptr),
        void * ptr
    ) [[gnu::nonnull(1, 2)]];

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
    ) [[gnu::nonnull(1, 2)]];

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
    ) [[gnu::nonnull(1)]];

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
    ) [[gnu::nonnull(1, 2)]];

/* like sorted_set_add_key but make a copy of key */
enum sorted_set_add_key_result sorted_set_add_key_copy(
        struct sorted_set * sorted_set,
        const char * key,
        size_t length,
        void * data
    ) [[gnu::nonnull(1, 2)]];

/* returns the set difference a \ b as a new sorted_set */
[[nodiscard]] struct sorted_set * sorted_set_difference(
        const struct sorted_set * a,
        const struct sorted_set * b
    ) [[gnu::nonnull(1, 2)]];

/* a sorted_set_maker
 *
 * this allows insertion of pre-sorted keys into a sorted_set in O(1) time
 * when the number of keys is known ahead of time
 */
struct sorted_set_maker;

/* create a sorted_set_maker that will make a sorted sorted with this number
 * of keys
 */
[[nodiscard]] struct sorted_set_maker * sorted_set_maker_create(
        size_t n_keys);

/* returns true if the number of keys added to this sorted_set_maker is equal
 * to the number of keys preallocated on its creation
 */
bool sorted_set_maker_complete(
        const struct sorted_set_maker * sorted_set_maker) [[gnu::nonnull(1)]];

/* finalize this sorted_set_maker, destroying it and returning the sorted_set
 * that was made
 *
 * this must be called after a number of keys have been added to the maker
 * equal to the number that were preallocated.
 */
struct sorted_set * sorted_set_maker_finalize(
        struct sorted_set_maker * sorted_set_maker) [[gnu::nonnull(1)]];

/* destroy this sorted_set_maker and any partially-constructed set inside it,
 * and free any keys
 */
void sorted_set_maker_destroy(
        struct sorted_set_maker * sorted_set_maker) [[gnu::nonnull(1)]];

/* destroy this sorted_set_maker and any partially-constructed set inside it,
 * but do not free any keys
 */
void sorted_set_maker_destroy_except_keys(
        struct sorted_set_maker * sorted_set_maker) [[gnu::nonnull(1)]];

/* add this key to this sorted_set_maker
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
    ) [[gnu::nonnull(1, 2)]];

#endif /* UTIL_SORTED_SET_H */
