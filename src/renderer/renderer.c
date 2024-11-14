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

/* TODO: when we add configuration, make this configurable at run-time */
#ifndef SHADER_BASE_PATH
#define SHADER_BASE_PATH "out/shaders"
#endif /* SHADER_BASE_PATH */

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "util/sorted_set.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

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
        } graphics, /* the graphics queue family */
          present; /* the presentation queue family */
    } queue_families; /* the queue families */

    VkQueue graphics_queue; /* the graphics queue */

    VkSurfaceKHR surface; /* the window surface */

    struct swap_chain_details {
        VkSurfaceCapabilitiesKHR capabilities;
        VkSurfaceFormatKHR * formats;
        VkSurfaceFormatKHR format; /* the format we picked */
        uint32_t  n_formats;
        VkPresentModeKHR * present_modes;
        VkPresentModeKHR present_mode; /* the present mode we picked */
        uint32_t n_present_modes;
        VkExtent2D extent;
    } chain_details; /* information about the swap chain */

    VkSwapchainKHR swap_chain;
    VkImage * swap_chain_images;
    uint32_t n_swap_chain_images;
    VkImageView * swap_chain_image_views;

    VkRenderPass render_pass;
    VkPipelineLayout layout;
    VkPipeline pipeline;

} renderer = { };

static enum renderer_init_result load_file(
        const char * name,
        char ** buffer_out,
        size_t * size_out
    ) [[gnu::nonnull(1, 2)]]
{
    size_t fullpath_length = snprintf(
            NULL, 0, "%s/%s", SHADER_BASE_PATH, name);
    char * fullpath = malloc(fullpath_length + 1);
    snprintf(fullpath, fullpath_length + 1, "%s/%s", SHADER_BASE_PATH, name);

    FILE * file = fopen(fullpath, "rb");

    if (!file) {
        fprintf(
                stderr,
                "[renderer] error opening file %s: %s\n",
                fullpath,
                strerror(errno)
            );
        free(fullpath);
        return RENDERER_INIT_ERROR;
    }

    if (fseek(file, 0, SEEK_END)) {
        fprintf(
                stderr,
                "[renderer] error fseeking file %s: %s\n",
                fullpath,
                strerror(ferror(file))
            );
        fclose(file);
        free(fullpath);
        return RENDERER_INIT_ERROR;
    }

    long tell_size = ftell(file);

    if (tell_size < 0) {
        fprintf(
                stderr,
                "[renderer] error ftelling file %s: %s\n",
                fullpath,
                strerror(ferror(file))
            );
        fclose(file);
        free(fullpath);
        return RENDERER_INIT_ERROR;
    }

    if (fseek(file, 0, SEEK_SET)) {
        fprintf(
                stderr,
                "[renderer] error fseeking file %s: %s\n",
                fullpath,
                strerror(ferror(file))
            );
        fclose(file);
        free(fullpath);
        return RENDERER_INIT_ERROR;
    }

    if (tell_size == 0) {
        fprintf(
                stderr,
                "[renderer] (INFO) file %s is empty\n",
                fullpath
            );
        fclose(file);
        free(fullpath);
        *buffer_out = NULL;
        *size_out = 0;
        return RENDERER_INIT_OKAY;
    }

    char * buffer = malloc(tell_size);

    if (fread(buffer, tell_size, 1, file) != 1) {
        fprintf(
                stderr,
                "[renderer] error reading file %s: %s\n",
                fullpath,
                strerror(ferror(file))
            );
        fclose(file);
        free(fullpath);
        free(buffer);
        return RENDERER_INIT_ERROR;
    }

    fprintf(stderr, "[renderer] (INFO) loaded file %s\n", fullpath);

    fclose(file);
    free(fullpath);

    assert((long)(size_t)tell_size == tell_size);

    *size_out = (size_t)tell_size;
    *buffer_out = buffer;

    return RENDERER_INIT_OKAY;
}

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

