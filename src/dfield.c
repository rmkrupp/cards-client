/* File: src/dfield.c
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
#include "dfield.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>

#include <lzma.h>

/* preset to use when compressing dfield data */
constexpr uint32_t lzma_preset = 6;

/* hint to lzma for max memory to use, UINT64_MAX for no limit */
constexpr uint64_t lzma_memory_usage_limit = UINT64_MAX;

/* the magic bytes at the beginning of a dfield file */
constexpr char magic[] = { 'D', 'F' };

/* get a string representation of an error. valid forever unless result is
 * DFIELD_RESULT_ERRNO, in which case it is valid at least until the next call
 * to dfield_result_string
 */
const char * dfield_result_string(enum dfield_result result)
{
    const char * strings[] = {
        [DFIELD_RESULT_OKAY] = "no error",
        /* errno is handled specially */
        [DFIELD_RESULT_ERROR_MEMORY] =
            "memory error (malloc returned NULL)",
        [DFIELD_RESULT_ERROR_READ_SIZE] =
            "number of bytes read didn't match expected",
        [DFIELD_RESULT_ERROR_MAGIC] = "magic bytes read didn't match expected",
        [DFIELD_RESULT_ERROR_BAD_SIZE] =
            "size fields contained invalid value(s)",
        [DFIELD_RESULT_ERROR_WRITE_SIZE] =
            "number of bytes written didn't match expected",
        [DFIELD_RESULT_ERROR_BAD_INPUT_SIZE] =
            "input_height or input_width are invalid (n <= 0)",
        [DFIELD_RESULT_ERROR_BAD_OUTPUT_SIZE] =
            "output_height or output_width are invalid (n <= 0)",
        [DFIELD_RESULT_ERROR_BAD_SPREAD] =
            "spread is invalid (n <= 0 or n > 32768)",
        [DFIELD_RESULT_ERROR_LZMA] = "LZMA error",
        [DFIELD_RESULT_ERROR_BAD_DECOMPRESSED_SIZE] =
            "decompressed size doesn't match size in the header"
    };

    if (result < 0 || result > sizeof(strings) / sizeof(*strings)) {
        return "unknown error";
    }

    if (result == DFIELD_RESULT_ERROR_ERRNO) {
        return strerror(errno);
    }

    return strings[result];
}

/* load a dfield from this file and put it in dfield_out
 *
 * returns 0 on success, non-zero on error
 */
