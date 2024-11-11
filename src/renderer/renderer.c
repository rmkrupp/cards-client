/* File: src/renderer/renderer.c
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

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "util/sorted_set.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct renderer {

    bool initialized; /* true if renderer_init() returned OKAY and 
                       * renderer_terminate() hasn't been called
                       */

    bool glfw_needs_terminate; /* did we call glfwInit()? */
    GLFWwindow * window; /* the window */

    VkInstance instance; /* the instance */

    VkPhysicalDevice physical_device; /* the physical device */
    VkDevice device; /* the logical device */

    size_t n_layers;
    const char ** layers;

    struct queue_families {
        struct queue_family {
            uint32_t index; /* the index of each family */
            bool exists; /* whether the family exists */
        } graphics; /* the graphics queue family */
    } queue_families; /* the queue families */

    VkQueue graphics_queue; /* the graphics queue */

} renderer = { };

/* initialize GLFW and create a window */
static enum renderer_init_result setup_glfw()
{
    glfwInit();
    renderer.glfw_needs_terminate = true;

    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    renderer.window = glfwCreateWindow(800, 600, "cards-client", NULL, NULL);

    if (!renderer.window) {
        fprintf(stderr, "[renderer] glfwCreateWindow() failed\n");
        renderer_terminate();
        return RENDERER_INIT_ERROR;
    }

    return RENDERER_INIT_OKAY;
}

/* callback for setup_instance() */
static void print_missing_layers(
        const char * key, size_t length, void * data, void * ptr)
{
    (void)length;
    (void)data;
    (void)ptr;
    fprintf(stderr, "[renderer] missing layer %s\n", key);
}

/* initialize the vulkan instance and extensions */
static enum renderer_init_result setup_instance()
{
    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "cards-client",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0), /* TODO */
        .pEngineName = "No Engine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_0
    };

    struct sorted_set * extensions_set = sorted_set_create();

    /* extensions required by GLFW */
    uint32_t glfw_extension_count = 0;
    const char ** glfw_extensions =
        glfwGetRequiredInstanceExtensions(&glfw_extension_count);

    sorted_set_add_keys_copy(
            extensions_set,
            glfw_extensions,
            NULL,
            NULL,
            glfw_extension_count
        );

    size_t n_extensions;
    const char ** extensions = sorted_set_flatten_keys(
            extensions_set, &n_extensions);

    struct sorted_set * layers_set = sorted_set_create();

#if !NDEBUG
    sorted_set_add_key_copy(
            layers_set, "VK_LAYER_KHRONOS_validation", 0, NULL);
#endif /* NDEBUG */

    struct sorted_set * available_layers_set = sorted_set_create();
    uint32_t n_available_layers;
    vkEnumerateInstanceLayerProperties(&n_available_layers, NULL);
    VkLayerProperties * available_layers =
        malloc(sizeof(*available_layers) * n_available_layers);
    vkEnumerateInstanceLayerProperties(&n_available_layers, available_layers);

    for (size_t i = 0; i < n_available_layers; i++) {
        sorted_set_add_key_copy(
                available_layers_set, available_layers[i].layerName, 0, NULL);
    }

    struct sorted_set * missing_layers =
        sorted_set_difference(layers_set, available_layers_set);

    if (sorted_set_size(missing_layers)) {
        sorted_set_apply(missing_layers, &print_missing_layers, NULL);
        sorted_set_destroy(missing_layers);
        sorted_set_destroy(layers_set);
        sorted_set_destroy(available_layers_set);
        renderer_terminate();
        return RENDERER_INIT_ERROR;
    }
    free(available_layers);
    sorted_set_destroy(available_layers_set);
    sorted_set_destroy(missing_layers);

    renderer.layers = sorted_set_flatten_keys(layers_set, &renderer.n_layers);

    VkInstanceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledExtensionCount = n_extensions,
        .ppEnabledExtensionNames = extensions,
        .enabledLayerCount = renderer.n_layers,
        .ppEnabledLayerNames = renderer.layers
    };

    VkResult result = vkCreateInstance(&create_info, NULL, &renderer.instance);

    free(extensions);
    sorted_set_destroy(extensions_set);

    sorted_set_destroy(layers_set);

    if (result != VK_SUCCESS) {
        fprintf(stderr, "[renderer] vkCreateInstance() failed\n");
        renderer_terminate();
        return RENDERER_INIT_ERROR;
    }

    return RENDERER_INIT_OKAY;
}

/* pick a physical device */
static enum renderer_init_result setup_physical_device()
{
    uint32_t n_devices;
    vkEnumeratePhysicalDevices(renderer.instance, &n_devices, NULL);
    if (n_devices == 0) {
        fprintf(stderr, "[renderer] no devices have Vulkan support\n");
        renderer_terminate();
        return RENDERER_INIT_ERROR;
    }

