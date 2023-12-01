#pragma once

#include <magma/renderer/vk.h>
#include <stdint.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

struct queue_indicies {
	uint32_t compute, graphics, transfer;
};


struct magma_vk_renderer {
	VkInstance instance;

	#ifdef MAGMA_VK_DEBUG
	VkDebugUtilsMessengerEXT debug_messenger;
	#endif /* ifdef MAGMA_VK_DEBUG */

	VkAllocationCallbacks *alloc;

	VkSurfaceKHR surface;

	VkPhysicalDevice phy_dev;
	VkDevice device;

	VkCommandPool command_pool;
	VkCommandBuffer draw_buffer;
	VkCommandBuffer transfer;

	VkPipelineLayout pipeline_layout;
	VkPipeline graphics_pipeline;

	VkRenderPass render_pass;

	VkImage vk_image;
	VkImage dst_image;
	VkImageView vk_image_view;
	VkFramebuffer vkfb;

	VkDeviceMemory src_mem;
	VkDeviceMemory dst_mem;

	uint32_t height,width;

	struct queue_indicies indicies;
	VkQueue queue; /*TODO: is it guranteeded we can relie on all queue 
					*features being supported on one queue?
					*/

	VkImage font_text;
	VkImageView font_text_view;
	VkDeviceMemory font_mem;

	VkDescriptorSet desc_set;
	VkDescriptorPool pool;
	VkDescriptorSetLayout desc_layout;
	VkSampler sampler;

	float *vertex;
	VkBuffer vertex_buffer;
	VkDeviceMemory vertbuf_mem;
};

VkResult magma_vk_create_instance(magma_backend_t *backend, VkAllocationCallbacks *callbacks, VkInstance *instance);
VkResult magma_vk_create_debug_messenger(VkInstance instance, VkAllocationCallbacks *callbacks, VkDebugUtilsMessengerEXT *messenger);
VkResult magma_vk_get_physical_device(magma_vk_renderer_t *renderer);
VkResult magma_vk_create_device(magma_vk_renderer_t *vk);



VkResult magma_vk_create_cmd_pool(VkDevice device, uint32_t queue_index,
		VkCommandPoolCreateFlags flags, void *pnext, 
		VkAllocationCallbacks *alloc, VkCommandPool *pool);
VkResult magma_vk_alloc_cmd_buffer(VkDevice device, VkCommandBufferLevel level,
		VkCommandPool pool, void *pnext, uint32_t buffer_count,
		VkCommandBuffer *buffers);


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


