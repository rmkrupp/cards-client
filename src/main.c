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

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "quat.h"
#include "util/sorted_set.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char ** argv)
{
    (void)argc;
    (void)argv;

    glfwInit();
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow * window = glfwCreateWindow(800, 600, "cards-client", NULL, NULL);

    /*
    uint32_t n_extensions = 0;
    vkEnumerateInstanceExtensionProperties(NULL, &n_extensions, NULL);
    printf("n_extensions = %u\n", n_extensions);
    */

    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "cards-client",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0), /* TODO */
        .pEngineName = "No Engine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_0
    };

    VkInstanceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info
    };

    /* EXTENSIONS */
    struct sorted_set * required_extensions_set = sorted_set_create();

    uint32_t glfw_extension_count = 0;
    const char ** glfw_extensions =
        glfwGetRequiredInstanceExtensions(&glfw_extension_count);

    sorted_set_add_keys_copy(
            required_extensions_set,
            glfw_extensions,
            NULL,
            NULL,
            glfw_extension_count
        );

    size_t n_extensions;
    const char ** extensions = sorted_set_flatten_keys(
            required_extensions_set, &n_extensions);

    create_info.enabledExtensionCount = n_extensions;
    create_info.ppEnabledExtensionNames = extensions;

    VkInstance instance;
    VkResult result = vkCreateInstance(&create_info, NULL, &instance);

    sorted_set_destroy(required_extensions_set);

    if (result != VK_SUCCESS) {
        fprintf(stderr, "failed to create instance\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    free(extensions);

    struct matrix matrix_a, matrix_b;
    matrix_identity(&matrix_a);
    matrix_identity(&matrix_b);
    matrix_multiply(&matrix_a, &matrix_a, &matrix_b);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
    }

    vkDestroyInstance(instance, NULL);
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
