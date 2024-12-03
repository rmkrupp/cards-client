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

#ifndef TEXTURE_RES
#define TEXTURE_RES "512"
#endif /* TEXTURE_RES */

void soho_step(struct scene * scene)
{
    static size_t tick, camera_tick;

    (void)tick;
    /*
    float r = 2.0;
    float x = r * cos((float)tick / 100.0);
    float y = 0.25;
    float z = r * sin((float)tick / 100.0);
    */

    if (scene->queue) {

        float interp = (float)camera_tick / (float)scene->queue->delta_time;

        scene->camera.x = scene->previous_camera.x +
            interp * (scene->queue->camera.x - scene->previous_camera.x);
        scene->camera.y = scene->previous_camera.y +
            interp * (scene->queue->camera.y - scene->previous_camera.y);
        scene->camera.z = scene->previous_camera.z +
            interp * (scene->queue->camera.z - scene->previous_camera.z);

        quaternion_slerp(
                &scene->camera.rotation,
                &scene->previous_camera.rotation,
                &scene->queue->camera.rotation,
                interp
            );

        if (camera_tick == scene->queue->delta_time) {
            camera_tick = 0;
            scene->previous_camera = scene->camera;
            struct camera_queue * old = scene->queue;
            scene->queue = scene->queue->next;
            free(old);
        }

        camera_tick++;
    }

    tick++;
}

void enqueue_camera(struct scene * scene, struct camera * camera, size_t delta)
{
    struct camera_queue * node = malloc(sizeof(*node));
    *node = (struct camera_queue) {
        .camera = *camera,
        .delta_time = delta,
        .next = NULL
    };

    if (scene->queue) {
        struct camera_queue * head = scene->queue;
        while (head->next) {
            head = head->next;
        }
        head->next = node;
    } else {
        scene->queue = node;
    }
}