/* callback  */
static void print_missing_thing(
        const char * key, size_t length, void * data, void * ptr)
{
    (void)length;
    (void)data;
    const char * thing = ptr;
    fprintf(stderr, "[renderer] (INFO) missing %s %s\n", key, thing);
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

    /* extensions required by us */
    /* TODO: the mac extension VK_KHR_portability_enumeration
     *       as an optional extension, and if it exists set the
     *       .flags field in the instance create info to
     *       VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_KHR
     */
    const char * our_extensions[] = { };
    sorted_set_add_keys_copy(
            extensions_set,
            our_extensions, 
            NULL,
            NULL,
            sizeof(our_extensions) / sizeof(*our_extensions)
        );

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

    struct sorted_set * available_extensions_set = sorted_set_create();
    uint32_t n_available_extensions;
    vkEnumerateInstanceExtensionProperties(
            NULL, &n_available_extensions, NULL);
    VkExtensionProperties * available_extensions = malloc(
            sizeof(*available_extensions) * n_available_extensions);
    vkEnumerateInstanceExtensionProperties(
            NULL, &n_available_extensions, available_extensions);

    for (size_t i = 0; i < n_available_extensions; i++) {
        sorted_set_add_key_copy(
                available_extensions_set,
                available_extensions[i].extensionName,
                0,
                NULL
            );
    }

    free(available_extensions);

    struct sorted_set * missing_extensions_set =
        sorted_set_difference(extensions_set, available_extensions_set);

    if (sorted_set_size(missing_extensions_set)) {
        sorted_set_apply(
                missing_extensions_set, &print_missing_thing, "extension");
        fprintf(stderr, "[renderer] missing required extensions\n");
        sorted_set_destroy(missing_extensions_set);
        sorted_set_destroy(available_extensions_set);
        sorted_set_destroy(extensions_set);
        renderer_terminate();
        return RENDERER_INIT_ERROR;
    }

    sorted_set_destroy(missing_extensions_set);
    sorted_set_destroy(available_extensions_set);

    struct sorted_set * layers_set = sorted_set_create();

#if !NDEBUG
    sorted_set_add_key_copy(
            layers_set, "VK_LAYER_KHRONOS_validation", 0, NULL);
#endif /* NDEBUG */

    struct sorted_set * available_layers_set = sorted_set_create();
    uint32_t n_available_layers;
    vkEnumerateInstanceLayerProperties(&n_available_layers, NULL);
    VkLayerProperties * available_layers = malloc(
        sizeof(*available_layers) * n_available_layers);
    vkEnumerateInstanceLayerProperties(&n_available_layers, available_layers);

    for (size_t i = 0; i < n_available_layers; i++) {
        sorted_set_add_key_copy(
                available_layers_set, available_layers[i].layerName, 0, NULL);
    }

    free(available_layers);

    struct sorted_set * missing_layers_set =
        sorted_set_difference(layers_set, available_layers_set);

    if (sorted_set_size(missing_layers_set)) {
        sorted_set_apply(
                missing_layers_set, &print_missing_thing, "layer");
        fprintf(stderr, "[renderer] missing required layers\n");
        sorted_set_destroy(missing_layers_set);
        sorted_set_destroy(layers_set);
        sorted_set_destroy(available_layers_set);
        renderer_terminate();
        return RENDERER_INIT_ERROR;
    }
    sorted_set_destroy(available_layers_set);
    sorted_set_destroy(missing_layers_set);

    size_t n_extensions;
    const char ** extensions = sorted_set_flatten_keys(
            extensions_set, &n_extensions);

    size_t n_layers;
    const char ** layers = sorted_set_flatten_keys(layers_set, &n_layers);

    VkInstanceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledExtensionCount = n_extensions,
        .ppEnabledExtensionNames = extensions,
        .enabledLayerCount = n_layers,
        .ppEnabledLayerNames = layers
    };

    VkResult result = vkCreateInstance(&create_info, NULL, &renderer.instance);

    free(extensions);
    free(layers);
    sorted_set_destroy(extensions_set);
    sorted_set_destroy(layers_set);

    if (result != VK_SUCCESS) {
        fprintf(
                stderr,
                "[renderer] vkCreateInstance() failed (%d)\n",
                result
            );
        renderer_terminate();
        return RENDERER_INIT_ERROR;
    }

    return RENDERER_INIT_OKAY;
}

/* have GLFW create a window sruface */
static enum renderer_init_result setup_window_surface()
{
    VkResult result = glfwCreateWindowSurface(
            renderer.instance,
            renderer.window,
            NULL,
            &renderer.surface
        );

