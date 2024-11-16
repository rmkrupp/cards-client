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

/* TODO: configuration
 *
 *       see below comments, but also:
 *
 *        - multisampling
 *        - color space stuff?
 */

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

    struct renderer_configuration config;

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

    VkQueue graphics_queue,
            present_queue; /* the queues */

    VkSurfaceKHR surface; /* the window surface */

    bool needs_recreation; /* should we recreate the swap chain? */
    bool recreated; /* did we just recreate the swap chain? */
    bool minimized; /* are we minimized (and thus should not render?) */

    struct swap_chain_details {
        VkSurfaceCapabilitiesKHR capabilities;
        VkSurfaceFormatKHR * formats;
        VkSurfaceFormatKHR format; /* the format we picked */
        uint32_t n_formats;
        VkPresentModeKHR * present_modes;
        VkPresentModeKHR present_mode; /* the present mode we picked */
        uint32_t n_present_modes;
        VkExtent2D extent;
    } chain_details; /* information about the swap chain */

    VkSwapchainKHR swap_chain;
    VkImage * swap_chain_images;
    uint32_t n_swap_chain_images;
    VkImageView * swap_chain_image_views;
    VkFramebuffer * framebuffers;

    VkRenderPass render_pass;
    VkPipelineLayout layout;
    VkPipeline pipeline;

    VkCommandPool command_pool;
    VkCommandBuffer * command_buffers;

    struct {
        VkSemaphore image_available,
                    render_finished;
        VkFence in_flight;
    } * sync;

    uint32_t current_frame;
    uint64_t frame;

} renderer = { };

/* recreate the parts of the renderer that can have gone stale */
static enum renderer_result renderer_recreate_swap_chain();

/* load a thing from a file */
static enum renderer_result load_file(
        const char * name,
        const char * basename,
        char ** buffer_out,
        size_t * size_out
    ) [[gnu::nonnull(1, 2)]]
{
    size_t fullpath_length = snprintf(
            NULL, 0, "%s/%s", basename, name);
    char * fullpath = malloc(fullpath_length + 1);
    snprintf(fullpath, fullpath_length + 1, "%s/%s", basename, name);

    FILE * file = fopen(fullpath, "rb");

    if (!file) {
        fprintf(
                stderr,
                "[renderer] error opening file %s: %s\n",
                fullpath,
                strerror(errno)
            );
        free(fullpath);
        return RENDERER_ERROR;
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
        return RENDERER_ERROR;
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
        return RENDERER_ERROR;
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
        return RENDERER_ERROR;
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
        return RENDERER_OKAY;
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
        return RENDERER_ERROR;
    }

    fprintf(stderr, "[renderer] (INFO) loaded file %s\n", fullpath);

    fclose(file);
    free(fullpath);

    assert((long)(size_t)tell_size == tell_size);

    *size_out = (size_t)tell_size;
    *buffer_out = buffer;

    return RENDERER_OKAY;
}

static void framebuffer_resize_callback(
        GLFWwindow * window, int width, int height)
{
    (void)window;
    (void)width;
    (void)height;
    renderer.needs_recreation = true;
}

/* initialize GLFW and create a window */
static enum renderer_result setup_glfw()
{
    glfwInit();
    renderer.glfw_needs_terminate = true;

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    /* TODO: configurable: window resolution, fullscreen vs windowed */
    renderer.window = glfwCreateWindow(800, 600, "cards-client", NULL, NULL);
    glfwSetFramebufferSizeCallback(
            renderer.window, &framebuffer_resize_callback);

    if (!renderer.window) {
        fprintf(stderr, "[renderer] glfwCreateWindow() failed\n");
        renderer_terminate();
        return RENDERER_ERROR;
    }

