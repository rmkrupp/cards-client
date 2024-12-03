/* File: include/renderer/renderer.h
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
#ifndef RENDERER_RENDERER_H
#define RENDERER_RENDERER_H

#include <stdint.h>
#include <stdbool.h>

/* the result of initializing the renderer */
enum renderer_result {
    RENDERER_OKAY,
    RENDERER_ERROR
};

struct renderer_configuration {
    uint32_t max_frames_in_flight;

    /* what size of dfield to load */
    uint32_t field_size;

    /* how many antialiasing samples?
     * must be one of 1, 2, 4, 8, 16, 32, or 64 */
    uint32_t msaa_samples;

    /* whether to enable anisotropic filtering */
    bool anisotropic_filtering;

    /* whether to enable sample shading */
    bool sample_shading;

    /* texture atlas settings */
    struct atlas_configuration {
        uint32_t max_texture_width;
        uint32_t max_texture_layers;
    } atlas;
};

/* call this once per program to initialize the renderer
 *
 * after it has been called, renderer_terminate() must be called when the
 * program ends
 */
enum renderer_result renderer_init(
        const struct renderer_configuration * config);

/* call this after renderer_init() before the program ends
 *
 * it is safe to call this repeatedly, and no matter the reutrn value of
 * init()
 */
void renderer_terminate();

/* enter the event loop */
void renderer_loop();

#endif /* RENDERER_RENDERER_H */