    if (result != VK_SUCCESS) {
        fprintf(
                stderr,
                "[renderer] glfwCreateWindowSurface() failed (%d)\n",
                result
            );
        renderer_terminate();
        return RENDERER_INIT_ERROR;
    }

    return RENDERER_INIT_OKAY;
}

/* find appropriate queue families (using a candidate physical device)
 *
 * this doesn't terminate on error because we might be able to try again with
 * a different device
 */
static enum renderer_init_result setup_queue_families(
        VkPhysicalDevice candidate)
{
    uint32_t n_queue_families;
    vkGetPhysicalDeviceQueueFamilyProperties(
            candidate, &n_queue_families, NULL);
    VkQueueFamilyProperties * queue_families =
        malloc(sizeof(*queue_families) * n_queue_families);
    vkGetPhysicalDeviceQueueFamilyProperties(
            candidate, &n_queue_families, queue_families);

    for (size_t i = 0; i < n_queue_families; i++) {
        if (!renderer.queue_families.graphics.exists) {
            if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                renderer.queue_families.graphics.index = i;
                renderer.queue_families.graphics.exists = true;
            }
        }

        if (!renderer.queue_families.present.exists) {
            VkBool32 can_present;
            vkGetPhysicalDeviceSurfaceSupportKHR(
                    candidate, i, renderer.surface, &can_present);
            if (can_present) {
                renderer.queue_families.present.index = i;
                renderer.queue_families.present.exists = true;
            }
        }
    }

    free(queue_families);

    if (!renderer.queue_families.graphics.exists) {
        fprintf(
                stderr,
                "[renderer] (INFO) candidate device lacks graphics bit\n"
            );
        return RENDERER_INIT_ERROR;
    }

    if (!renderer.queue_families.present.exists) {
        fprintf(
                stderr,
                "[renderer] (INFO) candidate device cannot present to surface\n"
            );
        return RENDERER_INIT_ERROR;
    }

    return RENDERER_INIT_OKAY;
}

/* set up the swap chain */
static enum renderer_init_result setup_swap_chain()
{
    /* prefer SRGB R8G8B8 */
    renderer.chain_details.format = renderer.chain_details.formats[0];
    for (uint32_t i = 0; i < renderer.chain_details.n_formats; i++) {
        if (renderer.chain_details.formats[i].format ==
                VK_FORMAT_B8G8R8A8_SRGB &&
                renderer.chain_details.formats[i].colorSpace ==
                VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            renderer.chain_details.format = renderer.chain_details.formats[i];
            break;
        }
    }

    /* prefer MAILBOX */
    renderer.chain_details.present_mode = VK_PRESENT_MODE_FIFO_KHR;
    for (uint32_t i = 0; i < renderer.chain_details.n_present_modes; i++) {
        if (renderer.chain_details.present_modes[i] ==
                VK_PRESENT_MODE_MAILBOX_KHR) {
            renderer.chain_details.present_mode =
                renderer.chain_details.present_modes[i];
        }
    }

    if (renderer.chain_details.capabilities.currentExtent.width !=
            UINT32_MAX) {
        renderer.chain_details.extent =
            renderer.chain_details.capabilities.currentExtent;
    } else {
        int width, height;
        glfwGetFramebufferSize(renderer.window, &width, &height);

        VkExtent2D extent = {
            .width = width,
            .height = height
        };

        if (extent.width <
                renderer.chain_details.capabilities.minImageExtent.width) {
            extent.width =
                renderer.chain_details.capabilities.minImageExtent.width;
        } else if (extent.width >
                renderer.chain_details.capabilities.maxImageExtent.width) {
            extent.width =
                renderer.chain_details.capabilities.maxImageExtent.width;
        }

        if (extent.height <
                renderer.chain_details.capabilities.minImageExtent.height) {
            extent.height =
                renderer.chain_details.capabilities.minImageExtent.height;
        } else if (extent.height >
                renderer.chain_details.capabilities.maxImageExtent.height) {
            extent.height =
                renderer.chain_details.capabilities.maxImageExtent.height;
        }

        renderer.chain_details.extent = extent;
    }

