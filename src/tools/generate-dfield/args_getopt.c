/* File: src/tools/cards_compile/args_getopt.c
 * Part of cards <github.com/rmkrupp/cards>
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
#include "tools/cards_compile/args.h"

#include "util/strdup.h"

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

static void usage()
{
    fprintf(stderr, "Usage: generate-dfield [--help] [-O|--output-size SIZE] [-I|--input-size SIZE] [-S|--spread SIZE] OUTPUT_FILE INPUT_FILE\n");
}

static struct option options[] = {
    { "output-size", required_argument, 0, 'O' },
    { "input-size", required_argument, 0, 'I' },
    { "spread", required_argument, 0, 'S' },
    { "output-width", required_argument, 0, 1000 },
    { "output-height", required_argument, 0, 1001 },
    { "input-width", required_argument, 0, 1002 },
    { "input-height", required_argument, 0, 1003 },
    { "help", 0, 0, 2000 },
    { }
};

int parse_args(
        struct arguments * args, int argc, char ** argv) [[gnu::nonnull(1)]]
{
    while (1) {
        int index = 0;
        int c = getopt_long(argc, argv, "O:I:S:", options, &index);

        if (c == -1) {
            break;
        }

        char * tmp;
        unsigned long n;

        switch (c) {
            case 'O':
                n = strtoul(optarg, &tmp, 0);
                if (*tmp || n == 0 || n > INT32_MAX) {
                    fprintf(stderr, "failed to parser --output-size=%s", optarg);
                    return 1;
                }
                args->output_width = (int32_t)n;
                args->output_height = (int32_t)n;
                break;

            case 'I':
                n = strtoul(optarg, &tmp, 0);
                    if (*tmp || n == 0 || n > INT32_MAX) {
                        fprintf(stderr, "failed to parse --input-size=%s", optarg);
                    }
                args->input_width = (int32_t)n;
                args->input_height = (int32_t)n;
                break;

            case 1000:
                n = strtoul(optarg, &tmp, 0);
                    if (*tmp || n == 0 || n > INT32_MAX) {
                        fprintf(stderr, "failed to parse --output-width=%s", optarg);
                    }
                args->output_width = (int32_t)n;
                break;

            case 1001:
                n = strtoul(optarg, &tmp, 0);
                    if (*tmp || n == 0 || n > INT32_MAX) {
                        fprintf(stderr, "failed to parse --output-height=%s", optarg);
                    }
                args->output_height = (int32_t)n;
                break;

            case 1002:
                n = strtoul(optarg, &tmp, 0);
                    if (*tmp || n == 0 || n > INT32_MAX) {
                        fprintf(stderr, "failed to parse --input-width=%s", optarg);
                    }
                args->input_width = (int32_t)n;
                break;

            case 1003:
                n = strtoul(optarg, &tmp, 0);
                    if (*tmp || n == 0 || n > INT32_MAX) {
                        fprintf(stderr, "failed to parse --input-height=%s", optarg);
                    }
                args->input_height = (int32_t)n;
                break;

            case 'S':
                n = strtoul(optarg, &tmp, 0);
                    if (*tmp || n == 0 || n > INT32_MAX) {
                        fprintf(stderr, "failed to parse --spread=%s", optarg);
                    }
                args->spread = (int32_t)n;
                break;

            case 2000:
            case '?':
                usage();
                return 2;

            default:
                return 2;
        }
    }

    if (optind + 2 != argc) {
        usage();
        return 1;
    }

    args->output_name = util_strdup(argv[optind]);
    args->input_name = util_strdup(argv[optind + 1]);

    return 0;
}
