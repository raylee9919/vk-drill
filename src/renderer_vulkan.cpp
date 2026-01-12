/* ========================================================================

   (C) Copyright 2025 by Sung Woo Lee, All Rights Reserved.

   This software is provided 'as-is', without any express or implied
   warranty. In no event will the authors be held liable for any damages
   arising from the use of this software.

   ======================================================================== */

#include "renderer_vulkan.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

struct Uniform_Buffer_Object {
    m4x4 ortho;
};

function m4x4
vk_perspective(f32 near_, f32 far_, f32 focal_length, f32 width, f32 height) {
    f32 f = focal_length;
    f32 N = near_;
    f32 F = far_;
    f32 a = safe_ratio(2.0f * f, 1.92f);
    f32 b = safe_ratio(2.0f * f, 1.08f);
    f32 c = safe_ratio(F, N - F);
    f32 d = safe_ratio(N*F, N - F);
    m4x4 P = {{
        { a,  0,  0,  0},
        { 0, -b,  0,  0},
        { 0,  0,  c,  d},
        { 0,  0, -1,  0}
    }};
    return P;
}

function m4x4
vk_orthographic(f32 width, f32 height) {
    f32 w = 2.0f / width;
    f32 h = 2.0f / height;
    m4x4 O = {{
        {w, 0, 0,-1},
        {0, h, 0,-1},
        {0, 0, 1, 0},
        {0, 0, 0, 1}
    }};

    return O;
}

function u32
vk_query_available_layer_count(void) {
    u32 result;
    vkEnumerateInstanceLayerProperties(&result, 0);
    return result;
}

function void
vk_query_available_layers(VkLayerProperties *layers) {
    u32 layer_count;
    vkEnumerateInstanceLayerProperties(&layer_count, layers);
}

function b32
vk_is_layer_available(const char *layer, VkLayerProperties *available_layers, u32 available_layer_count) {
    b32 result = false;
    for (u32 i = 0; i < available_layer_count; ++i) {
        if (cstring_equal(available_layers[i].layerName, layer)) {
            result = true;
            break;
        }
    }
    return result;
}

function u32
vk_query_physical_device_count(VkInstance instance) {
    u32 result;
    vkEnumeratePhysicalDevices(instance, &result, 0);
    return result;
}

function void
vk_query_physical_devices(VkInstance instance, VkPhysicalDevice *physical_devices) {
    u32 physical_device_count;
    ASSERT(vkEnumeratePhysicalDevices(instance, &physical_device_count, physical_devices) == VK_SUCCESS);
}

function void
vk_sort_physical_devices(VkPhysicalDevice *physical_devices, u32 physical_device_count) {
    s32 *scores = (s32 *)os.alloc(sizeof(s32)*physical_device_count);
    SCOPE_EXIT(os.free(scores));
    zeroarray(scores, physical_device_count);

    for (u32 i = 0; i < physical_device_count; ++i) {
        VkPhysicalDevice physical_device = physical_devices[i];
        physical_device;
        //@TODO: Set scores.
    }

    // @TODO: Sort physical devices by score.
}

function void
vk_create_device(Vulkan *vk) {
    Vk_Queue_Family queue_families[256];
    u32 queue_create_info_count = 0;

    {
        b32 already_in = false;
        for (u32 i = 0; i < queue_create_info_count; ++i) {
            if (queue_families[i].index == vk->graphics_queue_family.index) {
                already_in = true;
                break;
            }
        }
        if (!already_in) {
            queue_families[queue_create_info_count++] = vk->graphics_queue_family;
        }
    }

    {
        b32 already_in = false;
        for (u32 i = 0; i < queue_create_info_count; ++i) {
            if (queue_families[i].index == vk->present_queue_family.index) {
                already_in = true;
                break;
            }
        }
        if (!already_in) {
            queue_families[queue_create_info_count++] = vk->present_queue_family;
        }
    }

    u32 total_queue_count = 0;
    for (u32 i = 0; i < queue_create_info_count; ++i) {
        total_queue_count += queue_families[i].queue_count;
    }

    VkDeviceQueueCreateInfo *queue_create_infos = (VkDeviceQueueCreateInfo *)os.alloc(sizeof(VkDeviceQueueCreateInfo) * queue_create_info_count);
    SCOPE_EXIT(os.free(queue_create_infos));

    size_t priority_begin = 0;
    float *priorities = (float *)os.alloc(sizeof(float) * total_queue_count);
    SCOPE_EXIT(os.free(priorities));

    for (u32 i = 0; i < queue_create_info_count; ++i) {
        queue_create_infos[i].sType             = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_infos[i].queueFamilyIndex  = queue_families[i].index;
        queue_create_infos[i].queueCount        = queue_families[i].queue_count;
        queue_create_infos[i].pQueuePriorities  = priorities + priority_begin;
        priority_begin += queue_families[i].queue_count;
    }

    const char *extensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
    };

    // @TODO: This only turns on Core 1.0 features.
    // For future core version features and extension features, 
    // https://docs.vulkan.org/guide/latest/enabling_features.html
    vk->physical_device_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

    VkPhysicalDeviceDescriptorIndexingFeatures descriptor_indexing_feat{};
    descriptor_indexing_feat.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
    vk->physical_device_features.pNext = &descriptor_indexing_feat;

    vkGetPhysicalDeviceFeatures2(vk->physical_device, &vk->physical_device_features);


    VkDeviceCreateInfo device_create_info{};
    device_create_info.sType                    = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.pNext                    = &vk->physical_device_features;
    device_create_info.queueCreateInfoCount     = queue_create_info_count;
    device_create_info.pQueueCreateInfos        = queue_create_infos;
    // @TODO: How the fuck extensions, features work?
    device_create_info.enabledExtensionCount    = arraycount(extensions);
    device_create_info.ppEnabledExtensionNames  = extensions;
    device_create_info.pEnabledFeatures         = 0;

    ASSERT(vkCreateDevice(vk->physical_device, &device_create_info, 0, &vk->device) == VK_SUCCESS);
}

function void
vk_get_queue_handles_from_device(Vulkan *vk) {
    vkGetDeviceQueue(vk->device, vk->graphics_queue_family.index, 0, &vk->graphics_queue);
    vkGetDeviceQueue(vk->device, vk->present_queue_family.index, 0, &vk->present_queue);
}

function VkShaderModule
vk_create_shader_module(VkDevice device, u8 *data, size_t size) {
    VkShaderModuleCreateInfo create_info{};
    create_info.sType     = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize  = size;
    create_info.pCode     = (u32 *)data;

    VkShaderModule result{};
    ASSERT(vkCreateShaderModule(device, &create_info, 0, &result) == VK_SUCCESS);
    return result;
}

function VkCommandPool
vk_create_command_pool(VkDevice device, u32 queue_family_index, VkCommandPoolCreateFlags flags) {
    VkCommandPool result{};

    VkCommandPoolCreateInfo create_info{};
    create_info.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    create_info.flags            = flags;
    create_info.queueFamilyIndex = queue_family_index;

    ASSERT(vkCreateCommandPool(device, &create_info, 0, &result) == VK_SUCCESS);

    return result;
}

