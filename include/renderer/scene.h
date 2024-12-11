/* File: include/renderer/scene.h
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
#ifndef RENDERER_SCENE_H
#define RENDERER_SCENE_H

#include "quat.h"
#include <stdint.h>
#include <stddef.h>

struct object {
    bool enabled;
    bool glows;
    struct quaternion rotation;
    float cx, cy, cz;
    float x, y, z;
    float scale;
    uint32_t solid_index,
             outline_index,
             glow_index;
};

struct camera {
    struct quaternion rotation;
    float x, y, z;
};

struct light {
    bool enabled;
    float x, y, z;
    float intensity;
    float r, g, b;
};

struct scene {
    size_t n_textures;
    const char ** texture_names;
    size_t n_objects;
    struct object * objects;
    void (*step)(struct scene * scene, double delta_time);

    struct camera camera;
    struct camera_queue * queue;
    struct camera previous_camera;

    float ambient_light;
    struct light * lights;
    size_t n_lights;
};

struct camera_queue {
    struct camera camera;
    size_t delta_time;
    struct camera_queue * next;
};

void scene_load_soho(struct scene * scene);

void scene_destroy(struct scene * scene);

#endif /* RENDERER_SCENE_H */