    return RENDERER_OKAY;
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
static enum renderer_result setup_instance()
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
     *
     *       this will require a set_intersection sorted_set function
     *
     *       and a configuration option for verbosity for reporting whether
     *       optional extensions were present?
     */
    const char * our_extensions[] = { "VK_KHR_get_physical_device_properties2" };
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
        return RENDERER_ERROR;
    }

    sorted_set_destroy(missing_extensions_set);
    sorted_set_destroy(available_extensions_set);

    struct sorted_set * layers_set = sorted_set_create();

    /* TODO: base this on something besides NDEBUG ? */
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
        return RENDERER_ERROR;
    }
    sorted_set_destroy(available_layers_set);
    sorted_set_destroy(missing_layers_set);

    size_t n_extensions;
    const char ** extensions = sorted_set_flatten_keys(
            extensions_set, &n_extensions);

    size_t n_layers;
    const char ** layers = sorted_set_flatten_keys(layers_set, &n_layers);
    renderer.n_layers = n_layers;
    renderer.layers = layers;

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
    sorted_set_destroy(extensions_set);
    sorted_set_destroy_except_keys(layers_set);

    if (result != VK_SUCCESS) {
        fprintf(
                stderr,
                "[renderer] vkCreateInstance() failed (%d)\n",
                result
            );
        renderer_terminate();
        return RENDERER_ERROR;
    }

    return RENDERER_OKAY;
}

/* have GLFW create a window sruface */
static enum renderer_result setup_window_surface()
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
        return RENDERER_ERROR;
    }

    return RENDERER_OKAY;
}

/* find appropriate queue families (using a candidate physical device)
 *
 * this doesn't terminate on error because we might be able to try again with
 * a different device
 */
static enum renderer_result setup_queue_families(
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
        return RENDERER_ERROR;
    }

    if (!renderer.queue_families.present.exists) {
        fprintf(
                stderr,
                "[renderer] (INFO) candidate device cannot present to surface\n"
            );
        return RENDERER_ERROR;
    }

    return RENDERER_OKAY;
}

/* set up the swap chain */
static enum renderer_result setup_swap_chain(VkSwapchainKHR old_swap_chain)
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

        if (height == 0 || width == 0) {
            printf("minimized\n");
            renderer.minimized = true;
            renderer.needs_recreation = true;
            return RENDERER_OKAY;
        }

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
        .oldSwapchain = old_swap_chain
    };

    if (renderer.chain_details.capabilities.maxImageCount > 0) {
        if (create_info.minImageCount >
                renderer.chain_details.capabilities.maxImageCount) {
            create_info.minImageCount =
                renderer.chain_details.capabilities.maxImageCount;
        }
    }

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
        return RENDERER_ERROR;
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

    return RENDERER_OKAY;
}

/* test if this candidate supports the window surface/swap chain
 *
 * this doesn't terminate on error because it might be called with another
 * candidate
 */
static enum renderer_result setup_swap_chain_details(
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
        return RENDERER_ERROR;
    }

    if (renderer.chain_details.n_present_modes == 0) {
        fprintf(stderr, "[renderer] (INFO) device has no present modes for this surface\n");
        return RENDERER_ERROR;
    }

    return RENDERER_OKAY;
}

/* pick a physical device */
static enum renderer_result setup_physical_device()
{
    uint32_t n_devices;
    vkEnumeratePhysicalDevices(renderer.instance, &n_devices, NULL);
    if (n_devices == 0) {
        fprintf(stderr, "[renderer] no devices have Vulkan support\n");
        renderer_terminate();
        return RENDERER_ERROR;
    }

    VkPhysicalDevice * devices = malloc(sizeof(*devices) * n_devices);
    vkEnumeratePhysicalDevices(renderer.instance, &n_devices, devices);

    struct sorted_set * required_extensions_set = sorted_set_create();
    sorted_set_add_key_copy(
            required_extensions_set, VK_KHR_SWAPCHAIN_EXTENSION_NAME, 0, NULL);

    VkPhysicalDevice candidate = NULL;
    /* find a suitable device */
    /* TODO: configuration to either select the device automatically or to
     *       always select a specific device, plus a function to query for a
     *       list of devices to present as choices to the user, possibly
     *       filtered for devices that are compatible? (or present two lists)
     */
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
        return RENDERER_ERROR;
    }

    /* run it again now that we've picked */
    setup_queue_families(candidate);
    setup_swap_chain_details(candidate);

    VkPhysicalDeviceProperties device_properties;
    vkGetPhysicalDeviceProperties(candidate, &device_properties);
    /* TODO: configurable verbosity */
    fprintf(
            stderr,
            "[renderer] (INFO) picked device %s (discrete: %s)\n",
            device_properties.deviceName,
            device_properties.deviceType ==
                VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? "true" : "false"
       );

    renderer.physical_device = candidate;

    return RENDERER_OKAY;
}

