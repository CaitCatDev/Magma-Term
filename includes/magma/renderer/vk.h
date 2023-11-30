#pragma once

#include <magma/backend/backend.h>

typedef struct magma_vk_renderer magma_vk_renderer_t;



/* Expose all the features of Vk Create Image we probably won't use this function bug it's 
 * here if we need to create an image with unique/less used features
 */
VkResult __magma_vk_create_image_raw(VkDevice device, uint32_t height,
		uint32_t width, VkFormat format, VkSampleCountFlagBits samples,
		uint32_t layers, uint32_t qcount, const uint32_t *queues, 
		uint32_t depth, VkSharingMode sharing, VkImageLayout layout, 
		uint32_t miplvl, VkImageUsageFlags usage, VkImageTiling tiling,
		VkImageType type, VkImageCreateFlags flags, 
		VkAllocationCallbacks *alloc, void *pnext, VkImage *image);
VkResult __magma_vk_create_2dimage_raw(VkDevice device, uint32_t height,
		uint32_t width, VkFormat format, VkSampleCountFlagBits samples,
		uint32_t layers, uint32_t qcount, const uint32_t *queues,
		VkSharingMode sharing, VkImageLayout layout, uint32_t miplvl, 
		VkImageUsageFlags usage, VkImageTiling tiling, VkImageCreateFlags flags,
		VkAllocationCallbacks *alloc, void *pnext, VkImage *image);
VkResult magma_vk_create_2dimage(VkDevice device, uint32_t height, 
		uint32_t width, VkFormat format, VkImageUsageFlags usage,
		VkImageCreateFlags flags, VkImageTiling tiling, 
		uint32_t miplevels, uint32_t layers, void *pnext,
		VkAllocationCallbacks *alloc, VkImage *image);	
VkResult magma_vk_allocate_image(VkDevice device, VkPhysicalDevice phydev,
		VkImage image, VkMemoryPropertyFlags properties, void *pnext, 
		VkAllocationCallbacks *alloc, VkDeviceMemory *memory);
VkResult magma_vk_create_image_view(VkDevice device, VkFormat format,
		VkImage image, VkImageViewType type, VkImageViewCreateFlags flags,
		uint32_t base_mip_level, uint32_t base_layer, uint32_t layer_count,
		uint32_t mip_level_count, VkImageAspectFlags aspect_mask, 
		VkComponentMapping components, VkAllocationCallbacks *alloc, 
		VkImageView *view);

magma_buf_t *magma_vk_draw(magma_vk_renderer_t *vk);
void magma_vk_renderer_deinit(magma_vk_renderer_t *renderer);
magma_vk_renderer_t *magma_vk_renderer_init(magma_backend_t *backend);