enum dfield_result dfield_from_file(
        const char * path, struct dfield * dfield_out) [[gnu::nonnull(1, 2)]]
{
    /* open the file */
    FILE * dfield_file = fopen(path, "rb");

    if (!dfield_file) {
        return DFIELD_RESULT_ERROR_ERRNO;
    }

    /* read the header */
    char magic_in[sizeof(magic)];
    size_t rd = fread(magic_in, 1, sizeof(magic), dfield_file);
    if (rd != sizeof(magic)) {
        fclose(dfield_file);
        return DFIELD_RESULT_ERROR_READ_SIZE;
    }

    for (size_t i = 0; i < sizeof(magic); i++) {
        if (magic_in[i] != magic[i]) {
            fclose(dfield_file);
            return DFIELD_RESULT_ERROR_MAGIC;
        }
    }

    int32_t size[2];
    rd = fread(size, 1, sizeof(size), dfield_file);

    if (rd != sizeof(size)) {
        fclose(dfield_file);
        return DFIELD_RESULT_ERROR_READ_SIZE;
    }

    if (size[0] <= 0 || size[1] <= 0) {
        fclose(dfield_file);
        return DFIELD_RESULT_ERROR_BAD_SIZE;
    }

    size_t buffer_size = size[0] * size[1];
    uint8_t * buffer = malloc(buffer_size);
    if (!buffer) {
        fclose(dfield_file);
        return DFIELD_RESULT_ERROR_MEMORY;
    }
    uint8_t * read_buffer = malloc(buffer_size);
    if (!read_buffer) {
        fclose(dfield_file);
        free(buffer);
        return DFIELD_RESULT_ERROR_MEMORY;
    }
    lzma_stream stream = LZMA_STREAM_INIT;
    lzma_ret ret = lzma_stream_decoder(&stream, lzma_memory_usage_limit, 0);
    if (ret != LZMA_OK) {
        free(read_buffer);
        free(buffer);
        fclose(dfield_file);
        return DFIELD_RESULT_ERROR_LZMA;
    }

    stream.next_in = NULL;
    stream.avail_in = 0;
    stream.next_out = buffer;
    stream.avail_out = buffer_size;
    lzma_action action = LZMA_RUN;

    for (;;) {
        if (stream.avail_in == 0) {
            stream.next_in = read_buffer;
            stream.avail_in = fread(read_buffer, 1, buffer_size, dfield_file);
            if (feof(dfield_file)) {
                action = LZMA_FINISH;
            }
            if (ferror(dfield_file)) {
                lzma_end(&stream);
                free(read_buffer);
                free(buffer);
                return DFIELD_RESULT_ERROR_ERRNO;
            }
        }
        ret = lzma_code(&stream, action);

        if (stream.avail_out == 0) {
            if (ret != LZMA_STREAM_END) {
                lzma_end(&stream);
                free(read_buffer);
                free(buffer);
                fclose(dfield_file);
                return DFIELD_RESULT_ERROR_LZMA;
            }
            size_t total_size = buffer_size - stream.avail_out;
            if (total_size != (size_t)size[0] * (size_t)size[1]) {
                lzma_end(&stream);
                free(read_buffer);
                free(buffer);
                fclose(dfield_file);
                return DFIELD_RESULT_ERROR_BAD_DECOMPRESSED_SIZE;
            }
            break;
        }

        if (ret != LZMA_OK) {
            lzma_end(&stream);
            free(read_buffer);
            fclose(dfield_file);
            free(buffer);
            return DFIELD_RESULT_ERROR_LZMA;
        }
    }

    lzma_end(&stream);
    free(read_buffer);

    *dfield_out = (struct dfield) {
        .width = size[0],
        .height = size[1],
        .data = (int8_t *)buffer
    };

    return DFIELD_RESULT_OKAY;
}

/* load raw data (of the sort you could pass to dfield_generate) from this file
 * and put it in data
 *
 * returns DFIELD_RESULT_OKAY (0) on success, non-zero on error
 */
enum dfield_result dfield_data_from_file(
        const char * path,
        int32_t width,
        int32_t height,
        uint8_t ** data_out
    ) [[gnu::nonnull(1, 4)]]
{
    FILE * raw_file = fopen(path, "rb");

    if (!raw_file) {
        return DFIELD_RESULT_ERROR_ERRNO;
    }

    size_t size = (size_t)width * (size_t)height;

    uint8_t * data = malloc(size);
    if (!data) {
        fclose(raw_file);
        return DFIELD_RESULT_ERROR_MEMORY;
    }

    size_t rd = fread(data, 1, size, raw_file);

    fclose(raw_file);

    if (rd != size) {
        return DFIELD_RESULT_ERROR_READ_SIZE;
    }

    *data_out = data;
    return DFIELD_RESULT_OKAY;
}

/* write this dfield to this file
 *
 * returns DFIELD_RESULT_OKAY (0) on success, non-zero on error
 */