void scene_load_soho(struct scene * scene)
{
    static const char * filenames[] = {
        TEXTURE_BASE_PATH "/soho/" TEXTURE_RES "/front-wall-solid.dfield",
        TEXTURE_BASE_PATH "/soho/" TEXTURE_RES "/front-wall-outline.dfield",
        TEXTURE_BASE_PATH "/soho/" TEXTURE_RES "/side-wall-solid.dfield",
        TEXTURE_BASE_PATH "/soho/" TEXTURE_RES "/side-wall-outline.dfield",
        TEXTURE_BASE_PATH "/soho/" TEXTURE_RES "/roof-solid.dfield",
        TEXTURE_BASE_PATH "/soho/" TEXTURE_RES "/roof-outline.dfield",
        TEXTURE_BASE_PATH "/soho/" TEXTURE_RES "/rear-wall-solid.dfield",
        TEXTURE_BASE_PATH "/soho/" TEXTURE_RES "/rear-wall-outline.dfield",
        TEXTURE_BASE_PATH "/soho/" TEXTURE_RES "/rear-wall-interior-solid.dfield",
        TEXTURE_BASE_PATH "/soho/" TEXTURE_RES "/rear-wall-interior-outline.dfield",
        TEXTURE_BASE_PATH "/soho/" TEXTURE_RES "/front-wall-interior-solid.dfield",
        TEXTURE_BASE_PATH "/soho/" TEXTURE_RES "/front-wall-interior-outline.dfield",
        TEXTURE_BASE_PATH "/soho/" TEXTURE_RES "/roof-interior-outline.dfield",
        TEXTURE_BASE_PATH "/soho/" TEXTURE_RES "/road-solid.dfield",
        TEXTURE_BASE_PATH "/soho/" TEXTURE_RES "/road-outline.dfield",
        TEXTURE_BASE_PATH "/soho/" TEXTURE_RES "/lamp-solid.dfield",
        TEXTURE_BASE_PATH "/soho/" TEXTURE_RES "/lamp-outline.dfield",
        TEXTURE_BASE_PATH "/soho/" TEXTURE_RES "/lamp-glow.dfield",
        TEXTURE_BASE_PATH "/soho/" TEXTURE_RES "/fence-outline.dfield",
    };
    size_t n_filenames = sizeof(filenames) / sizeof(*filenames);

    scene->texture_names = filenames;
    scene->n_textures = n_filenames;
    scene->step = &soho_step;

    scene->n_objects = 28;
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

/* object 12: the road */
    scene->objects[12] = (struct object) {
        .cx = 0.0,
        .cy = 0.0,
        .cz = 0.0,
        .x = 0.0,
        .y = -0.5,
        .z = -1.0,
        .scale = 2.0,
        .solid_index = 13,
        .outline_index = 14
    };

    quaternion_from_axis_angle(
            &scene->objects[12].rotation, 1.0, 0.0, 0.0, M_PI / 2.0);
    struct quaternion q_tmp_2;
    quaternion_from_axis_angle(
            &q_tmp_2, 0.0, 0.0, 1.0, M_PI / 2);
    quaternion_multiply(
            &scene->objects[12].rotation, &scene->objects[12].rotation, &q_tmp_2);

    scene->objects[13] = (struct object) {
        .cx = 0.0,
        .cy = 0.0,
        .cz = 0.0,
        .x = -2.0,
        .y = -0.5,
        .z = -1.0,
        .scale = 2.0,
        .solid_index = 13,
        .outline_index = 14
    };

    quaternion_from_axis_angle(
            &scene->objects[13].rotation, 1.0, 0.0, 0.0, M_PI / 2.0);
    quaternion_multiply(
            &scene->objects[13].rotation, &scene->objects[13].rotation, &q_tmp_2);

    scene->objects[14] = (struct object) {
        .cx = 0.0,
        .cy = 0.0,
        .cz = 0.0,
        .x = -0.0,
        .y = -0.0,
        .z = -1.5,
        .scale = 1.0,
        .solid_index = 15,
        .outline_index = 16,
        .glow_index = 17
    };

    quaternion_identity(&scene->objects[14].rotation);

    scene->objects[15] = (struct object) {
        .cx = 0.0,
        .cy = 0.0,
        .cz = 0.0,
        .x = -0.0,
        .y = -0.0,
        .z = -1.5,
        .scale = 1.0,
        .solid_index = 15,
        .outline_index = 16,
        .glow_index = 17
    };

    quaternion_identity(&scene->objects[15].rotation);
    quaternion_multiply(
            &scene->objects[15].rotation, &scene->objects[15].rotation, &q_tmp);

    scene->objects[16] = (struct object) {
        .cx = 0.0,
        .cy = 0.0,
        .cz = 0.0,
        .x = -1.5,
        .y = -0.0,
        .z = -1.5,
        .scale = 1.0,
        .solid_index = 15,
        .outline_index = 16,
        .glow_index = 17
    };

    quaternion_identity(&scene->objects[16].rotation);

    scene->objects[17] = (struct object) {
        .cx = 0.0,
        .cy = 0.0,
        .cz = 0.0,
        .x = -1.5,
        .y = -0.0,
        .z = -1.5,
        .scale = 1.0,
        .solid_index = 15,
        .outline_index = 16,
        .glow_index = 17
    };

    quaternion_identity(&scene->objects[17].rotation);
    quaternion_multiply(
            &scene->objects[17].rotation, &scene->objects[17].rotation, &q_tmp);


    scene->objects[18] = (struct object) {
        .cx = 0.0,
        .cy = 0.0,
        .cz = 0.0,
        .x = -3.0,
        .y = -0.0,
        .z = -1.5,
        .scale = 1.0,
        .solid_index = 15,
        .outline_index = 16,
        .glow_index = 17
    };

    quaternion_identity(&scene->objects[18].rotation);

    scene->objects[19] = (struct object) {
        .cx = 0.0,
        .cy = 0.0,
        .cz = 0.0,
        .x = -3.0,
        .y = -0.0,
        .z = -1.5,
        .scale = 1.0,
        .solid_index = 15,
        .outline_index = 16,
        .glow_index = 17
    };

    quaternion_identity(&scene->objects[19].rotation);
    quaternion_multiply(
            &scene->objects[19].rotation, &scene->objects[19].rotation, &q_tmp);

    scene->objects[20] = (struct object) {
        .cx = 0.0,
        .cy = 0.0,
        .cz = 0.0,
        .x = 0.0,
        .y = -0.0,
        .z = -1.65,
        .scale = 1.0,
        .solid_index = 18,
        .outline_index = 18,
    };

    quaternion_identity(&scene->objects[20].rotation);

    scene->objects[21] = (struct object) {
        .cx = 0.0,
        .cy = 0.0,
        .cz = 0.0,
        .x = 0.0,
        .y = -0.0,
        .z = -1.65,
        .scale = 1.0,
        .solid_index = 18,
        .outline_index = 18,
    };

    quaternion_identity(&scene->objects[21].rotation);
    quaternion_multiply(
            &scene->objects[21].rotation, &scene->objects[21].rotation, &q_tmp);

    scene->objects[22] = (struct object) {
        .cx = 0.0,
        .cy = 0.0,
        .cz = 0.0,
        .x = -1.0,
        .y = -0.0,
        .z = -1.65,
        .scale = 1.0,
        .solid_index = 18,
        .outline_index = 18,
    };

    quaternion_identity(&scene->objects[22].rotation);

    scene->objects[23] = (struct object) {
        .cx = 0.0,
        .cy = 0.0,
        .cz = 0.0,
        .x = -1.0,
        .y = -0.0,
        .z = -1.65,
        .scale = 1.0,
        .solid_index = 18,
        .outline_index = 18,
    };

    quaternion_identity(&scene->objects[23].rotation);
    quaternion_multiply(
            &scene->objects[23].rotation, &scene->objects[23].rotation, &q_tmp);

    scene->objects[24] = (struct object) {
        .cx = 0.0,
        .cy = 0.0,
        .cz = 0.0,
        .x = -2.0,
        .y = -0.0,
        .z = -1.65,
        .scale = 1.0,
        .solid_index = 18,
        .outline_index = 18,
    };

    quaternion_identity(&scene->objects[24].rotation);

    scene->objects[25] = (struct object) {
        .cx = 0.0,
        .cy = 0.0,
        .cz = 0.0,
        .x = -2.0,
        .y = -0.0,
        .z = -1.65,
        .scale = 1.0,
        .solid_index = 18,
        .outline_index = 18,
    };

    quaternion_identity(&scene->objects[25].rotation);
    quaternion_multiply(
            &scene->objects[25].rotation, &scene->objects[25].rotation, &q_tmp);

    scene->objects[26] = (struct object) {
        .cx = 0.0,
        .cy = 0.0,
        .cz = 0.0,
        .x = -3.0,
        .y = -0.0,
        .z = -1.65,
        .scale = 1.0,
        .solid_index = 18,
        .outline_index = 18,
    };

    quaternion_identity(&scene->objects[26].rotation);

    scene->objects[27] = (struct object) {
        .cx = 0.0,
        .cy = 0.0,
        .cz = 0.0,
        .x = -3.0,
        .y = -0.0,
        .z = -1.65,
        .scale = 1.0,
        .solid_index = 18,
        .outline_index = 18,
    };

    quaternion_identity(&scene->objects[27].rotation);
    quaternion_multiply(
            &scene->objects[27].rotation, &scene->objects[27].rotation, &q_tmp);


    /* setup the camera */
    scene->camera = (struct camera) {
        .rotation = { 0.0, 0.0, 0.0, 1.0 },
        .x = 0.0,
        .y = 0.0,
        .z = 1.0
    };
    scene->previous_camera = scene->camera;
    quaternion_from_axis_angle(
            &scene->camera.rotation, 0.0, 1.0, 0.0, M_PI / 2);

    struct quaternion q_final;
    quaternion_from_axis_angle(&q_final, 0.0, 1.0, 0.0, -M_PI / 2);

    struct quaternion q_final_2;
    quaternion_from_axis_angle(&q_final_2, 0.0, 1.0, 0.0, M_PI / 2);


    enqueue_camera(scene,
            &(struct camera) {
                .rotation = q_final,
                .x = 3.0,
                .y = 0.0,
                .z = 1.0
            },
            360
        );
    enqueue_camera(scene,
            &(struct camera) {
                .rotation = q_final,
                .x = 3.0,
                .y = 0.0,
                .z = 1.0
            },
            180
        );
    enqueue_camera(scene,
            &(struct camera) {
                .rotation = q_final,
                .x = 0.0,
                .y = 0.0,
                .z = 1.0
            },
            360
        );
    enqueue_camera(scene,
            &(struct camera) {
                .rotation = q_final_2,
                .x = 0.0,
                .y = 0.0,
                .z = 1.0
            },
            360
        );
    enqueue_camera(scene,
            &(struct camera) {
                .rotation = q_final_2,
                .x = 0.0,
                .y = 0.0,
                .z = 1.0
            },
            90
        );
    enqueue_camera(scene,
            &(struct camera) {
                .rotation = q_final_2,
                .x = 3.0,
                .y = 0.0,
                .z = 1.0
            },
            360
        );
}

void scene_destroy(struct scene * scene)
{
    free(scene->objects);
    if (scene->queue) {
        struct camera_queue * next = scene->queue->next;
        free(scene->queue);
        while (next) {
            struct camera_queue * prev = next;
            next = next->next;
            free(prev);
        }
    }
}