    VkSwapchainCreateInfoKHR create_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = renderer.surface,
        .minImageCount =
            renderer.chain_details.capabilities.minImageCount + 1,
        .imageFormat = renderer.chain_details.format.format,
        .imageColorSpace = renderer.chain_details.format.colorSpace,
        .imageExtent = renderer.chain_details.extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .preTransform =
            renderer.chain_details.capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = renderer.chain_details.present_mode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE
    };

    if (renderer.queue_families.graphics.index !=
            renderer.queue_families.present.index) {
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = (uint32_t[]){
            renderer.queue_families.graphics.index,
            renderer.queue_families.present.index
        };
    } else {
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VkResult result = vkCreateSwapchainKHR(
                renderer.device, &create_info, NULL, &renderer.swap_chain);

    if (result != VK_SUCCESS) {
        fprintf(
                stderr,
                "[renderer] vkCreateSwapchainKHR failed (%d)\n",
                result
            );
        renderer_terminate();
        return RENDERER_INIT_ERROR;
    }

    vkGetSwapchainImagesKHR(
            renderer.device,
            renderer.swap_chain,
            &renderer.n_swap_chain_images,
            NULL
        );
    renderer.swap_chain_images = malloc(
            sizeof(*renderer.swap_chain_images) *
            renderer.n_swap_chain_images);
    vkGetSwapchainImagesKHR(
            renderer.device,
            renderer.swap_chain,
            &renderer.n_swap_chain_images,
            renderer.swap_chain_images
        );

    return RENDERER_INIT_OKAY;
}

/* test if this candidate supports the window surface/swap chain
 *
 * this doesn't terminate on error because it might be called with another
 * candidate
 */
static enum renderer_init_result setup_swap_chain_details(
        VkPhysicalDevice candidate)
{
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            candidate,
            renderer.surface,
            &renderer.chain_details.capabilities
        );

    vkGetPhysicalDeviceSurfaceFormatsKHR(
            candidate,
            renderer.surface,
            &renderer.chain_details.n_formats,
            NULL
        );

    if (renderer.chain_details.formats) {
        free(renderer.chain_details.formats);
    }

    renderer.chain_details.formats =
        malloc(sizeof(*renderer.chain_details.formats) *
                renderer.chain_details.n_formats);

    vkGetPhysicalDeviceSurfaceFormatsKHR(
            candidate,
            renderer.surface,
            &renderer.chain_details.n_formats,
            renderer.chain_details.formats
        );

    vkGetPhysicalDeviceSurfacePresentModesKHR(
            candidate,
            renderer.surface,
            &renderer.chain_details.n_present_modes,
            NULL
        );

    if (renderer.chain_details.present_modes) {
        free(renderer.chain_details.present_modes);
    }

    renderer.chain_details.present_modes =
        malloc(sizeof(*renderer.chain_details.present_modes) *
                renderer.chain_details.n_present_modes);

    vkGetPhysicalDeviceSurfacePresentModesKHR(
            candidate,
            renderer.surface,
            &renderer.chain_details.n_present_modes,
            renderer.chain_details.present_modes
        );

    if (renderer.chain_details.n_formats == 0) {
        fprintf(stderr, "[renderer] (INFO) device has no formats for this surface\n");
        return RENDERER_INIT_ERROR;
    }

    if (renderer.chain_details.n_present_modes == 0) {
        fprintf(stderr, "[renderer] (INFO) device has no present modes for this surface\n");
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

    struct sorted_set * required_extensions_set = sorted_set_create();
    sorted_set_add_key_copy(
            required_extensions_set, VK_KHR_SWAPCHAIN_EXTENSION_NAME, 0, NULL);

    VkPhysicalDevice candidate = NULL;
    /* find a suitable device */
    /* TODO: user override */
    for (size_t i = 0; i < n_devices; i++) {
        VkPhysicalDeviceProperties device_properties;
        vkGetPhysicalDeviceProperties(devices[i], &device_properties);
        fprintf(
                stderr,
                "[renderer] (INFO) found physical device %s\n",
                device_properties.deviceName
            );

        uint32_t n_extensions;
        vkEnumerateDeviceExtensionProperties(
                devices[i], NULL, &n_extensions, NULL);
        VkExtensionProperties * extensions =
            malloc(sizeof(*extensions) * n_extensions);
        vkEnumerateDeviceExtensionProperties(
                devices[i], NULL, &n_extensions, extensions);

        struct sorted_set * extensions_set = sorted_set_create();

        for (size_t i = 0; i < n_extensions; i++ ) {
            sorted_set_add_key_copy(
                    extensions_set, extensions[i].extensionName, 0, NULL);
        }

        free(extensions);

        struct sorted_set * missing_set =
            sorted_set_difference(required_extensions_set, extensions_set);

        if (sorted_set_size(missing_set)) {
            sorted_set_apply(missing_set, &print_missing_thing, "extension");
            sorted_set_destroy(missing_set);
            sorted_set_destroy(extensions_set);
            continue;
        }

        sorted_set_destroy(missing_set);
        sorted_set_destroy(extensions_set);

        if (setup_queue_families(devices[i])) {
            continue;
        }

        if (setup_swap_chain_details(devices[i])) {
            continue;
        }

        if (device_properties.deviceType ==
                VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            candidate = devices[i];
        } else if (!candidate) {
            candidate = devices[i];
        }
    }

    sorted_set_destroy(required_extensions_set);
    free(devices);

    if (!candidate) {
        fprintf(
                stderr,
                "[renderer] no suitable physical dveices found\n"
            );
        renderer_terminate();
        return RENDERER_INIT_ERROR;
    }

    /* run it again now that we've picked */
    setup_queue_families(candidate);
    setup_swap_chain_details(candidate);

    VkPhysicalDeviceProperties device_properties;
    vkGetPhysicalDeviceProperties(candidate, &device_properties);
    fprintf(
            stderr,
            "[renderer] (INFO) picked device %s (discrete: %s)\n",
            device_properties.deviceName,
            device_properties.deviceType ==
                VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? "true" : "false"
       );

    renderer.physical_device = candidate;

    return RENDERER_INIT_OKAY;
}