/* create a logical device */
static enum renderer_result setup_logical_device()
{
    /* TODO: create for each unique queue family */
    VkDeviceQueueCreateInfo queue_create_info[] = {
        {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = renderer.queue_families.graphics.index,
            .queueCount = 1,
            .pQueuePriorities = &(float){1.0f}
        }
    };

    const char * extensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    VkDeviceCreateInfo device_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pQueueCreateInfos = queue_create_info,
        .queueCreateInfoCount =
            sizeof(queue_create_info) / sizeof(*queue_create_info),
        .pEnabledFeatures = &(VkPhysicalDeviceFeatures){ },
        .enabledExtensionCount = sizeof(extensions) / sizeof(*extensions),
        .ppEnabledExtensionNames = extensions,
        .enabledLayerCount = renderer.n_layers,
        .ppEnabledLayerNames = renderer.layers
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
        return RENDERER_ERROR;
    }

    vkGetDeviceQueue(
            renderer.device,
            renderer.queue_families.graphics.index,
            0,
            &renderer.graphics_queue
        );

    vkGetDeviceQueue(
            renderer.device,
            renderer.queue_families.present.index,
            0,
            &renderer.present_queue
        );

    return RENDERER_OKAY;
}

/* create image views for every image in the swap chain */
static enum renderer_result setup_image_views()
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
            return RENDERER_ERROR;
        }
    }

    return RENDERER_OKAY;
}

/* create the graphics pipeline(s) */
static enum renderer_result setup_pipeline()
{
    char * vertex_shader_blob = NULL,
         * fragment_shader_blob = NULL;
    size_t vertex_shader_blob_size,
           fragment_shader_blob_size;

    enum renderer_result result1 = load_file(
                "vertex.spv",
                SHADER_BASE_PATH,
                &vertex_shader_blob,
                &vertex_shader_blob_size);
    enum renderer_result result2 = load_file(
                "fragment.spv",
                SHADER_BASE_PATH,
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
        return RENDERER_ERROR;
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
        return RENDERER_ERROR;
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
        },
        .dependencyCount = 1,
        .pDependencies = (VkSubpassDependency[]) {
            {
                .srcSubpass = VK_SUBPASS_EXTERNAL,
                .dstSubpass = 0,
                .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .srcAccessMask = 0,
                .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
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
        return RENDERER_ERROR;
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
            .logicOp = VK_LOGIC_OP_COPY,
            .attachmentCount = 1,
            .pAttachments = (VkPipelineColorBlendAttachmentState[]) {
                {
                    .colorWriteMask =
                        VK_COLOR_COMPONENT_R_BIT |
                        VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT |
                        VK_COLOR_COMPONENT_A_BIT,
                    .blendEnable = VK_FALSE
                }
            },
            .blendConstants = { 0.0f, 0.0f, 0.0f, 0.0f }
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
        return RENDERER_ERROR;
    }

    vkDestroyShaderModule(renderer.device, vertex_module, NULL);
    vkDestroyShaderModule(renderer.device, fragment_module, NULL);

    return RENDERER_OKAY;
}

/* create the framebuffers */
static enum renderer_result setup_framebuffers()
{
    renderer.framebuffers = calloc(
            renderer.n_swap_chain_images, sizeof(*renderer.framebuffers));


    for (uint32_t i = 0; i < renderer.n_swap_chain_images; i++) {
        VkFramebufferCreateInfo framebuffer_info = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = renderer.render_pass,
            .attachmentCount = 1,
            .pAttachments = (VkImageView[]) {
                renderer.swap_chain_image_views[i]
            },
            .width = renderer.chain_details.extent.width,
            .height = renderer.chain_details.extent.height,
            .layers = 1
        };

        VkResult result = vkCreateFramebuffer(
                renderer.device,
                &framebuffer_info,
                NULL,
                &renderer.framebuffers[i]
            );

