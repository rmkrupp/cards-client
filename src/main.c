/* File: src/main.c
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

#include "renderer/renderer.h"

#include <stdio.h>

int main(int argc, char ** argv)
{
    (void)argc;
    (void)argv;

    enum renderer_result result =
        renderer_init(
                &(struct renderer_configuration) {
                    .max_frames_in_flight = 2
                }
            );
    
    if (result) {
        return 1;
    }

    renderer_loop();

    renderer_terminate();
    return 0;
}
