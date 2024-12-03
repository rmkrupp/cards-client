/* File: src/renderer/scene.c
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

#include "renderer/scene.h"
#include <math.h>
#include <stdlib.h>

#ifndef TEXTURE_BASE_PATH
#define TEXTURE_BASE_PATH "out/data"
#endif /* TEXTURE_BASE_PATH */

void soho_step(struct scene * scene)
{
    static size_t tick;

    float r = 2.0;
    float x = r * cos((float)tick / 100.0);
    float y = 0.25;
    float z = r * sin((float)tick / 100.0);

    scene->camera.x = x;
    scene->camera.y = y;
    scene->camera.z = z;
    quaternion_from_axis_angle(
            &scene->camera.rotation, 0.0, -1.0, 0.0, atan2(x, z));

    tick++;
}

void scene_load_soho(struct scene * scene)
{
    static const char * filenames[] = {
        TEXTURE_BASE_PATH "/soho/512/front-wall-solid.dfield",
        TEXTURE_BASE_PATH "/soho/512/front-wall-outline.dfield",
        TEXTURE_BASE_PATH "/soho/512/side-wall-solid.dfield",
        TEXTURE_BASE_PATH "/soho/512/side-wall-outline.dfield",
        TEXTURE_BASE_PATH "/soho/512/roof-solid.dfield",
        TEXTURE_BASE_PATH "/soho/512/roof-outline.dfield",
        TEXTURE_BASE_PATH "/soho/512/rear-wall-solid.dfield",
        TEXTURE_BASE_PATH "/soho/512/rear-wall-outline.dfield",
        TEXTURE_BASE_PATH "/soho/512/rear-wall-interior-solid.dfield",
        TEXTURE_BASE_PATH "/soho/512/rear-wall-interior-outline.dfield",
        TEXTURE_BASE_PATH "/soho/512/front-wall-interior-solid.dfield",
        TEXTURE_BASE_PATH "/soho/512/front-wall-interior-outline.dfield",
        TEXTURE_BASE_PATH "/soho/512/roof-interior-outline.dfield",
    };
    size_t n_filenames = sizeof(filenames) / sizeof(*filenames);

    scene->texture_names = filenames;
    scene->n_textures = n_filenames;
    scene->step = &soho_step;

    scene->n_objects = 12;
    scene->objects = malloc(sizeof(*scene->objects) * scene->n_objects);

    /* object 0: the front wall */
    scene->objects[0] = (struct object) {
        .cx = 0.0,
        .cy = 0.0,
        .cz = 0.0,
        .x = 0.0,
        .y = 0.0,
        .z = 0.0,
        .scale = 1.0,
        .solid_index = 0,
        .outline_index = 1
    };
    quaternion_identity(&scene->objects[0].rotation);

    /* object 1 and 2: the side walls */
    scene->objects[1] = (struct object) {
        .cx = -0.25,
        .cy = 0.0,
        .cz = 0.0,
        .x = 0.5,
        .y = 0.0,
        .z = 0.25,
        .scale = 1.0,
        .solid_index = 2,
        .outline_index = 3
    };
    scene->objects[2] = (struct object) {
        .cx = -0.25,
        .cy = 0.0,
        .cz = 0.0,
        .x = -0.5,
        .y = 0.0,
        .z = 0.25,
        .scale = 1.0,
        .solid_index = 2,
        .outline_index = 3
    };
    quaternion_from_axis_angle(
            &scene->objects[1].rotation, 0.0, 1.0, 0.0, -M_PI / 2.0);
    quaternion_from_axis_angle(
            &scene->objects[2].rotation, 0.0, 1.0, 0.0, M_PI / 2.0);

    /* object 3 and 4: the roof */
    scene->objects[3] = (struct object) {
        .cx = -0.0,
        .cy = 0.0,
        .cz = 0.0,
        .x = 0.0,
        .y = 0.252,
        .z = 0.25,
        .scale = 1.05,
        .solid_index = 4,
        .outline_index = 5
    };
    scene->objects[4] = (struct object) {
        .cx = -0.0,
        .cy = 0.0,
        .cz = 0.0,
        .x = -0.0,
        .y = 0.252,
        .z = 0.25,
        .scale = 1.05,
        .solid_index = 4,
        .outline_index = 5
    };

    /* object 5 and 6: the inside of the roof */
    scene->objects[5] = (struct object) {
        .cx = -0.0,
        .cy = 0.0,
        .cz = 0.0,
        .x = 0.0,
        .y = 0.252,
        .z = 0.25,
        .scale = 1.05,
        .solid_index = 4,
        .outline_index = 12
    };
    scene->objects[6] = (struct object) {
        .cx = -0.0,
        .cy = 0.0,
        .cz = 0.0,
        .x = -0.0,
        .y = 0.252,
        .z = 0.25,
        .scale = 1.05,
        .solid_index = 4,
        .outline_index = 12
    };
    
    struct quaternion q_tmp;
    quaternion_from_axis_angle(
            &q_tmp, 0.0, 1.0, 0.0, M_PI);

    quaternion_from_axis_angle(
            &scene->objects[3].rotation, 1.0, 0.0, 0.0, M_PI / 4.0 * 1.0);

    quaternion_from_axis_angle(
            &scene->objects[4].rotation, 1.0, 0.0, 0.0, -M_PI / 4.0 * 1.0);
    quaternion_multiply(
            &scene->objects[4].rotation, &scene->objects[4].rotation, &q_tmp);

    quaternion_from_axis_angle(
            &scene->objects[5].rotation, 1.0, 0.0, 0.0, M_PI / 4.0 * 1.0);
    quaternion_multiply(
            &scene->objects[5].rotation, &scene->objects[5].rotation, &q_tmp);

    quaternion_from_axis_angle(
            &scene->objects[6].rotation, 1.0, 0.0, 0.0, -M_PI / 4.0 * 1.0);

    /* object 7 and 8: the rear wall */
    scene->objects[7] = (struct object) {
        .cx = -0.0,
        .cy = 0.0,
        .cz = 0.0,
        .x = 0.0,
        .y = 0.0,
        .z = 0.5,
        .scale = 1.0,
        .solid_index = 6,
        .outline_index = 7
    };
    scene->objects[8] = (struct object) {
        .cx = -0.0,
        .cy = 0.0,
        .cz = 0.0,
        .x = 0.0,
        .y = 0.0,
        .z = 0.5,
        .scale = 1.0,
        .solid_index = 8,
        .outline_index = 9
    };

    quaternion_identity(&scene->objects[7].rotation);
    quaternion_multiply(
            &scene->objects[7].rotation, &scene->objects[7].rotation, &q_tmp);

    quaternion_identity(&scene->objects[8].rotation);

    /* object 9 and 10: the side wall interiors */
    scene->objects[9] = (struct object) {
        .cx = -0.25,
        .cy = 0.0,
        .cz = 0.0,
        .x = 0.5,
        .y = 0.0,
        .z = 0.25,
        .scale = 1.0,
        .solid_index = 2,
        .outline_index = 3
    };
    scene->objects[10] = (struct object) {
        .cx = -0.25,
        .cy = 0.0,
        .cz = 0.0,
        .x = -0.5,
        .y = 0.0,
        .z = 0.25,
        .scale = 1.0,
        .solid_index = 2,
        .outline_index = 3
    };
    quaternion_from_axis_angle(
            &scene->objects[9].rotation, 0.0, 1.0, 0.0, -M_PI / 2.0);
    quaternion_multiply(
            &scene->objects[9].rotation, &scene->objects[9].rotation, &q_tmp);
    quaternion_from_axis_angle(
            &scene->objects[10].rotation, 0.0, 1.0, 0.0, M_PI / 2.0);
    quaternion_multiply(
            &scene->objects[10].rotation, &scene->objects[10].rotation, &q_tmp);

    /* object 11: the front interior wall */
    scene->objects[11] = (struct object) {
        .cx = 0.0,
        .cy = 0.0,
        .cz = 0.0,
        .x = 0.0,
        .y = 0.0,
        .z = 0.0,
        .scale = 1.0,
        .solid_index = 10,
        .outline_index = 11
    };

    quaternion_identity(&scene->objects[11].rotation);
    quaternion_multiply(
            &scene->objects[11].rotation, &scene->objects[11].rotation, &q_tmp);
}
