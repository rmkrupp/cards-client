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

/* load a dfield from this file and put it in dfield_out
 *
 * returns 0 on success, non-zero on error
 */
int dfield_from_file(
        const char * path, struct dfield * dfield_out) [[gnu::nonnull(1, 2)]]
{
    FILE * dfield_file = fopen(path, "rb");

    if (!dfield_file) {
        return 1;
    }

    int32_t size[2];
    size_t rd = fread(size, 2, sizeof(*size), dfield_file);

    if (rd != sizeof(size)) {
        fclose(dfield_file);
        return 2;
    }

    if (size[0] <= 0 || size[1] <= 0) {
        fclose(dfield_file);
        return 3;
    }

    int8_t * buffer = malloc(size[0] * size[1]);
    rd = fread(buffer, 1, size[0] * size[1], dfield_file);

    fclose(dfield_file);

    if (rd != (size_t)size[0] * (size_t)size[1]) {
        return 4;
    }

    *dfield_out = (struct dfield) {
        .width = size[0],
        .height = size[1],
        .data = buffer
    };

    return 0;
}

/* load raw data (of the sort you could pass to dfield_generate) from this file
 * and put it in data
 *
 * returns 0 on success, non-zero on error
 */
int dfield_data_from_file(
        const char * path,
        int32_t width,
        int32_t height,
        uint8_t ** data_out
    ) [[gnu::nonnull(1, 4)]]
{
    FILE * raw_file = fopen(path, "rb");

    if (!raw_file) {
        return 1;
    }

    size_t size = (size_t)width * (size_t)height;

    uint8_t * data = malloc(size);

    size_t rd = fread(data, 1, size, raw_file);

    fclose(raw_file);

    if (rd != size) {
        return 2;
    }

    *data_out = data;
    return 0;
}

/* write this dfield to this file
 *
 * returns 0 on success, non-zero on error
 */
int dfield_to_file(
        const char * path,
        const struct dfield * dfield
    ) [[gnu::nonnull(1, 2)]]
{
    assert(dfield->width > 0);
    assert(dfield->height > 0);

    FILE * dfield_file = fopen(path, "wb");

    if (!dfield_file) {
        return 1;
    }

    size_t rd = fwrite(&dfield->width, 1, sizeof(dfield->width), dfield_file);
    rd += fwrite(&dfield->height, 1, sizeof(dfield->height), dfield_file);

    if (rd != sizeof(dfield->width) + sizeof(dfield->height)) {
        fclose(dfield_file);
        return 2;
    }

    rd = fwrite(&dfield->data, dfield->width * dfield->height, 1, dfield_file);

    fclose(dfield_file);

    if (rd != (size_t)dfield->width * (size_t)dfield->height) {
        return 3;
    }

    return 0;
}

/* using this data (which should be boolean-like black and white data, with
 * 0 treated as black and all other values treated as white) generate a
 * distance field of this size with this spread value and put it in dfield_out
 *
 * returns 0 on success, non-zero on error
 */
[[nodiscard]] int dfield_generate(
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
        return 1;
    }
    if (output_width <= 0 || output_height <= 0) {
        return 2;
    }
    /* 2 * spread * spread must fit in an int32_t
     *
     * (it shouldn't ever be close)
     */
    if (spread < 0 || spread > 32768) {
        return 3;
    }
    
    int8_t * field = malloc(output_width * output_height);

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

            minimum_g /= (spread * M_SQRT2 * 128);

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

    return 0;
}