/* create a logical device */
static enum renderer_init_result setup_logical_device()
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

    const char * extensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    VkDeviceCreateInfo device_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pQueueCreateInfos = queue_create_info,
        .queueCreateInfoCount =
            sizeof(queue_create_info) / sizeof(*queue_create_info),
        .pEnabledFeatures = &device_features,
        .enabledExtensionCount = sizeof(extensions) / sizeof(*extensions),
        .ppEnabledExtensionNames = extensions
    };

    VkResult result = vkCreateDevice(
            renderer.physical_device,
            &device_create_info,
            NULL,
            &renderer.device
        );

    if (result != VK_SUCCESS) {
        fprintf(
                stderr,
                "[renderer] vkCreateDevice() failed (%d)\n",
                result
            );
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

/* create image views for every image in the swap chain */
static enum renderer_init_result setup_image_views()
{
    renderer.swap_chain_image_views = calloc(
            renderer.n_swap_chain_images,
            sizeof(*renderer.swap_chain_images)
        );

    for (uint32_t i = 0; i < renderer.n_swap_chain_images; i++) {
        VkImageViewCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = renderer.swap_chain_images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = renderer.chain_details.format.format,
            .components = {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY
            },
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        };

        VkResult result = vkCreateImageView(
                renderer.device,
                &create_info,
                NULL,
                &renderer.swap_chain_image_views[i]
            );

        if (result != VK_SUCCESS) {
            fprintf(
                    stderr,
                    "[renderer] vkCreateImageView failed (%d)\n",
                    result
                );
            renderer_terminate();
            return RENDERER_INIT_ERROR;
        }
    }

    return RENDERER_INIT_OKAY;
}

/* create the graphics pipeline(s) */
enum renderer_init_result setup_pipeline()
{
    char * vertex_shader_blob = NULL,
         * fragment_shader_blob = NULL;
    size_t vertex_shader_blob_size,
           fragment_shader_blob_size;

    enum renderer_init_result result1 = load_file(
                "vertex.spv",
                &vertex_shader_blob,
                &vertex_shader_blob_size);
    enum renderer_init_result result2 = load_file(
                "fragment.spv",
                &fragment_shader_blob,
                &fragment_shader_blob_size);

    if (result1 || result2) {
        fprintf(
                stderr,
                "[renderer] loading shaders failed\n"
            );
        if (vertex_shader_blob) {
            free(vertex_shader_blob);
        }
        if (fragment_shader_blob) {
            free(fragment_shader_blob);
        }
        renderer_terminate();
        return RENDERER_INIT_ERROR;
    }

    VkShaderModule vertex_module,
                   fragment_module;