function VkSemaphore
vk_create_semaphore(VkDevice device) {
    VkSemaphore result{};
    VkSemaphoreCreateInfo semaphore_create_info{};
    semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    ASSERT(vkCreateSemaphore(device, &semaphore_create_info, 0, &result) == VK_SUCCESS);
    return result;
}

function VkFence
vk_create_fence(VkDevice device, b32 init_signaled) {
    VkFence result{};
    VkFenceCreateInfo fence_create_info{};
    fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    if (init_signaled) {
        fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    }
    vkCreateFence(device, &fence_create_info, 0, &result);
    return result;
}

function VkCommandBuffer
vk_begin_one_time_command(VkDevice device, VkCommandPool pool) {
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool        = pool;
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer result{};
    vkAllocateCommandBuffers(device, &alloc_info, &result);

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(result, &begin_info);

    return result;
}

function void
vk_end_one_time_command(VkDevice device, VkCommandBuffer cmd_buffer, VkCommandPool pool, VkQueue queue) {
    vkEndCommandBuffer(cmd_buffer);

    VkSubmitInfo submit_info{};
    submit_info.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers    = &cmd_buffer;

    vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    vkFreeCommandBuffers(device, pool, 1, &cmd_buffer);
}

function u32
vk_get_memory_type_index(VkPhysicalDeviceMemoryProperties *physical_device_memory_properties, u32 type_filter, VkMemoryPropertyFlags properties) {
    for (u32 i = 0; i < physical_device_memory_properties->memoryTypeCount; ++i) {
        if ( (type_filter & (1 << i)) && (physical_device_memory_properties->memoryTypes[i].propertyFlags & properties) ) {
            return i;
        }
    }
    INVALID_CODE_PATH;
    return 0;
}

function void
vk_copy_buffer(Vulkan *vk, VkBuffer src, VkBuffer dst, VkDeviceSize size) {
    VkCommandBuffer command_buffer = vk_begin_one_time_command(vk->device, vk->command_pool);

    VkBufferCopy copy_region{};
    copy_region.srcOffset = 0;
    copy_region.dstOffset = 0;
    copy_region.size      = size;
    vkCmdCopyBuffer(command_buffer, src, dst, 1, &copy_region);

    vk_end_one_time_command(vk->device, command_buffer, vk->command_pool, vk->graphics_queue);
}

function void
vk_alloc_buffer(Vulkan *vk, VkBuffer *buffer, VkDeviceMemory *buffer_memory,
                 VkDeviceSize size, u32 usage, VkSharingMode sharing_mode, u32 memory_property_flags) {
    VkBufferCreateInfo buffer_create_info{};
    buffer_create_info.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_create_info.flags       = 0;
    buffer_create_info.size        = size;
    buffer_create_info.usage       = usage;
    buffer_create_info.sharingMode = sharing_mode;

    ASSERT(vkCreateBuffer(vk->device, &buffer_create_info, 0, buffer) == VK_SUCCESS);

    VkMemoryRequirements memreq{};
    vkGetBufferMemoryRequirements(vk->device, *buffer, &memreq);

    VkMemoryAllocateInfo buffer_alloc_info{};
    buffer_alloc_info.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    buffer_alloc_info.allocationSize  = memreq.size;
    buffer_alloc_info.memoryTypeIndex = vk_get_memory_type_index(&vk->memory_properties,
                                                                 memreq.memoryTypeBits,
                                                                 memory_property_flags);

    VkResult res = vkAllocateMemory(vk->device, &buffer_alloc_info, 0, buffer_memory);
    if (res != VK_SUCCESS) {
        printf("%d\n", res);
        INVALID_CODE_PATH;
    }

    vkBindBufferMemory(vk->device, *buffer, *buffer_memory, 0);
}

function void
vk_copy_to_buffer(Vulkan *vk, void *cpu_memory, VkDeviceSize offset, VkDeviceSize range, VkDeviceMemory buffer_memory) {
    void *data;
    vkMapMemory(vk->device, buffer_memory, offset, range, 0, &data);
    copy(cpu_memory, data, range);
    vkUnmapMemory(vk->device, buffer_memory);
}