        if (result != VK_SUCCESS) {
            fprintf(
                    stderr,
                    "[renderer] vkCreateFramebuffer() failed (%d)\n",
                    result
                );
            renderer_terminate();
            return RENDERER_ERROR;
        }
    }

    return RENDERER_OKAY;
}
/* create the command pool and buffer */
static enum renderer_result setup_command_pool()
{
    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = renderer.queue_families.graphics.index
    };

    VkResult result = vkCreateCommandPool(
            renderer.device, &pool_info, NULL, &renderer.command_pool);

    if (result != VK_SUCCESS) {
        fprintf(
                stderr,
                "[renderer] vkCreateCommandPool() failed (%d)\n",
                result
            );
        renderer_terminate();
        return RENDERER_ERROR;
    }

    renderer.command_buffers = malloc(
            sizeof(*renderer.command_buffers) *
            renderer.config.max_frames_in_flight
        );

    VkCommandBufferAllocateInfo buffer_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = renderer.command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = renderer.config.max_frames_in_flight
    };

    result = vkAllocateCommandBuffers(
            renderer.device, &buffer_info, renderer.command_buffers);

    if (result != VK_SUCCESS) {
        fprintf(
                stderr,
                "[renderer] vkAllocateCommandBuffers() failed (%d)\n",
                result
            );
        renderer_terminate();
        return RENDERER_ERROR;
    }

    return RENDERER_OKAY;
}

/* set up sync objects */
static enum renderer_result setup_sync_objects()
{
    renderer.sync = calloc(
            renderer.config.max_frames_in_flight, sizeof(*renderer.sync));

    for (uint32_t i = 0; i < renderer.config.max_frames_in_flight; i++) {
        VkSemaphoreCreateInfo semaphore_info = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
        };

        VkResult result = vkCreateSemaphore(
                renderer.device,
                &semaphore_info,
                NULL,
                &renderer.sync[i].image_available
            );

        if (result != VK_SUCCESS) {
            fprintf(
                    stderr,
                    "[renderer] vkCreateSemaphore() failed (%d)\n",
                    result
               );
            return RENDERER_ERROR;
        }

        result = vkCreateSemaphore(
                renderer.device,
                &semaphore_info,
                NULL,
                &renderer.sync[i].render_finished
            );

        if (result != VK_SUCCESS) {
            fprintf(
                    stderr,
                    "[renderer] vkCreateSemaphore() failed (%d)\n",
                    result
                );
            return RENDERER_ERROR;
        }

        VkFenceCreateInfo fence_info = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT /* start signalled because the
                                                   * first frame should not wait
                                                   */
        };

        result = vkCreateFence(
                renderer.device,
                &fence_info,
                NULL,
                &renderer.sync[i].in_flight
            );

        if (result != VK_SUCCESS) {
            fprintf(
                    stderr,
                    "[renderer] vkCreateFence() failed (%d)\n",
                    result
                );
            return RENDERER_ERROR;
        }
    }

    return RENDERER_OKAY;
}

/* record commands into a command buffer */
static enum renderer_result record_command_buffer(
        VkCommandBuffer command_buffer,
        uint32_t image_index
    )
{
    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
    };

    VkResult result = vkBeginCommandBuffer(command_buffer, &begin_info);

    if (result != VK_SUCCESS) {
        fprintf(
                stderr,
                "[renderer] vkBeginCommandBuffer() failed (%d)\n",
                result
            );
        return RENDERER_ERROR;
    }

    VkRenderPassBeginInfo render_pass_begin_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = renderer.render_pass,
        .framebuffer = renderer.framebuffers[image_index],
        .renderArea = {
            .offset = { 0, 0 },
            .extent = renderer.chain_details.extent
        },
        .clearValueCount = 1,
        .pClearValues = (VkClearValue[]) {
            {
                .color = { { 1.0f, 1.0f, 1.0f, 1.0f } }
            }
        }
    };

    vkCmdBeginRenderPass(
            command_buffer,
            &render_pass_begin_info,
            VK_SUBPASS_CONTENTS_INLINE
        );

    vkCmdBindPipeline(
            command_buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            renderer.pipeline
        );

    vkCmdSetViewport(
            command_buffer,
            0,
            1,
            &(VkViewport) {
                .x = 0.0f,
                .y = 0.0f,
                .width = (float)renderer.chain_details.extent.width,
                .height = (float)renderer.chain_details.extent.height,
                .minDepth = 0.0f,
                .maxDepth = 1.0f
            }
        );

    vkCmdSetScissor(
            command_buffer,
            0,
            1,
            &(VkRect2D) {
                .offset = { 0, 0 },
                .extent = renderer.chain_details.extent
            }
        );
        
    vkCmdDraw(command_buffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(command_buffer);

    result = vkEndCommandBuffer(command_buffer);

    if (result != VK_SUCCESS) {
        return RENDERER_ERROR;
    }

    return RENDERER_OKAY;
}

