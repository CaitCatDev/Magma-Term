#include <errno.h>
#include <string.h>
#include <vulkan/vulkan.h>

#include <magma/private/renderer/vk.h>
#include <magma/renderer/vk.h>
#include <magma/logger/log.h>

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <vulkan/vulkan_core.h>


/* Expose all the features of Vk Create Image we probably won't use this function bug it's 
 * here if we need to create an image with unique/less used features
 */
VkResult __magma_vk_create_image_raw(VkDevice device, uint32_t height,
		uint32_t width, VkFormat format, VkSampleCountFlagBits samples,
		uint32_t layers, uint32_t qcount, const uint32_t *queues, 
		uint32_t depth, VkSharingMode sharing, VkImageLayout layout, 
		uint32_t miplvl, VkImageUsageFlags usage, VkImageTiling tiling,
		VkImageType type, VkImageCreateFlags flags, 
		VkAllocationCallbacks *alloc, void *pnext, VkImage *image) {
	VkImageCreateInfo info = { 0 };
	
	/*These are guranteded for images*/
	info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	
	/*Variable*/
	info.extent.width = width;
	info.extent.height = height;
	info.extent.depth = depth;
	info.imageType = type;
	info.tiling = tiling;
	info.usage = usage;
	info.mipLevels = miplvl;
	info.arrayLayers = layers;
	info.flags = flags;
	info.samples = samples;
	info.pNext = pnext;
	info.format = format;
	info.initialLayout = layout;
	info.queueFamilyIndexCount = qcount;
	info.pQueueFamilyIndices = queues;
	info.sharingMode = sharing;
	

	return vkCreateImage(device, &info, alloc, image);
}

/*Again likely this is more verbose than we will ever actually use but 
 * it exists purely so if we ever do need to swap image types, depth, etc 
 * it shouldn't be to hard to do 
 */
VkResult __magma_vk_create_2dimage_raw(VkDevice device, uint32_t height,
		uint32_t width, VkFormat format, VkSampleCountFlagBits samples,
		uint32_t layers, uint32_t qcount, const uint32_t *queues,
		VkSharingMode sharing, VkImageLayout layout, uint32_t miplvl, 
		VkImageUsageFlags usage, VkImageTiling tiling, VkImageCreateFlags flags,
		VkAllocationCallbacks *alloc, void *pnext, VkImage *image) {
	return __magma_vk_create_image_raw(device, height, width, format, 
			samples, layers, qcount, queues, 1, sharing, layout, miplvl,
			usage, tiling, VK_IMAGE_TYPE_2D, flags, alloc, pnext, image);
}

VkResult magma_vk_create_2dimage(VkDevice device, uint32_t height, 
		uint32_t width, VkFormat format, VkImageUsageFlags usage,
		VkImageCreateFlags flags, VkImageTiling tiling, 
		uint32_t miplevels, uint32_t layers, void *pnext,
		VkAllocationCallbacks *alloc, VkImage *image) {
	return __magma_vk_create_image_raw(device, height, width, 
			format, VK_SAMPLE_COUNT_1_BIT, layers, 0, NULL, 1, 
			VK_SHARING_MODE_EXCLUSIVE, VK_IMAGE_LAYOUT_UNDEFINED, 
			miplevels, usage, tiling, VK_IMAGE_TYPE_2D, flags, 
			alloc, pnext, image);
}

uint32_t magma_vk_mem_index(VkPhysicalDevice phydevice, 
		uint32_t type, VkMemoryPropertyFlags properties) {
	VkPhysicalDeviceMemoryProperties mem_props;
	vkGetPhysicalDeviceMemoryProperties(phydevice, &mem_props);
	for(uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
		if((type & 1) == 1 && (mem_props.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
		type >>= 1;
	}

	return 0;
}

VkResult magma_vk_allocate_image(VkDevice device, VkPhysicalDevice phydev,
		VkImage image, VkMemoryPropertyFlags properties, void *pnext, 
		VkAllocationCallbacks *alloc, VkDeviceMemory *memory) {
	VkMemoryRequirements mem_req = { 0 };
	VkMemoryAllocateInfo mem_alloc = { 0 };
	
	vkGetImageMemoryRequirements(device, image, &mem_req);

	mem_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	mem_alloc.allocationSize = mem_req.size;
	mem_alloc.pNext = pnext;
	mem_alloc.memoryTypeIndex = magma_vk_mem_index(phydev, mem_req.memoryTypeBits, properties);

	return vkAllocateMemory(device, &mem_alloc, alloc, memory);
}

VkResult magma_vk_create_image_view(VkDevice device, VkFormat format,
		VkImage image, VkImageViewType type, VkImageViewCreateFlags flags,
		uint32_t base_mip_level, uint32_t base_layer, uint32_t layer_count,
		uint32_t mip_level_count, VkImageAspectFlags aspect_mask, 
		VkComponentMapping components, VkAllocationCallbacks *alloc, 
		VkImageView *view) {
	VkImageViewCreateInfo info = { 0 };
	info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	info.format = format;
	info.flags = flags;
	info.image = image;
	info.viewType = type;
	info.subresourceRange.aspectMask = aspect_mask;
	info.subresourceRange.levelCount = mip_level_count;
	info.subresourceRange.baseMipLevel = base_mip_level;
	info.subresourceRange.layerCount = layer_count;
	info.subresourceRange.baseArrayLayer = base_layer;
	info.components = components;


	return vkCreateImageView(device, &info, alloc, view); 
}