function void
vk_alloc_image(Vulkan *vk, VkImage *image, VkDeviceMemory *image_memory,
               u32 width, u32 height, VkFormat format, u32 usage) {
    VkImageCreateInfo image_create_info{};
    image_create_info.sType             = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_create_info.flags             = 0;
    image_create_info.imageType         = VK_IMAGE_TYPE_2D;
    image_create_info.format            = format;
    image_create_info.extent.width      = width;
    image_create_info.extent.height     = height;
    image_create_info.extent.depth      = 1;
    image_create_info.mipLevels         = 1;
    image_create_info.arrayLayers       = 1;
    image_create_info.samples           = VK_SAMPLE_COUNT_1_BIT;
    image_create_info.tiling            = VK_IMAGE_TILING_OPTIMAL;
    image_create_info.usage             = usage;
    image_create_info.sharingMode       = VK_SHARING_MODE_EXCLUSIVE;
    image_create_info.initialLayout     = VK_IMAGE_LAYOUT_UNDEFINED;
    ASSERT(vkCreateImage(vk->device, &image_create_info, 0, image) == VK_SUCCESS);

    VkMemoryRequirements memreq{};
    vkGetImageMemoryRequirements(vk->device, *image, &memreq);

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType            = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize   = memreq.size;
    alloc_info.memoryTypeIndex  = vk_get_memory_type_index(&vk->memory_properties, memreq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    ASSERT(vkAllocateMemory(vk->device, &alloc_info, 0, image_memory) == VK_SUCCESS);

    vkBindImageMemory(vk->device, *image, *image_memory, 0);
}

function VkImageView
vk_create_image_view(Vulkan *vk, VkImage image, VkFormat format, u32 aspect_mask) {
    VkImageView result{};

    VkImageViewCreateInfo image_view_info{};
    image_view_info.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_info.image                           = image;
    image_view_info.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    image_view_info.format                          = format;
    image_view_info.subresourceRange.aspectMask     = aspect_mask;
    image_view_info.subresourceRange.baseMipLevel   = 0;
    image_view_info.subresourceRange.levelCount     = 1;
    image_view_info.subresourceRange.baseArrayLayer = 0;
    image_view_info.subresourceRange.layerCount     = 1;
    ASSERT(vkCreateImageView(vk->device, &image_view_info, 0, &result) == VK_SUCCESS);

    return result;
}

function void
vk_image_transition(Vulkan *vk, VkImage image, VkImageLayout old_layout, VkImageLayout new_layout) {
    VkCommandBuffer command_buffer = vk_begin_one_time_command(vk->device, vk->command_pool);

    VkImageMemoryBarrier barrier{};
    barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout                       = old_layout;
    barrier.newLayout                       = new_layout;
    barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.image                           = image;
    barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT; 
    barrier.subresourceRange.baseMipLevel   = 0; 
    barrier.subresourceRange.levelCount     = 1; 
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 1;

    VkPipelineStageFlags src_stage{};
    VkPipelineStageFlags dst_stage{};

    switch(old_layout) {
        case VK_IMAGE_LAYOUT_UNDEFINED: {
            barrier.srcAccessMask   = 0;
            src_stage               = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        } break;

        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: {
            barrier.srcAccessMask   = VK_ACCESS_TRANSFER_WRITE_BIT;
            src_stage               = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } break;

        INVALID_DEFAULT_CASE;
    }

    switch(new_layout) {
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: {
            barrier.dstAccessMask   = VK_ACCESS_TRANSFER_WRITE_BIT;
            dst_stage               = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } break;

        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: {
            barrier.dstAccessMask   = VK_ACCESS_SHADER_READ_BIT;
            dst_stage               = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        } break;

        INVALID_DEFAULT_CASE;
    }

    vkCmdPipelineBarrier(command_buffer, src_stage, dst_stage, 0, 0, 0, 0, 0, 1, &barrier);

    vk_end_one_time_command(vk->device, command_buffer, vk->command_pool, vk->graphics_queue);
}

function void
vk_recreate_swapchain(Vulkan *vk, u32 width, u32 height) {
    //
    // Cleanup
    //
    for (u32 i = 0; i < vk->swapchain_image_count; ++i) {
        vkDestroyFramebuffer(vk->device, vk->swapchain_framebuffers[i], 0);
        vkDestroyImageView(vk->device, vk->swapchain_image_views[i], 0);
    }
    vkDestroySwapchainKHR(vk->device, vk->swapchain, 0);
    vkDestroyImageView(vk->device, vk->depth_image_view, 0);
    vkDestroyImage(vk->device, vk->depth_image, 0);


    //
    // Depth Image
    //
    vk_alloc_image(vk, &vk->depth_image, &vk->depth_image_memory,
                   width, height,
                   VK_FORMAT_D24_UNORM_S8_UINT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    vk->depth_image_view = vk_create_image_view(vk, vk->depth_image, VK_FORMAT_D24_UNORM_S8_UINT, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);

    //
    // Create SwapChain
    //
    VkSurfaceCapabilitiesKHR surface_capabilities{};
    ASSERT(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk->physical_device, vk->surface, &surface_capabilities) == VK_SUCCESS);

    u32 swapchain_min_image_count = surface_capabilities.minImageCount + 1;
    if (surface_capabilities.maxImageCount > 0 && swapchain_min_image_count > surface_capabilities.maxImageCount) {
        swapchain_min_image_count = surface_capabilities.maxImageCount;
    }

    vk->swapchain_image_extent.width  = MIN(MAX(width,  surface_capabilities.minImageExtent.width),  surface_capabilities.maxImageExtent.width);
    vk->swapchain_image_extent.height = MIN(MAX(height, surface_capabilities.minImageExtent.height), surface_capabilities.maxImageExtent.height);

    VkSwapchainCreateInfoKHR swapchain_create_info{};
    swapchain_create_info.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_create_info.pNext            = 0;
    swapchain_create_info.flags            = 0;
    swapchain_create_info.surface          = vk->surface;
    swapchain_create_info.minImageCount    = swapchain_min_image_count;
    swapchain_create_info.imageFormat      = vk->swapchain_image_format;
    swapchain_create_info.imageColorSpace  = vk->swapchain_image_color_space;
    swapchain_create_info.imageExtent      = vk->swapchain_image_extent;
    swapchain_create_info.imageArrayLayers = 1;
    swapchain_create_info.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchain_create_info.preTransform     = surface_capabilities.currentTransform;
    swapchain_create_info.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_create_info.presentMode      = vk->present_mode;
    swapchain_create_info.clipped          = VK_TRUE;
    swapchain_create_info.oldSwapchain     = VK_NULL_HANDLE;

    u32 queue_family_indices[] = {vk->graphics_queue_family.index, vk->present_queue_family.index};
    if (vk->graphics_queue_family.index != vk->present_queue_family.index) {
        swapchain_create_info.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        swapchain_create_info.queueFamilyIndexCount = 2;
        swapchain_create_info.pQueueFamilyIndices   = queue_family_indices;
    } else {
        swapchain_create_info.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
        swapchain_create_info.queueFamilyIndexCount = 0;
        swapchain_create_info.pQueueFamilyIndices   = 0;
    }

    ASSERT(vkCreateSwapchainKHR(vk->device, &swapchain_create_info, 0, &vk->swapchain) == VK_SUCCESS);

    u32 actual_swapchain_image_count;
    vkGetSwapchainImagesKHR(vk->device, vk->swapchain, &actual_swapchain_image_count, 0);
    vk->swapchain_image_count = actual_swapchain_image_count;

    os.free(vk->swapchain_images);
    vk->swapchain_images = (VkImage *)os.alloc(sizeof(VkImage) * actual_swapchain_image_count);
    ASSERT(vkGetSwapchainImagesKHR(vk->device, vk->swapchain, &actual_swapchain_image_count, vk->swapchain_images) == VK_SUCCESS);


    //
    // Create Image Views
    //
    os.free(vk->swapchain_image_views);
    vk->swapchain_image_views = (VkImageView *)os.alloc(sizeof(VkImageView) * vk->swapchain_image_count);
    for (u32 i = 0; i < vk->swapchain_image_count; ++i) {
        vk->swapchain_image_views[i] = vk_create_image_view(vk, vk->swapchain_images[i], vk->swapchain_image_format, VK_IMAGE_ASPECT_COLOR_BIT);
    }

    //
    // Create Framebuffers
    //
    os.free(vk->swapchain_framebuffers);
    vk->swapchain_framebuffers = (VkFramebuffer *)os.alloc(sizeof(VkFramebuffer) * vk->swapchain_image_count);

    for (u32 i = 0; i < vk->swapchain_image_count; i++) {
        VkImageView color_depth_stencil_attachments[] = {vk->swapchain_image_views[i], vk->depth_image_view};
        VkFramebufferCreateInfo framebuffer_create_info{};
        framebuffer_create_info.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_create_info.flags           = 0;
        framebuffer_create_info.renderPass      = vk->render_pass;
        framebuffer_create_info.attachmentCount = arraycount(color_depth_stencil_attachments);
        framebuffer_create_info.pAttachments    = color_depth_stencil_attachments;
        framebuffer_create_info.width           = vk->swapchain_image_extent.width;
        framebuffer_create_info.height          = vk->swapchain_image_extent.height;
        framebuffer_create_info.layers          = 1;
        ASSERT(vkCreateFramebuffer(vk->device, &framebuffer_create_info, 0, vk->swapchain_framebuffers + i) == VK_SUCCESS);
    }
}

function void
vk_copy_buffer_to_image(Vulkan *vk, VkBuffer buffer, VkImage image, u32 width, u32 height) {
    VkCommandBuffer command_buffer = vk_begin_one_time_command(vk->device, vk->command_pool);

    VkBufferImageCopy region{};
    region.bufferOffset                     = 0;
    region.bufferRowLength                  = 0;
    region.bufferImageHeight                = 0;
    region.imageSubresource.aspectMask      = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel        = 0;
    region.imageSubresource.baseArrayLayer  = 0;
    region.imageSubresource.layerCount      = 1;
    region.imageOffset                      = {0, 0, 0};
    region.imageExtent.width                = width;
    region.imageExtent.height               = height;
    region.imageExtent.depth                = 1;

    vkCmdCopyBufferToImage(command_buffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    vk_end_one_time_command(vk->device, command_buffer, vk->command_pool, vk->graphics_queue);
}

function void
vk_create_image(Vulkan *vk, Image data) {
    Vk_Image_Unit unit{};

    VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
    u32 aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;

    VkDeviceSize size = data.width * data.height * 4;

    VkBuffer staging_buffer{};
    VkDeviceMemory staging_buffer_memory{};
    vk_alloc_buffer(vk, &staging_buffer, &staging_buffer_memory, size,
                     VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_SHARING_MODE_EXCLUSIVE,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    SCOPE_EXIT(vkDestroyBuffer(vk->device, staging_buffer, 0));
    SCOPE_EXIT(vkFreeMemory(vk->device, staging_buffer_memory, 0));

    vk_copy_to_buffer(vk, data.data, 0, size, staging_buffer_memory);

    vk_alloc_image(vk, &unit.image, &unit.memory, data.width, data.height,
                   format, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

    vk_image_transition(vk, unit.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    vk_copy_buffer_to_image(vk, staging_buffer, unit.image, data.width, data.height);
    vk_image_transition(vk, unit.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    unit.view = vk_create_image_view(vk, unit.image, format, aspect_mask);

    vk->image_hash_table.insert(data.id, unit);
}

function void
vk_update_image_descriptor(Vulkan *vk, VkDescriptorSet descriptor_set, u32 image_id, u32 binding) {
    auto hash_result = vk->image_hash_table.get(image_id);
    ASSERT(hash_result.found);
    auto unit = hash_result.value;

    VkDescriptorImageInfo image_info{};
    image_info.imageLayout  = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    image_info.imageView    = unit.view;
    image_info.sampler      = vk->DEBUG_texture_sampler;

    VkWriteDescriptorSet descriptor_write{};

    descriptor_write.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptor_write.dstSet           = descriptor_set;
    descriptor_write.dstBinding       = binding;
    descriptor_write.dstArrayElement  = 0;
    descriptor_write.descriptorCount  = 1;
    descriptor_write.descriptorType   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptor_write.pBufferInfo      = 0;
    descriptor_write.pImageInfo       = &image_info;
    descriptor_write.pTexelBufferView = 0;

    vkUpdateDescriptorSets(vk->device, 1, &descriptor_write, 0, 0);
}

function void
vk_draw(Vulkan *vk) {
    vkWaitForFences(vk->device, 1, &vk->in_flight_fence, VK_TRUE, UINT64_MAX);
    vkResetFences(vk->device, 1, &vk->in_flight_fence);


    while (!empty(&renderer.image_create_queue)) {
        Image image = dequeue(&renderer.image_create_queue);
        vk_create_image(vk, image);
    }



    //
    // Begin
    //
    u32 swapchain_image_index;
    vkAcquireNextImageKHR(vk->device, vk->swapchain, UINT64_MAX, vk->image_available_semaphore, VK_NULL_HANDLE, &swapchain_image_index);

    /* Begin */
    vkResetCommandBuffer(vk->command_buffer, 0);

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags            = 0;
    begin_info.pInheritanceInfo = 0;

    vkBeginCommandBuffer(vk->command_buffer, &begin_info);

    VkClearValue clear_values[2]{}; 
    clear_values[0].color = {0.02f, 0.02f, 0.02f, 1.0f};
    clear_values[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo render_pass_begin_info{};
    render_pass_begin_info.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_begin_info.renderPass        = vk->render_pass;
    render_pass_begin_info.framebuffer       = vk->swapchain_framebuffers[swapchain_image_index];
    render_pass_begin_info.renderArea.offset = {0, 0};
    render_pass_begin_info.renderArea.extent = vk->swapchain_image_extent;
    render_pass_begin_info.clearValueCount   = arraycount(clear_values);
    render_pass_begin_info.pClearValues      = clear_values;

    vkCmdBeginRenderPass(vk->command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(vk->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk->simple_pipeline);

    VkViewport viewport{};
    viewport.x          = 0.0f;
    viewport.y          = 0.0f;
    viewport.width      = (float)vk->swapchain_image_extent.width;
    viewport.height     = (float)vk->swapchain_image_extent.height;
    viewport.minDepth   = 0.0f;
    viewport.maxDepth   = 1.0f;
    vkCmdSetViewport(vk->command_buffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = vk->swapchain_image_extent;
    vkCmdSetScissor(vk->command_buffer, 0, 1, &scissor);

    /* Vertex Buffer */
    {
        VkDeviceSize size = renderer.vertices.count * sizeof(Vertex);

        VkBuffer staging_buffer{};
        VkDeviceMemory staging_buffer_memory{};

        vk_alloc_buffer(vk, &staging_buffer, &staging_buffer_memory, size,
                        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                        VK_SHARING_MODE_EXCLUSIVE,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        SCOPE_EXIT(vkDestroyBuffer(vk->device, staging_buffer, 0));
        SCOPE_EXIT(vkFreeMemory(vk->device, staging_buffer_memory, 0));

        vk_copy_to_buffer(vk, renderer.vertices.data, 0, size, staging_buffer_memory);
        vk_copy_buffer(vk, staging_buffer, vk->vertex_buffer, size);
    }
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(vk->command_buffer, 0, 1, &vk->vertex_buffer, offsets);


    u32 drawn_triangle_count = 0;
#if 1 /* Optimized Draw */
    for (u32 i = 0; i < renderer.drawcall_vertex_count.count; ++i) {
        u32 vertex_count_to_draw = renderer.drawcall_vertex_count.data[i];
#else /* Unoptimized */
    for (u32 i = 0; i < renderer.vertices.count; i += 3) {
        u32 vertex_count_to_draw = 3;
#endif

        /* Update descriptor set before binding */
        u32 image_id = renderer.sort_keys.data[drawn_triangle_count].image_id;
        u32 sampler_binding = 1;
        vk_update_image_descriptor(vk, vk->descriptor_set, image_id, sampler_binding);

        /* Descriptor */
        vkCmdBindDescriptorSets(vk->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk->pipeline_layout, 0, 1, &vk->descriptor_set, 0, 0);

        /* Uniform */
        Uniform_Buffer_Object ubo{};
        ubo.ortho = vk_orthographic((f32)vk->swapchain_image_extent.width, (f32)vk->swapchain_image_extent.height);

        copy(&ubo, vk->uniform_buffer_mapped, sizeof(ubo));

        vkCmdDraw(vk->command_buffer, vertex_count_to_draw, 1, drawn_triangle_count*3, 0);

        drawn_triangle_count += (vertex_count_to_draw/3);
    }

    /* End */
    vkCmdEndRenderPass(vk->command_buffer);
    ASSERT(vkEndCommandBuffer(vk->command_buffer) == VK_SUCCESS);



    VkSemaphore wait_semaphores[]       = {vk->image_available_semaphore};
    VkSemaphore signal_semaphores[]     = {vk->render_finished_semaphore};
    VkPipelineStageFlags wait_stages[]  = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

    VkSubmitInfo submit_info{};
    submit_info.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount   = 1;
    submit_info.pWaitSemaphores      = wait_semaphores;
    submit_info.pWaitDstStageMask    = wait_stages;
    submit_info.commandBufferCount   = 1;
    submit_info.pCommandBuffers      = &vk->command_buffer;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores    = signal_semaphores;

    ASSERT(vkQueueSubmit(vk->graphics_queue, 1, &submit_info, vk->in_flight_fence) == VK_SUCCESS);


    VkPresentInfoKHR present_info{};
    present_info.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores    = signal_semaphores;
    present_info.swapchainCount     = 1;
    present_info.pSwapchains        = &vk->swapchain;
    present_info.pImageIndices      = &swapchain_image_index;
    present_info.pResults           = 0;

    vkQueuePresentKHR(vk->present_queue, &present_info);
}

function void
vk_init(Vulkan *vk) {
    vkGetPhysicalDeviceProperties(vk->physical_device, &vk->physical_device_properties);
    vkGetPhysicalDeviceMemoryProperties(vk->physical_device, &vk->memory_properties);
    vk_create_device(vk);
    vk_get_queue_handles_from_device(vk);

    VkSurfaceCapabilitiesKHR surface_capabilities{};
    ASSERT(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk->physical_device, vk->surface, &surface_capabilities) == VK_SUCCESS);

    u32 present_mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(vk->physical_device, vk->surface, &present_mode_count, 0);
    VkPresentModeKHR *present_modes = (VkPresentModeKHR *)os.alloc(sizeof(VkPresentModeKHR) * present_mode_count);
    SCOPE_EXIT(os.free(present_modes));
    ASSERT(vkGetPhysicalDeviceSurfacePresentModesKHR(vk->physical_device, vk->surface, &present_mode_count, present_modes) == VK_SUCCESS);

    // @NOTE: FIFO is always available.
    vk->present_mode = VK_PRESENT_MODE_FIFO_KHR;
    for (u32 i = 0; i < present_mode_count; ++i) {
        VkPresentModeKHR mode = present_modes[i];
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            vk->present_mode = mode;
            break;
        }
    }

    u32 swapchain_min_image_count = surface_capabilities.minImageCount + 1;
    if (surface_capabilities.maxImageCount > 0 && swapchain_min_image_count > surface_capabilities.maxImageCount) {
        swapchain_min_image_count = surface_capabilities.maxImageCount;
    }

    if (surface_capabilities.currentExtent.width == 0xFFFFFFFF) {
        // @NOTE: In some OS (e.g. Wayland), swap chain extent determines surface extent.
        // @TODO: Should dimension be passed initially from OS layer?
        vk->swapchain_image_extent = {640, 480};
        vk->swapchain_image_extent.width = MIN(MAX(vk->swapchain_image_extent.width, surface_capabilities.minImageExtent.width), surface_capabilities.maxImageExtent.width);
        vk->swapchain_image_extent.height = MIN(MAX(vk->swapchain_image_extent.height, surface_capabilities.minImageExtent.height), surface_capabilities.maxImageExtent.height);
    } else {
        vk->swapchain_image_extent = surface_capabilities.currentExtent;
    }


    // @NOTE: VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT is always there.
    VkImageUsageFlags desired_image_flags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    ASSERT((surface_capabilities.supportedUsageFlags & desired_image_flags) == desired_image_flags);


    VkSurfaceFormatKHR *surface_formats{};
    u32 surface_format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(vk->physical_device, vk->surface, &surface_format_count, 0);
    surface_formats = (VkSurfaceFormatKHR *)os.alloc(sizeof(VkSurfaceFormatKHR) * surface_format_count);
    SCOPE_EXIT(os.free(surface_formats));
    ASSERT(vkGetPhysicalDeviceSurfaceFormatsKHR(vk->physical_device, vk->surface, &surface_format_count, surface_formats) == VK_SUCCESS);
    ASSERT(surface_format_count > 0);

    VkSurfaceFormatKHR desired_surface_format{};
    desired_surface_format.format = VK_FORMAT_B8G8R8A8_SRGB;
    desired_surface_format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

    VkFormat image_format{};
    VkColorSpaceKHR image_color_space{};

    if (surface_format_count == 1 && surface_formats[0].format == VK_FORMAT_UNDEFINED) {
        image_format      = desired_surface_format.format;
        image_color_space = desired_surface_format.colorSpace;
    } else {
        b32 found = false;
        for (u32 i = 0; i < surface_format_count; ++i) {
            VkSurfaceFormatKHR surface_format = surface_formats[i];
            if (surface_format.format == desired_surface_format.format) {
                if (surface_format.colorSpace == desired_surface_format.colorSpace) {
                    image_format      = desired_surface_format.format;
                    image_color_space = desired_surface_format.colorSpace;
                    found = true;
                    break;
                } else {
                    image_format      = desired_surface_format.format;
                    image_color_space = surface_format.colorSpace;
                    found = true;
                }
            }
        }

        if (!found) {
            image_format = surface_formats[0].format;
            image_color_space = surface_formats[0].colorSpace;
        }
    }

    vk->swapchain_image_format      = image_format;
    vk->swapchain_image_color_space = image_color_space;

    VkSwapchainCreateInfoKHR swapchain_create_info{};
    swapchain_create_info.sType                 = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_create_info.pNext                 = 0;
    swapchain_create_info.flags                 = 0;
    swapchain_create_info.surface               = vk->surface;
    swapchain_create_info.minImageCount         = swapchain_min_image_count;
    swapchain_create_info.imageFormat           = vk->swapchain_image_format;
    swapchain_create_info.imageColorSpace       = vk->swapchain_image_color_space;
    swapchain_create_info.imageExtent           = vk->swapchain_image_extent;
    swapchain_create_info.imageArrayLayers      = 1;
    swapchain_create_info.imageUsage            = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchain_create_info.preTransform          = surface_capabilities.currentTransform;
    swapchain_create_info.compositeAlpha        = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_create_info.presentMode           = vk->present_mode;
    swapchain_create_info.clipped               = VK_TRUE;
    swapchain_create_info.oldSwapchain          = VK_NULL_HANDLE;

    u32 queue_family_indices[] = {vk->graphics_queue_family.index, vk->present_queue_family.index};
    if (vk->graphics_queue_family.index != vk->present_queue_family.index) {
        swapchain_create_info.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        swapchain_create_info.queueFamilyIndexCount = 2;
        swapchain_create_info.pQueueFamilyIndices   = queue_family_indices;
    } else {
        swapchain_create_info.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
        swapchain_create_info.queueFamilyIndexCount = 0;
        swapchain_create_info.pQueueFamilyIndices   = 0;
    }

    ASSERT(vkCreateSwapchainKHR(vk->device, &swapchain_create_info, 0, &vk->swapchain) == VK_SUCCESS);



    // @NOTE: Drivers may create more images than we requested.
    u32 actual_swapchain_image_count;
    vkGetSwapchainImagesKHR(vk->device, vk->swapchain, &actual_swapchain_image_count, 0);
    vk->swapchain_image_count = actual_swapchain_image_count;
    vk->swapchain_images = (VkImage *)os.alloc(sizeof(VkImage) * actual_swapchain_image_count);
    ASSERT(vkGetSwapchainImagesKHR(vk->device, vk->swapchain, &actual_swapchain_image_count, vk->swapchain_images) == VK_SUCCESS);



    vk->swapchain_image_views = (VkImageView *)os.alloc(sizeof(VkImageView) * vk->swapchain_image_count);
    for (u32 i = 0; i < vk->swapchain_image_count; ++i) {
        vk->swapchain_image_views[i] = vk_create_image_view(vk, vk->swapchain_images[i], vk->swapchain_image_format, VK_IMAGE_ASPECT_COLOR_BIT);
    }


    //
    // Depth Image
    //
    vk_alloc_image(vk, &vk->depth_image, &vk->depth_image_memory,
                   vk->swapchain_image_extent.width, vk->swapchain_image_extent.height,
                   VK_FORMAT_D24_UNORM_S8_UINT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    vk->depth_image_view = vk_create_image_view(vk, vk->depth_image, VK_FORMAT_D24_UNORM_S8_UINT, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);






    //
    // Shader Module
    //
    Buffer vs_spv = read_entire_file("../data/shaders/simple_vs.spv");
    Buffer fs_spv = read_entire_file("../data/shaders/simple_fs.spv");
    VkShaderModule vs_module = vk_create_shader_module(vk->device, vs_spv.data, vs_spv.size);
    VkShaderModule fs_module = vk_create_shader_module(vk->device, fs_spv.data, fs_spv.size);

    VkPipelineShaderStageCreateInfo vs_stage_info{};
    vs_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vs_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vs_stage_info.module = vs_module;
    vs_stage_info.pName = "main";

    VkPipelineShaderStageCreateInfo fs_stage_info{};
    fs_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fs_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fs_stage_info.module = fs_module;
    fs_stage_info.pName = "main";

    VkPipelineShaderStageCreateInfo shader_stages[] = {vs_stage_info, fs_stage_info};


    VkVertexInputBindingDescription v_binding{};
    v_binding.binding   = 0;
    v_binding.stride    = sizeof(Vertex);
    v_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription v_attributes[3];
    v_attributes[0].binding  = 0;
    v_attributes[0].location = 0;
    v_attributes[0].format   = VK_FORMAT_R32G32_SFLOAT;
    v_attributes[0].offset   = offset_of(Vertex, position);
    v_attributes[1].binding  = 0;
    v_attributes[1].location = 1;
    v_attributes[1].format   = VK_FORMAT_R32G32B32A32_SFLOAT;
    v_attributes[1].offset   = offset_of(Vertex, color);
    v_attributes[2].binding  = 0;
    v_attributes[2].location = 2;
    v_attributes[2].format   = VK_FORMAT_R32G32_SFLOAT;
    v_attributes[2].offset   = offset_of(Vertex, uv);



    VkPipelineVertexInputStateCreateInfo vertex_input_state{};
    vertex_input_state.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_state.vertexBindingDescriptionCount   = 1;
    vertex_input_state.pVertexBindingDescriptions      = &v_binding;
    vertex_input_state.vertexAttributeDescriptionCount = arraycount(v_attributes);
    vertex_input_state.pVertexAttributeDescriptions    = v_attributes;

    VkPipelineInputAssemblyStateCreateInfo input_assembly_state{};
    input_assembly_state.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_state.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly_state.primitiveRestartEnable = VK_FALSE;

    VkDynamicState dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    VkPipelineDynamicStateCreateInfo dynamic_state{};
    dynamic_state.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = arraycount(dynamic_states);
    dynamic_state.pDynamicStates    = dynamic_states;

    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount  = 1;

    VkPipelineRasterizationStateCreateInfo rasterization_state{};
    rasterization_state.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization_state.depthClampEnable        = VK_FALSE;
    rasterization_state.rasterizerDiscardEnable = VK_FALSE;
    rasterization_state.polygonMode             = VK_POLYGON_MODE_FILL;  // @NOTE: Other than POLYGON_FILL requires GPU feature.
    rasterization_state.lineWidth               = 1.0f;                  // @NOTE: > 1.0f requires GPU feature.
    rasterization_state.cullMode                = VK_CULL_MODE_BACK_BIT;
    rasterization_state.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization_state.depthBiasEnable         = VK_FALSE;
    rasterization_state.depthBiasConstantFactor = 0.0f;
    rasterization_state.depthBiasClamp          = 0.0f;
    rasterization_state.depthBiasSlopeFactor    = 0.0f;

    VkPipelineMultisampleStateCreateInfo ms_state{};
    ms_state.sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms_state.sampleShadingEnable   = VK_FALSE;
    ms_state.rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT;
    ms_state.minSampleShading      = 1.0f;
    ms_state.pSampleMask           = 0;
    ms_state.alphaToCoverageEnable = VK_FALSE;
    ms_state.alphaToOneEnable      = VK_FALSE;

    VkPipelineColorBlendAttachmentState color_blend_attachment{};
    color_blend_attachment.colorWriteMask       = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable          = VK_TRUE;
    color_blend_attachment.srcColorBlendFactor  = VK_BLEND_FACTOR_SRC_ALPHA;
    color_blend_attachment.dstColorBlendFactor  = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend_attachment.colorBlendOp         = VK_BLEND_OP_ADD;
    color_blend_attachment.srcAlphaBlendFactor  = VK_BLEND_FACTOR_ONE;
    color_blend_attachment.dstAlphaBlendFactor  = VK_BLEND_FACTOR_ZERO;
    color_blend_attachment.alphaBlendOp         = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorblend_state{};
    colorblend_state.sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorblend_state.logicOpEnable     = VK_FALSE;
    colorblend_state.logicOp           = VK_LOGIC_OP_COPY;
    colorblend_state.attachmentCount   = 1;
    colorblend_state.pAttachments      = &color_blend_attachment;
    colorblend_state.blendConstants[0] = 0.0f;
    colorblend_state.blendConstants[1] = 0.0f;
    colorblend_state.blendConstants[2] = 0.0f;
    colorblend_state.blendConstants[3] = 0.0f;

    VkPipelineDepthStencilStateCreateInfo depth_stencil_state{};
    depth_stencil_state.sType                   = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil_state.depthTestEnable         = VK_TRUE;
    depth_stencil_state.depthWriteEnable        = VK_TRUE;
    depth_stencil_state.depthCompareOp          = VK_COMPARE_OP_LESS;
    depth_stencil_state.depthBoundsTestEnable   = VK_FALSE;
    depth_stencil_state.minDepthBounds          = 0.0f;
    depth_stencil_state.maxDepthBounds          = 1.0f;
    depth_stencil_state.stencilTestEnable       = VK_FALSE;

    VkDescriptorSetLayoutBinding ubo_layout_binding{};
    ubo_layout_binding.binding              = 0;
    ubo_layout_binding.descriptorType       = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubo_layout_binding.descriptorCount      = 1;
    ubo_layout_binding.stageFlags           = VK_SHADER_STAGE_ALL_GRAPHICS; // @NOTE: Should be specify this?
    ubo_layout_binding.pImmutableSamplers   = 0;

    VkDescriptorSetLayoutBinding sampler_layout_binding{};
    sampler_layout_binding.binding              = 1;
    sampler_layout_binding.descriptorType       = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sampler_layout_binding.descriptorCount      = 1;
    sampler_layout_binding.stageFlags           = VK_SHADER_STAGE_FRAGMENT_BIT;
    sampler_layout_binding.pImmutableSamplers   = 0;

    VkDescriptorSetLayoutBinding descriptor_set_layout_bindings[] = {ubo_layout_binding, sampler_layout_binding};
    u32 binding_count = arraycount(descriptor_set_layout_bindings);

    VkDescriptorSetLayout descriptor_set_layout{};

    VkDescriptorBindingFlagsEXT flags[] = {0, 0};

    VkDescriptorSetLayoutBindingFlagsCreateInfoEXT binding_flags{};
    binding_flags.sType          = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT;
    binding_flags.bindingCount   = binding_count;
    binding_flags.pBindingFlags  = flags;

    VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info{};
    descriptor_set_layout_create_info.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptor_set_layout_create_info.pNext        = &binding_flags;
    descriptor_set_layout_create_info.flags        = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT;
    descriptor_set_layout_create_info.bindingCount = binding_count;
    descriptor_set_layout_create_info.pBindings    = descriptor_set_layout_bindings;
    ASSERT(vkCreateDescriptorSetLayout(vk->device, &descriptor_set_layout_create_info, 0, &descriptor_set_layout) == VK_SUCCESS);


    VkPipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount         = 1;
    pipeline_layout_info.pSetLayouts            = &descriptor_set_layout;
    pipeline_layout_info.pushConstantRangeCount = 0;
    pipeline_layout_info.pPushConstantRanges    = 0;

    
    vkCreatePipelineLayout(vk->device, &pipeline_layout_info, 0, &vk->pipeline_layout);

    VkAttachmentDescription color_attachment{};
    color_attachment.flags          = 0;
    color_attachment.format         = vk->swapchain_image_format;
    color_attachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment_ref{};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription depth_attachment{};
    depth_attachment.flags          = 0;
    depth_attachment.format         = VK_FORMAT_D24_UNORM_S8_UINT;
    depth_attachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    depth_attachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    depth_attachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    depth_attachment.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depth_attachment_ref{};
    depth_attachment_ref.attachment = 1;
    depth_attachment_ref.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.flags                   = 0;
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 1;
    subpass.pColorAttachments       = &color_attachment_ref;
    subpass.pDepthStencilAttachment = &depth_attachment_ref;


    VkSubpassDependency dependency{};
    dependency.srcSubpass       = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass       = 0;
    dependency.srcStageMask     = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstStageMask     = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask    = 0;
    dependency.dstAccessMask    = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkAttachmentDescription attachments[] = { color_attachment, depth_attachment };
    VkRenderPassCreateInfo render_pass_create_info{};
    render_pass_create_info.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_create_info.pNext           = 0;
    render_pass_create_info.flags           = 0;
    render_pass_create_info.attachmentCount = 2;
    render_pass_create_info.pAttachments    = attachments;
    render_pass_create_info.subpassCount    = 1;
    render_pass_create_info.pSubpasses      = &subpass;
    render_pass_create_info.dependencyCount = 1;
    render_pass_create_info.pDependencies   = &dependency;

    ASSERT(vkCreateRenderPass(vk->device, &render_pass_create_info, 0, &vk->render_pass) == VK_SUCCESS);

    VkGraphicsPipelineCreateInfo pipeline_create_info{};
    pipeline_create_info.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_create_info.pNext               = 0;
    pipeline_create_info.flags               = 0;
    pipeline_create_info.stageCount          = arraycount(shader_stages);
    pipeline_create_info.pStages             = shader_stages;
    pipeline_create_info.pVertexInputState   = &vertex_input_state;
    pipeline_create_info.pInputAssemblyState = &input_assembly_state;
    pipeline_create_info.pTessellationState  = 0;
    pipeline_create_info.pViewportState      = &viewport_state;
    pipeline_create_info.pRasterizationState = &rasterization_state;
    pipeline_create_info.pMultisampleState   = &ms_state;
    pipeline_create_info.pDepthStencilState  = &depth_stencil_state;
    pipeline_create_info.pColorBlendState    = &colorblend_state;
    pipeline_create_info.pDynamicState       = &dynamic_state;
    pipeline_create_info.layout              = vk->pipeline_layout;
    pipeline_create_info.renderPass          = vk->render_pass;
    pipeline_create_info.subpass             = 0;
    pipeline_create_info.basePipelineHandle  = VK_NULL_HANDLE;
    pipeline_create_info.basePipelineIndex   = -1;

    VkPipelineCache cache = VK_NULL_HANDLE;
    ASSERT(vkCreateGraphicsPipelines(vk->device, cache, 1, &pipeline_create_info, 0, &vk->simple_pipeline) == VK_SUCCESS);


    vk->swapchain_framebuffers = (VkFramebuffer *)os.alloc(sizeof(VkFramebuffer) * vk->swapchain_image_count);
    for (u32 i = 0; i < vk->swapchain_image_count; i++) {
        VkImageView color_depth_stencil_attachments[] = {vk->swapchain_image_views[i], vk->depth_image_view};
        VkFramebufferCreateInfo framebuffer_create_info{};
        framebuffer_create_info.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_create_info.flags           = 0;
        framebuffer_create_info.renderPass      = vk->render_pass;
        framebuffer_create_info.attachmentCount = arraycount(color_depth_stencil_attachments);
        framebuffer_create_info.pAttachments    = color_depth_stencil_attachments;
        framebuffer_create_info.width           = vk->swapchain_image_extent.width;
        framebuffer_create_info.height          = vk->swapchain_image_extent.height;
        framebuffer_create_info.layers          = 1;
        ASSERT(vkCreateFramebuffer(vk->device, &framebuffer_create_info, 0, vk->swapchain_framebuffers + i) == VK_SUCCESS);
    }





    //
    // Command Pool/Buffer
    //
    vk->command_pool = vk_create_command_pool(vk->device, vk->graphics_queue_family.index, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType               = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool         = vk->command_pool;
    alloc_info.level               = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount  = 1;
    ASSERT(vkAllocateCommandBuffers(vk->device, &alloc_info, &vk->command_buffer) == VK_SUCCESS);


    //
    // Sync.
    //
    vk->image_available_semaphore = vk_create_semaphore(vk->device);
    vk->render_finished_semaphore = vk_create_semaphore(vk->device);
    vk->in_flight_fence           = vk_create_fence(vk->device, true);


    //
    // DEBUG Image
    //
    int width, height, channels;
    u8* texels = stbi_load("../data/texture.jpg", &width, &height, &channels, 4);
    ASSERT(texels);
    VkDeviceSize image_size = width * height * 4;

    VkBuffer staging_buffer{};
    VkDeviceMemory staging_buffer_memory{};
    vk_alloc_buffer(vk, &staging_buffer, &staging_buffer_memory, image_size,
                     VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_SHARING_MODE_EXCLUSIVE,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    SCOPE_EXIT(vkDestroyBuffer(vk->device, staging_buffer, 0));
    SCOPE_EXIT(vkFreeMemory(vk->device, staging_buffer_memory, 0));

    vk_copy_to_buffer(vk, texels, 0, image_size, staging_buffer_memory);
    stbi_image_free(texels);

    vk_alloc_image(vk, &vk->DEBUG_image, &vk->DEBUG_image_memory,
                   width, height, VK_FORMAT_R8G8B8A8_SRGB,
                   VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

    vk_image_transition(vk, vk->DEBUG_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    vk_copy_buffer_to_image(vk, staging_buffer, vk->DEBUG_image, width, height);
    vk_image_transition(vk, vk->DEBUG_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vk->DEBUG_texture_image_view = vk_create_image_view(vk, vk->DEBUG_image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);

    //
    // DEBUG sampler
    //
    VkSamplerCreateInfo sampler_create_info{};
    sampler_create_info.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_create_info.magFilter               = VK_FILTER_LINEAR;
    sampler_create_info.minFilter               = VK_FILTER_LINEAR;
    sampler_create_info.addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_create_info.addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_create_info.addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_create_info.anisotropyEnable        = vk->physical_device_features.features.samplerAnisotropy ? VK_TRUE : VK_FALSE;
    sampler_create_info.maxAnisotropy           = vk->physical_device_properties.limits.maxSamplerAnisotropy;
    sampler_create_info.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_create_info.unnormalizedCoordinates = VK_FALSE;
    sampler_create_info.compareEnable           = VK_FALSE;
    sampler_create_info.compareOp               = VK_COMPARE_OP_ALWAYS;
    sampler_create_info.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_create_info.mipLodBias              = 0.0f;
    sampler_create_info.minLod                  = 0.0f;
    sampler_create_info.maxLod                  = 0.0f;
    ASSERT(vkCreateSampler(vk->device, &sampler_create_info, 0, &vk->DEBUG_texture_sampler) == VK_SUCCESS);


    //
    // Vertex Buffer
    //
    {
        VkDeviceSize temporary_size = sizeof(Vertex) * 4096;
        vk_alloc_buffer(vk, &vk->vertex_buffer, &vk->vertex_buffer_memory, temporary_size,
                         VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                         VK_SHARING_MODE_EXCLUSIVE,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    }

    //
    // Index Buffer
    //
    {
        VkDeviceSize temporary_size = sizeof(u32) * 500;
        vk_alloc_buffer(vk, &vk->index_buffer, &vk->index_buffer_memory, temporary_size,
                         VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                         VK_SHARING_MODE_EXCLUSIVE,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    }

    //
    // Uniform Buffers
    //
    VkDeviceSize buffer_size = sizeof(Uniform_Buffer_Object);
    vk_alloc_buffer(vk, &vk->uniform_buffer, &vk->uniform_buffer_memory, buffer_size,
                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     VK_SHARING_MODE_EXCLUSIVE,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vkMapMemory(vk->device, vk->uniform_buffer_memory, 0, buffer_size, 0, &vk->uniform_buffer_mapped);

    //
    // Descriptor Pool
    //
    VkDescriptorPoolSize pool_sizes[2]{};
    pool_sizes[0].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    pool_sizes[0].descriptorCount = 1; // @TODO: MAX_FRAME_IN_FLIGHT
    pool_sizes[1].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_sizes[1].descriptorCount = 1; // @TODO: MAX_FRAME_IN_FLIGHT

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags         = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT;
    pool_info.poolSizeCount = arraycount(pool_sizes);
    pool_info.pPoolSizes    = pool_sizes;
    pool_info.maxSets       = 1; // @TODO: MAX_FRAME_IN_FLIGHT

    VkDescriptorPool descriptor_pool{};
    ASSERT(vkCreateDescriptorPool(vk->device, &pool_info, 0, &descriptor_pool) == VK_SUCCESS);

    //
    // Descriptor Set
    //
    VkDescriptorSetAllocateInfo descriptor_set_alloc_info{};
    descriptor_set_alloc_info.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptor_set_alloc_info.descriptorPool     = descriptor_pool;
    descriptor_set_alloc_info.descriptorSetCount = 1; // @TODO: MAX_FRAME_IN_FLIGHT
    descriptor_set_alloc_info.pSetLayouts        = &descriptor_set_layout;

    ASSERT(vkAllocateDescriptorSets(vk->device, &descriptor_set_alloc_info, &vk->descriptor_set) == VK_SUCCESS);

    VkDescriptorBufferInfo buffer_info{};
    buffer_info.buffer = vk->uniform_buffer;
    buffer_info.offset = 0;
    buffer_info.range  = VK_WHOLE_SIZE; // @TODO: vs sizeof(Uniform_Buffer_Object)

    VkWriteDescriptorSet descriptor_writes[1]{};
    descriptor_writes[0].sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptor_writes[0].dstSet           = vk->descriptor_set;
    descriptor_writes[0].dstBinding       = 0;
    descriptor_writes[0].dstArrayElement  = 0;
    descriptor_writes[0].descriptorCount  = 1;
    descriptor_writes[0].descriptorType   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptor_writes[0].pBufferInfo      = &buffer_info;
    descriptor_writes[0].pImageInfo       = 0;
    descriptor_writes[0].pTexelBufferView = 0;
    vkUpdateDescriptorSets(vk->device, arraycount(descriptor_writes), descriptor_writes, 0, 0);
}