enum dfield_result dfield_to_file(
        const char * path,
        const struct dfield * dfield
    ) [[gnu::nonnull(1, 2)]]
{
    assert(dfield->width > 0);
    assert(dfield->height > 0);

    lzma_stream stream = LZMA_STREAM_INIT;
    lzma_ret ret = lzma_easy_encoder(&stream, lzma_preset, LZMA_CHECK_CRC64);
    if (ret != LZMA_OK) {
        lzma_end(&stream);
        return DFIELD_RESULT_ERROR_LZMA;
    }

    /* open the file */
    FILE * dfield_file = fopen(path, "wb");

    if (!dfield_file) {
        lzma_end(&stream);
        return DFIELD_RESULT_ERROR_ERRNO;
    }

    /* write the header */
    size_t rd = fwrite(magic, 1, sizeof(magic), dfield_file);
    rd += fwrite(&dfield->width, 1, sizeof(dfield->width), dfield_file);
    rd += fwrite(&dfield->height, 1, sizeof(dfield->height), dfield_file);

    if (rd != sizeof(magic) + sizeof(dfield->width) + sizeof(dfield->height)) {
        lzma_end(&stream);
        fclose(dfield_file);
        return DFIELD_RESULT_ERROR_WRITE_SIZE;
    }

    /* write the compressed data */
    constexpr size_t buffer_size = 1024 * 1024;
    uint8_t * buffer = malloc(buffer_size);
    if (!buffer) {
        lzma_end(&stream);
        fclose(dfield_file);
        return DFIELD_RESULT_ERROR_MEMORY;
    }
    stream.avail_in = dfield->width * dfield->height;
    stream.next_in = (uint8_t *)dfield->data;
    stream.avail_out = buffer_size;
    stream.next_out = buffer;

    lzma_action action = LZMA_RUN;
    for (;;) {
        if (stream.avail_in == 0) {
            action = LZMA_FINISH;
        }
        ret = lzma_code(&stream, action);
        if (stream.avail_out == 0 || ret == LZMA_STREAM_END) {
            rd = fwrite(buffer, 1, buffer_size - stream.avail_out, dfield_file);
            if (rd != buffer_size - stream.avail_out) {
                lzma_end(&stream);
                free(buffer);
                fclose(dfield_file);
                return DFIELD_RESULT_ERROR_WRITE_SIZE;
            }
            break;
        }
    }
    if (ret != LZMA_STREAM_END) {
        lzma_end(&stream);
        free(buffer);
        fclose(dfield_file);
        return DFIELD_RESULT_ERROR_LZMA;
    }

    lzma_end(&stream);
    free(buffer);
    fclose(dfield_file);

    return DFIELD_RESULT_OKAY;
}

/* using this data (which should be boolean-like black and white data, with
 * 0 treated as black and all other values treated as white) generate a
 * distance field of this size with this spread value and put it in dfield_out
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
    ) [[gnu::nonnull(1)]]
{
    if (input_width <= 0 || input_height <= 0) {
        return DFIELD_RESULT_ERROR_BAD_INPUT_SIZE;
    }
    if (output_width <= 0 || output_height <= 0) {
        return DFIELD_RESULT_ERROR_BAD_OUTPUT_SIZE;
    }
    /* 2 * spread * spread must fit in an int32_t
     *
     * (it shouldn't ever be close)
     */
    if (spread < 0 || spread > 32768) {
        return DFIELD_RESULT_ERROR_BAD_SPREAD;
    }
    
    int8_t * field = malloc(output_width * output_height);
    if (!field) {
        return DFIELD_RESULT_ERROR_MEMORY;
    }

    double y_scale = (double)input_height / output_height;
    double x_scale = (double)input_width / output_width;

    #pragma omp parallel for
    for (int32_t y = 0; y < output_height; y++) {
        for (int32_t x = 0; x < output_width; x++) {
            int32_t x_in = (int32_t)lrint(x * x_scale);
            int32_t y_in = (int32_t)lrint(y * y_scale);
            bool state = data[y_in * input_width + x_in] != 0;

            int32_t minimum = INT32_MAX;
            for (int32_t i = -spread; i <= spread; i++) {
                int32_t y_in2 = y_in + i;
                if (y_in2 < 0) {
                    continue;
                }
                if (y_in2 >= input_height) {
                    break;
                }
                for (int32_t j = -spread; j <= spread; j++) {
                    int32_t x_in2 = x_in + j;
                    if (x_in2 < 0) {
                        continue;
                    }
                    if (x_in2 >= input_width) {
                        break;
                    }
                    bool state2 = data[y_in2 * input_width + x_in2] != 0;
                    if (state2 != state) {
                        int32_t dsq = i * i + j * j;
                        if (dsq < minimum) {
                            minimum = dsq;
                        }
                    }
                }
            }

            double minimum_g = sqrt(minimum);

            if (state) {
                minimum_g = -minimum_g;
            }

            minimum_g = minimum_g / spread / M_SQRT2 * 128;

            int32_t result = (int32_t)lrint(minimum_g);
            if (result > 127) {
                result = 127;
            }
            if (result < -127) {
                result = -127;
            }

            field[y * output_width + x] = (int8_t)result;
        }
    }

    *dfield_out = (struct dfield) {
        .width = output_width,
        .height = output_height,
        .data = field
    };

    return DFIELD_RESULT_OKAY;
}

/* free the data associated with a dfield
 *
 * this is equivalent to free(dfield->data)
 */
void dfield_free(struct dfield * dfield)
{
    free(dfield->data);
}