    VkResult result = vkCreateShaderModule(
            renderer.device,
            &(VkShaderModuleCreateInfo) {
                .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                .codeSize = vertex_shader_blob_size,
                .pCode = (const uint32_t *)vertex_shader_blob
            },
            NULL,
            &vertex_module
        );

    if (result != VK_SUCCESS) {
        fprintf(
                stderr,
                "[renderer] vkCreateShaderModule() failed (%d) for vertex shader\n",
                result
            );
        renderer_terminate();
    }

    result = vkCreateShaderModule(
            renderer.device,
            &(VkShaderModuleCreateInfo) {
                .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                .codeSize = fragment_shader_blob_size,
                .pCode = (const uint32_t *)fragment_shader_blob
            },
            NULL,
            &fragment_module
        );

    if (result != VK_SUCCESS) {
        fprintf(
                stderr,
                "[renderer] vkCreateShaderModule() failed (%d) for fragment shader\n",
                result
            );
        vkDestroyShaderModule(renderer.device, vertex_module, NULL);
        renderer_terminate();
    }

    free(vertex_shader_blob);
    free(fragment_shader_blob);

    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO
    };

    result = vkCreatePipelineLayout(
            renderer.device, &pipeline_layout_info, NULL, &renderer.layout);
    
    if (result != VK_SUCCESS) {
        fprintf(
                stderr,
                "[renderer] vkCreatePipelineLayout() failed (%d)\n",
                result
            );
        vkDestroyShaderModule(renderer.device, vertex_module, NULL);
        vkDestroyShaderModule(renderer.device, fragment_module, NULL);
        renderer_terminate();
        return RENDERER_INIT_ERROR;
    }

    VkRenderPassCreateInfo render_pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = (VkAttachmentDescription[]) {
            {
                .format = renderer.chain_details.format.format,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
            }
        },
        .subpassCount = 1,
        .pSubpasses = (VkSubpassDescription[]) {
            {
                .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                .colorAttachmentCount = 1,
                .pColorAttachments = (VkAttachmentReference[]) {
                    {
                        .attachment = 0,
                        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                    }
                }
            }
        }
    };

    result = vkCreateRenderPass(
            renderer.device, &render_pass_info, NULL, &renderer.render_pass);

    if (result != VK_SUCCESS) {
        fprintf(
                stderr,
                "[renderer] vkCreateRenderPass() failed (%d)\n",
                result
            );
        vkDestroyShaderModule(renderer.device, vertex_module, NULL);
        vkDestroyShaderModule(renderer.device, fragment_module, NULL);
        renderer_terminate();
        return RENDERER_INIT_ERROR;
    }

    VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = (VkPipelineShaderStageCreateInfo[]) {
            {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_VERTEX_BIT,
                .pName = "main",
                .module = vertex_module
                /* can add specialization info here */
            },
            {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                .pName = "main",
                .module = fragment_module
                /* can add specialization info here */
            }
        },
        .pDynamicState = &(VkPipelineDynamicStateCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .dynamicStateCount = 2,
            .pDynamicStates = (VkDynamicState[]) {
                VK_DYNAMIC_STATE_VIEWPORT,
                VK_DYNAMIC_STATE_SCISSOR
            }
        },
        .pVertexInputState = &(VkPipelineVertexInputStateCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO
        },
        .pInputAssemblyState = &(VkPipelineInputAssemblyStateCreateInfo) {
            .sType =
                VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .primitiveRestartEnable = VK_FALSE
        },
        .pViewportState = &(VkPipelineViewportStateCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1,
            .pViewports = (VkViewport[]) {
                {
                    .x = 0.0f,
                    .y = 0.0f,
                    .width = (float)renderer.chain_details.extent.width,
                    .height = (float)renderer.chain_details.extent.height,
                    .minDepth = 0.0f,
                    .maxDepth = 1.0f
                }
            },
            .scissorCount = 1,
            .pScissors = (VkRect2D[]) {
                {
                    .offset = { 0, 0 },
                    .extent = renderer.chain_details.extent
                }
            }
        },
        .pRasterizationState = &(VkPipelineRasterizationStateCreateInfo) {
            .sType =
                VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .depthClampEnable = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .lineWidth = 1.0f,
            .cullMode = VK_CULL_MODE_BACK_BIT,
            .frontFace = VK_FRONT_FACE_CLOCKWISE,
            .depthBiasEnable = VK_FALSE
        },
        .pMultisampleState = &(VkPipelineMultisampleStateCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .sampleShadingEnable = VK_FALSE,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
        },
        .pDepthStencilState = &(VkPipelineDepthStencilStateCreateInfo) {
            .sType =
                VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        },
        .pColorBlendState = &(VkPipelineColorBlendStateCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .logicOpEnable = VK_FALSE,
            .attachmentCount = 1,
            .pAttachments = (VkPipelineColorBlendAttachmentState[]) {
                {
                    .blendEnable = VK_FALSE
                }
            }
        },
        .layout = renderer.layout,
        .renderPass = renderer.render_pass,
        .subpass = 0 /* index into render_pass subpasses used to which this
                        pipeline belongs */
    };

    result = vkCreateGraphicsPipelines(
            renderer.device,
            VK_NULL_HANDLE,
            1,
            &pipeline_info,
            NULL,
            &renderer.pipeline
        );

    if (result != VK_SUCCESS) {
        fprintf(
                stderr,
                "[renderer] vkCreateGraphicsPipelines() failed (%d)\n",
                result
            );
        vkDestroyShaderModule(renderer.device, vertex_module, NULL);
        vkDestroyShaderModule(renderer.device, fragment_module, NULL);
        renderer_terminate();
        return RENDERER_INIT_ERROR;
    }

    vkDestroyShaderModule(renderer.device, vertex_module, NULL);
    vkDestroyShaderModule(renderer.device, fragment_module, NULL);

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

    result = setup_window_surface();
    if (result) return result;

    result = setup_physical_device();
    if (result) return result;

    result = setup_logical_device();
    if (result) return result;

    result = setup_swap_chain();
    if (result) return result;

    result = setup_image_views();
    if (result) return result;

    result = setup_pipeline();
    if (result) return result;

    renderer.initialized = true;

    return RENDERER_INIT_OKAY;
}