    VkPhysicalDevice * devices = malloc(sizeof(*devices) * n_devices);
    vkEnumeratePhysicalDevices(renderer.instance, &n_devices, devices);

    VkPhysicalDevice candidate = NULL;
    /* find a suitable device */
    /* TODO: user override */
    for (size_t i = 0; i < n_devices; i++) {
        VkPhysicalDeviceProperties device_properties;
        vkGetPhysicalDeviceProperties(devices[i], &device_properties);
        /*
        VkPhysicalDeviceFeatures device_features;
        vkGetPhysicalDeviceFeatures(devices[i], &device_features);
        */
        fprintf(
                stdout,
                "[renderer] found device %s\n",
                device_properties.deviceName
            );

        if (device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            candidate = devices[i];
        } else if (!candidate) {
            candidate = devices[i];
        }
    }
    free(devices);

    VkPhysicalDeviceProperties device_properties;
    vkGetPhysicalDeviceProperties(candidate, &device_properties);
    fprintf(
            stdout,
            "[renderer] picked device %s (discrete: %s)\n",
            device_properties.deviceName,
            device_properties.deviceType ==
                VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? "true" : "false"
       );

    renderer.physical_device = candidate;

    return RENDERER_INIT_OKAY;
}

enum renderer_init_result setup_queue_families()
{
    uint32_t n_queue_families;
    vkGetPhysicalDeviceQueueFamilyProperties(
            renderer.physical_device, &n_queue_families, NULL);
    VkQueueFamilyProperties * queue_families =
        malloc(sizeof(*queue_families) * n_queue_families);
    vkGetPhysicalDeviceQueueFamilyProperties(
            renderer.physical_device, &n_queue_families, queue_families);

    for (size_t i = 0; i < n_queue_families; i++) {
        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            renderer.queue_families.graphics.index = i;
            renderer.queue_families.graphics.exists = true;
        }
    }

    free(queue_families);

    if (!renderer.queue_families.graphics.exists) {
        fprintf(
                stderr,
                "[renderer] no queue family has the graphics bit set\n"
            );
        renderer_terminate();
        return RENDERER_INIT_ERROR;
    }

    return RENDERER_INIT_OKAY;
}

enum renderer_init_result setup_logical_device()
{
    VkDeviceQueueCreateInfo queue_create_info[] = {
        {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = renderer.queue_families.graphics.index,
            .queueCount = 1,
            .pQueuePriorities = &(float){1.0f}
        }
    };

    VkPhysicalDeviceFeatures device_features = { };

    VkDeviceCreateInfo device_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pQueueCreateInfos = queue_create_info,
        .queueCreateInfoCount =
            sizeof(queue_create_info) / sizeof(*queue_create_info),
        .pEnabledFeatures = &device_features
    };

    VkResult result = vkCreateDevice(
            renderer.physical_device,
            &device_create_info,
            NULL,
            &renderer.device
        );

    if (result != VK_SUCCESS) {
        fprintf(stderr, "[renderer vkCreateDevice() failed\n");
        renderer_terminate();
        return RENDERER_INIT_ERROR;
    }

    vkGetDeviceQueue(
            renderer.device,
            renderer.queue_families.graphics.index,
            0,
            &renderer.graphics_queue
        );

    return RENDERER_INIT_OKAY;
}

/* initialize the renderer */
enum renderer_init_result renderer_init()
{
    enum renderer_init_result result;

    result = setup_glfw();
    if (result) return result;

    result = setup_instance();
    if (result) return result;

    result = setup_physical_device();
    if (result) return result;

    result = setup_queue_families();
    if (result) return result;

    result = setup_logical_device();
    if (result) return result;

    renderer.initialized = true;

    return RENDERER_INIT_OKAY;
}

void renderer_terminate()
{
    renderer.initialized = false;

    if (renderer.device) {
        vkDestroyDevice(renderer.device, NULL);
        renderer.device = NULL;
        /* don't have to separately destroy VkQueues */
        renderer.graphics_queue = NULL;
    }

    /* VkPhysicalDevice doesn't have a separate destroy */

    if (renderer.instance) {
        vkDestroyInstance(renderer.instance, NULL);
        renderer.instance = NULL;
        renderer.physical_device = NULL;
        renderer.queue_families = (struct queue_families) { };
    }

    if (renderer.layers) {
        free(renderer.layers);
        renderer.layers = NULL;
        renderer.n_layers = 0;
    }

    if (renderer.window) {
        glfwDestroyWindow(renderer.window);
        renderer.window = NULL;
    }

    if (renderer.glfw_needs_terminate) {
        glfwTerminate();
        renderer.glfw_needs_terminate = false;
    }
}

void renderer_loop()
{
    if (!renderer.initialized) {
        return;
    }
    while (!glfwWindowShouldClose(renderer.window)) {
        glfwPollEvents();
    }
}