/* draw a frame */
static enum renderer_result renderer_draw_frame()
{
    if (!renderer.initialized) {
        assert(0);
        printf("not initialized\n");
        if (!renderer_init(NULL)) {
            return RENDERER_ERROR;
        }
    }

    vkWaitForFences(
            renderer.device,
            1,
            &renderer.sync[renderer.current_frame].in_flight,
            VK_TRUE,
            UINT64_MAX
        );

    uint32_t image_index;

    renderer.recreated = false;
    VkResult result = vkAcquireNextImageKHR(
            renderer.device,
            renderer.swap_chain,
            UINT64_MAX,
            renderer.sync[renderer.current_frame].image_available,
            VK_NULL_HANDLE,
            &image_index
        );

    if (renderer.needs_recreation ||
            result == VK_SUBOPTIMAL_KHR ||
            result == VK_ERROR_OUT_OF_DATE_KHR) {
        if (!renderer_recreate_swap_chain()) {
            renderer.needs_recreation = false;
            renderer.recreated = true;
        } else {
            return RENDERER_ERROR;
        }
        return RENDERER_OKAY;
    } else if (result != VK_SUCCESS) {
        return RENDERER_ERROR;
    }

    if (renderer.minimized) {
        return RENDERER_OKAY;
    }

    /* TODO when should this happen? */
    vkResetFences(
            renderer.device,
            1,
            &renderer.sync[renderer.current_frame].in_flight
        );

    vkResetCommandBuffer(renderer.command_buffers[renderer.current_frame], 0);
    if (record_command_buffer(
                renderer.command_buffers[renderer.current_frame],
                image_index
            )) {
        return RENDERER_ERROR;
    }

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = (VkSemaphore[]) {
            renderer.sync[renderer.current_frame].image_available
        },
        .pWaitDstStageMask = (VkPipelineStageFlags[]) {
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
        },
        .commandBufferCount = 1,
        .pCommandBuffers = (VkCommandBuffer[]) {
            renderer.command_buffers[renderer.current_frame]
        },
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = (VkSemaphore[]) {
            renderer.sync[renderer.current_frame].render_finished
        }
    };
    renderer.frame++;

    result = vkQueueSubmit(
            renderer.graphics_queue,
            1,
            &submit_info,
            renderer.sync[renderer.current_frame].in_flight
        );

    if (result != VK_SUCCESS) {
        fprintf(
                stderr,
                "[renderer] vkQueueSubmit() failed (%d)\n",
                result
            );
        return RENDERER_ERROR;
    }

    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = (VkSemaphore[]) {
            renderer.sync[renderer.current_frame].render_finished
        },
        .swapchainCount = 1,
        .pSwapchains = (VkSwapchainKHR[]) {
            renderer.swap_chain
        },
        .pImageIndices = &image_index
    };

    vkQueuePresentKHR(renderer.present_queue, &present_info);

    renderer.current_frame =
        (renderer.current_frame + 1) % renderer.config.max_frames_in_flight;

    return RENDERER_OKAY;
}

/* recreate the parts of the renderer that can have gone stale */
static enum renderer_result renderer_recreate_swap_chain()
{
    vkDeviceWaitIdle(renderer.device);