void renderer_terminate()
{
    renderer.initialized = false;

    if (renderer.pipeline) {
        vkDestroyPipeline(renderer.device, renderer.pipeline, NULL);
        renderer.pipeline = NULL;
    }

    if (renderer.render_pass) {
        vkDestroyRenderPass(renderer.device, renderer.render_pass, NULL);
        renderer.render_pass = NULL;
    }

    if (renderer.layout) {
        vkDestroyPipelineLayout(renderer.device, renderer.layout, NULL);
        renderer.layout = NULL;
    }

    if (renderer.swap_chain) {
        for (uint32_t i = 0; i < renderer.n_swap_chain_images; i++) {
            if (renderer.swap_chain_image_views[i]) {
                vkDestroyImageView(
                        renderer.device,
                        renderer.swap_chain_image_views[i],
                        NULL
                    );
                renderer.swap_chain_image_views[i] = NULL;
            }
        }
        free(renderer.swap_chain_images);
        free(renderer.swap_chain_image_views);

        vkDestroySwapchainKHR(renderer.device, renderer.swap_chain, NULL);
        renderer.swap_chain = NULL;
    }

    if (renderer.chain_details.formats) {
        free(renderer.chain_details.formats);
        renderer.chain_details.formats = NULL;
        renderer.chain_details.n_formats = 0;
        renderer.chain_details.format = (VkSurfaceFormatKHR) { };
    }

    if (renderer.chain_details.present_modes) {
        free(renderer.chain_details.present_modes);
        renderer.chain_details.present_modes = NULL;
        renderer.chain_details.n_present_modes = 0;
        renderer.chain_details.present_mode = (VkPresentModeKHR) { };
    }

    if (renderer.device) {
        vkDestroyDevice(renderer.device, NULL);
        renderer.device = NULL;
        /* don't have to separately destroy VkQueues */
        renderer.graphics_queue = NULL;
    }

    /* VkPhysicalDevice doesn't have a separate destroy */
    renderer.physical_device = NULL;
    renderer.queue_families = (struct queue_families) { };

    if (renderer.surface) {
        vkDestroySurfaceKHR(renderer.instance, renderer.surface, NULL);
        renderer.surface = NULL;
    }

    if (renderer.instance) {
        vkDestroyInstance(renderer.instance, NULL);
        renderer.instance = NULL;
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
