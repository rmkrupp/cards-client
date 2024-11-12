/* File: src/tools/generate-dfield/args_argp.c
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
#include "tools/generate-dfield/args.h"

#include "util/strdup.h"

#include <stdlib.h>
#include <argp.h>

const char * argp_program_version =
    "generate-dfield " VERSION;

const char * argp_program_bug_address =
    "<beka.krupp@gmail.com>";

static char doc[] =
    "generate-dfield -- generate .dfield files from .dat files";

static char args_doc[] =
    "OUTPUT_FILE INPUT_FILE";

static struct argp_option options[] = {
    { "output-size", 'O', "SIZE", 0,
        "set both the width and height of the output file" },
    { "input-size", 'I', "SIZE", 0,
        "set both the width and height of the input file" },
    { "output-width", 1000, "WIDTH", 0,
        "set the width of the output file" },
    { "output-height", 1001, "HEIGHT", 0,
        "set the height of the output file" },
    { "input-width", 1002, "WIDTH", 0,
        "set the width of the input file" },
    { "input-height", 1003, "HEIGHT", 0,
        "set the height of the input file" },
    { "spread", 'S', "SPREAD", 0,
        "set the spread" },
    { }
};

static error_t parse_opt(int key, char * argv, struct argp_state * state)
{
    struct arguments * args = state->input;

    char * tmp;
    unsigned long n;

    switch (key) {
        case 'O':
            n = strtoul(argv, &tmp, 0);
            if (*tmp || n == 0 || n > INT32_MAX) {
                argp_failure(state, 1, 0, "failed to parse --output-size=%s", argv);
            }
            args->output_width = (int32_t)n;
            args->output_height = (int32_t)n;
            break;

        case 'I':
            n = strtoul(argv, &tmp, 0);
            if (*tmp || n == 0 || n > INT32_MAX) {
                argp_failure(state, 1, 0, "failed to parse --input-size=%s", argv);
            }
            args->input_width = (int32_t)n;
            args->input_height = (int32_t)n;
            break;

        case 1000:
            n = strtoul(argv, &tmp, 0);
            if (*tmp || n == 0 || n > INT32_MAX) {
                argp_failure(state, 1, 0, "failed to parse --output-width=%s", argv);
            }
            args->output_width = (int32_t)n;
            break;

        case 1001:
            n = strtoul(argv, &tmp, 0);
            if (*tmp || n == 0 || n > INT32_MAX) {
                argp_failure(state, 1, 0, "failed to parse --output-height=%s", argv);
            }
            args->output_height = (int32_t)n;
            break;

        case 1002:
            n = strtoul(argv, &tmp, 0);
            if (*tmp || n == 0 || n > INT32_MAX) {
                argp_failure(state, 1, 0, "failed to parse --input-width=%s", argv);
            }
            args->input_width = (int32_t)n;
            break;

        case 1003:
            n = strtoul(argv, &tmp, 0);
            if (*tmp || n == 0 || n > INT32_MAX) {
                argp_failure(state, 1, 0, "failed to parse --input-height=%s", argv);
            }
            args->input_height = (int32_t)n;
            break;

        case 'S':
            n = strtoul(argv, &tmp, 0);
            if (*tmp || n == 0 || n > INT32_MAX) {
                argp_failure(state, 1, 0, "failed to parse --spread=%s", argv);
            }
            args->spread = (int32_t)n;
            break;

        case ARGP_KEY_ARG:
            if (!args->output_path) {
                args->output_path = util_strdup(argv);
            } else if (!args->input_path) {
                args->input_path = util_strdup(argv);
            } else {
                argp_usage(state);
                return 1;
            }
            break;

        case ARGP_KEY_END:
            if (!args->output_path || !args->input_path) {
                argp_usage(state);
                return 1;
            }
            break;

        case ARGP_KEY_ERROR:
            free(args->output_path);
            free(args->input_path);
            break;
        default:
            return ARGP_ERR_UNKNOWN;
    }

    return 0;
}

int parse_args(
        struct arguments * args, int argc, char ** argv) [[gnu::nonnull(1)]]
{
    struct argp argp = (struct argp) {
        .options = options,
        .parser = parse_opt,
        .doc = doc,
        .args_doc = args_doc
    };

    return argp_parse(&argp, argc, argv, 0, 0, args);
}
