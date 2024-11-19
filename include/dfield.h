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

/* a signed distance field */
struct dfield {
    int32_t width, height;
    int8_t * data;
};

/* the result of the operations in this file */
enum dfield_result {
    DFIELD_RESULT_OKAY = 0,

    DFIELD_RESULT_ERROR_ERRNO, /* error, check errno */
    DFIELD_RESULT_ERROR_MEMORY, /* malloc returned NULL
                                   note that we check this because of the
                                   externally-derived allocation sizes implied
                                   by the size fields in the heaader */
    DFIELD_RESULT_ERROR_READ_SIZE, /* bytes read didn't match expected */
    DFIELD_RESULT_ERROR_MAGIC, /* magic bytes didn't match expected */
    DFIELD_RESULT_ERROR_BAD_SIZE, /* size fields contained invalid value */
    DFIELD_RESULT_ERROR_WRITE_SIZE, /* bytes written didn't match expected */
    
    DFIELD_RESULT_ERROR_BAD_INPUT_SIZE, /* values passed for input_height or
                                         * input_width are invalid
                                         */
    DFIELD_RESULT_ERROR_BAD_OUTPUT_SIZE, /* values passed for output_height or
                                          * output_width are invalid
                                          */
    DFIELD_RESULT_ERROR_BAD_SPREAD, /* value passed for spread is invalid */

    DFIELD_RESULT_ERROR_LZMA, /* error with lzma library */
    DFIELD_RESULT_ERROR_BAD_DECOMPRESSED_SIZE /* post-decompression size
                                               * doesn't match the header
                                               */
};

/* get a string representation of an error. valid forever unless result is
 * DFIELD_RESULT_ERRNO, in which case it is valid at least until the next call
 * to dfield_result_string
 */
const char * dfield_result_string(enum dfield_result result);

/* load a dfield from this file and put it in dfield_out
 *
 * returns DFIELD_RESULT_OKAY (0) on success, non-zero on error
 */
enum dfield_result dfield_from_file(
        const char * path, struct dfield * dfield_out) [[gnu::nonnull(1, 2)]];

/* load raw data (of the sort you could pass to dfield_generate) from this file
 * and put it in data_out
 *
 * returns DFIELD_RESULT_OKAY (0) on success, non-zero on error
 */
enum dfield_result dfield_data_from_file(
        const char * path,
        int32_t width,
        int32_t height,
        uint8_t ** data_out
    ) [[gnu::nonnull(1, 4)]];

/* write this dfield to this file
 *
 * returns DFIELD_RESULT_OKAY (0) on success, non-zero on error
 */
enum dfield_result dfield_to_file(
        const char * path,
        const struct dfield * dfield
    ) [[gnu::nonnull(1, 2)]];

/* using this data (which should be boolean-like black and white data, with
 * 0 treated as black and all other values treated as white) generate a
 * distance field of this size with this spread value
 *
 * returns DFIELD_RESULT_OKAY (0) on success, non-zero on error
 */
[[nodiscard]] enum dfield_result dfield_generate(
        uint8_t * data,
        int32_t input_width,
        int32_t input_height,
        int32_t output_width,
        int32_t output_height,
        int32_t spread,
        struct dfield * dfield_out
    ) [[gnu::nonnull(1)]];

/* free the data associated with a dfield
 *
 * this is equivalent to free(dfield->data)
 */
void dfield_free(struct dfield * dfield);

#endif /* DFIELD_H */
