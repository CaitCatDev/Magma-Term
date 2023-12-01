#include <stdint.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>


#include <magma/private/renderer/vk.h>
#include <magma/logger/log.h>

VkResult magma_vk_create_cmd_pool(VkDevice device, uint32_t queue_index,
		VkCommandPoolCreateFlags flags, void *pnext, 
		VkAllocationCallbacks *alloc, VkCommandPool *pool) {
	VkCommandPoolCreateInfo info = { 0 };

	info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	info.queueFamilyIndex = queue_index;
	info.flags = flags;
	info.pNext = pnext;

	return vkCreateCommandPool(device, &info, alloc, pool);
}

VkResult magma_vk_alloc_cmd_buffer(VkDevice device, VkCommandBufferLevel level,
		VkCommandPool pool, void *pnext, uint32_t buffer_count,
		VkCommandBuffer *buffers) {
	VkCommandBufferAllocateInfo info = { 0 };

	info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	info.commandBufferCount = buffer_count;
	info.commandPool = pool;
	info.pNext = pnext;
	info.level = level;

	return vkAllocateCommandBuffers(device, &info, buffers);
}
