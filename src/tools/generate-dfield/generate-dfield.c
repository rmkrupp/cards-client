/* File: src/tools/generate-dfield.c
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
#include "tools/generate-dfield/args.h"

#include <stdio.h>
#include <stdlib.h>

static void free_args(struct arguments * args)
{
    free(args->input_path);
    free(args->output_path);
}

int main(int argc, char ** argv)
{
    struct arguments args = { };

    int parse_result;
    if ((parse_result = parse_args(&args, argc, argv))) {
        free_args(&args);
        return parse_result;
    }

    if (args.input_width == 0 || args.input_height == 0) {
        fprintf(stderr, "input size not specified (no default)\n");
        free_args(&args);
        return 1;
    }

    if (args.output_width == 0 || args.output_height == 0) {
        fprintf(stderr, "output size not specified (no default)\n");
        free_args(&args);
        return 1;
    }

    if (args.spread == 0) {
        fprintf(stderr, "spread not specified (no default)\n");
        free_args(&args);
        return 1;
    }

    uint8_t * data;
    int result;
    if ((result = dfield_data_from_file(
                args.input_path,
                args.input_width,
                args.input_height,
                &data))) {
        fprintf(
                stderr,
                "error %d reading input data from file %s\n",
                result,
                args.input_path
            );
        free_args(&args);
        return 1;
    }

    struct dfield dfield;
    if ((result = dfield_generate(
                    data,
                    args.input_width,
                    args.input_height,
                    args.output_width,
                    args.output_height,
                    args.spread,
                    &dfield))) {
        fprintf(stderr, "error %d generating dfield\n", result);
        free(data);
        free_args(&args);
        return 1;
    }

    free(data);

    if ((result = dfield_to_file(args.output_path, &dfield))) {
        fprintf(
                stderr,
                "error %d writing dfield to file %s\n",
                result,
                args.output_path
            );
        free(dfield.data);
        free_args(&args);
        return 1;
    }

    free(dfield.data);
    free_args(&args);
    return 0;
}