    if (renderer.sync) {
        for (size_t i = 0; i < renderer.config.max_frames_in_flight; i++) {
            if (renderer.sync[i].image_available) {
                vkDestroySemaphore(
                        renderer.device, renderer.sync[i].image_available, NULL);
                renderer.sync[i].image_available = NULL;
            }

            if (renderer.sync[i].render_finished) {
                vkDestroySemaphore(
                        renderer.device, renderer.sync[i].render_finished, NULL);
                renderer.sync[i].render_finished = NULL;
            }

            if (renderer.sync[i].in_flight) {
                vkDestroyFence(
                        renderer.device, renderer.sync[i].in_flight, NULL);
                renderer.sync[i].in_flight = NULL;
            }
        }

        free(renderer.sync);
    }
 
    if (renderer.framebuffers) {
        for (uint32_t i = 0; i < renderer.n_swap_chain_images; i++) {
            if (renderer.framebuffers[i]) {
                vkDestroyFramebuffer(
                        renderer.device, renderer.framebuffers[i], NULL);
                renderer.framebuffers[i] = NULL;
            }
        }
        free(renderer.framebuffers);
    }

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
        renderer.swap_chain_images = NULL;
        renderer.swap_chain_image_views = NULL;

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

    enum renderer_result result =
        setup_swap_chain_details(renderer.physical_device);
    if (result) return result;

    result = setup_swap_chain(NULL);
    if (result) return result;

    if (renderer.minimized) {
        return RENDERER_OKAY;
    }

    result = setup_image_views();
    if (result) return result;

    result = setup_pipeline();
    if (result) return result;

    result = setup_framebuffers();
    if (result) return result;

    result = setup_sync_objects();
    if (result) return result;

    return RENDERER_OKAY;
}
/* initialize the renderer */
enum renderer_result renderer_init(
        const struct renderer_configuration * config)
{
    if (config) {
        renderer.config = *config;
    }

    enum renderer_result result;

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

    result = setup_sync_objects();
    if (result) return result;

    result = setup_command_pool();
    if (result) return result;

    result = setup_swap_chain(NULL);
    if (result) return result;

    renderer.initialized = true;
    renderer.needs_recreation = false;
    renderer.recreated = false;

    /* TODO */
    if (renderer.minimized) {
        return RENDERER_OKAY;
    }

    result = setup_image_views();
    if (result) return result;

    result = setup_pipeline();
    if (result) return result;

    result = setup_framebuffers();
    if (result) return result;

    return RENDERER_OKAY;
}

void renderer_terminate()
{
    if (renderer.sync) {
        for (size_t i = 0; i < renderer.config.max_frames_in_flight; i++) {
            if (renderer.sync[i].image_available) {
                vkDestroySemaphore(
                        renderer.device, renderer.sync[i].image_available, NULL);
                renderer.sync[i].image_available = NULL;
            }

            if (renderer.sync[i].render_finished) {
                vkDestroySemaphore(
                        renderer.device, renderer.sync[i].render_finished, NULL);
                renderer.sync[i].render_finished = NULL;
            }

            if (renderer.sync[i].in_flight) {
                vkDestroyFence(
                        renderer.device, renderer.sync[i].in_flight, NULL);
                renderer.sync[i].in_flight = NULL;
            }
        }

        free(renderer.sync);
    }

    if (renderer.command_pool) {
        vkDestroyCommandPool(renderer.device, renderer.command_pool, NULL);
    }

    if (renderer.framebuffers) {
        for (uint32_t i = 0; i < renderer.n_swap_chain_images; i++) {
            if (renderer.framebuffers[i]) {
                vkDestroyFramebuffer(
                        renderer.device, renderer.framebuffers[i], NULL);
                renderer.framebuffers[i] = NULL;
            }
        }
        free(renderer.framebuffers);
    }

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
        for (size_t i = 0; i < renderer.n_layers; i++) {
            free((char *)renderer.layers[i]);
        }
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

    renderer.initialized = false;
}

void renderer_loop()
{
    if (!renderer.initialized) {
        return;
    }
    while (!glfwWindowShouldClose(renderer.window)) {
        glfwPollEvents();
        if (renderer_draw_frame()) {
            return;
        }
    }
    vkDeviceWaitIdle(renderer.device);
}
