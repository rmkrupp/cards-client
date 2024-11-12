/* File: include/dfield.h
 *
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
#ifndef DFIELD_H
#define DFIELD_H

#include <stdint.h>

struct dfield {
    int32_t width, height;
    int8_t * data;
};

/* load a dfield from this file and put it in dfield_out
 *
 * returns 0 on success, non-zero on error
 */
int dfield_from_file(
        const char * path, struct dfield * dfield_out) [[gnu::nonnull(1, 2)]];

/* load raw data (of the sort you could pass to dfield_generate) from this file
 * and put it in data_out
 *
 * returns 0 on success, non-zero on error
 */
int dfield_data_from_file(
        const char * path,
        int32_t width,
        int32_t height,
        uint8_t ** data_out
    ) [[gnu::nonnull(1, 4)]];

/* write this dfield to this file
 *
 * returns 0 on success, non-zero on error
 */
int dfield_to_file(
        const char * path,
        const struct dfield * dfield
    ) [[gnu::nonnull(1, 2)]];


/* using this data (which should be boolean-like black and white data, with
 * 0 treated as black and all other values treated as white) generate a
 * distance field of this size with this spread value
 */
[[nodiscard]] int dfield_generate(
        uint8_t * data,
        int32_t input_width,
        int32_t input_height,
        int32_t output_width,
        int32_t output_height,
        int32_t spread,
        struct dfield * dfield_out
    ) [[gnu::nonnull(1)]];

#endif /* DFIELD_H */
