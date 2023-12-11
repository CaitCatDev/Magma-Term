#include "freetype/freetype.h"
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include <magma/renderer/vk.h>
#include <magma/backend/backend.h>
#include <magma/logger/log.h>
#include <magma/private/renderer/vk.h>

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

#include <drm_fourcc.h>

#ifdef MAGMA_VK_DEBUG
VkAllocationCallbacks *magma_vk_allocator(void);
void magma_vk_allocator_print_totals(void);
#endif

VkResult magmaVkCreateRenderPass(magma_vk_renderer_t *vk);
VkResult magmaVkCreatePipeline(magma_vk_renderer_t *vk);

void insertImageMemoryBarrier(
	VkCommandBuffer cmdbuffer,
	VkImage image,
	VkAccessFlags srcAccessMask,
	VkAccessFlags dstAccessMask,
	VkImageLayout oldImageLayout,
	VkImageLayout newImageLayout,
	VkPipelineStageFlags srcStageMask,
	VkPipelineStageFlags dstStageMask,
	VkImageSubresourceRange subresourceRange)
{
	VkImageMemoryBarrier imageMemoryBarrier = {0};
	imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imageMemoryBarrier.srcAccessMask = srcAccessMask;
	imageMemoryBarrier.dstAccessMask = dstAccessMask;
	imageMemoryBarrier.oldLayout = oldImageLayout;
	imageMemoryBarrier.newLayout = newImageLayout;
	imageMemoryBarrier.image = image;
	imageMemoryBarrier.subresourceRange = subresourceRange;

	vkCmdPipelineBarrier(
		cmdbuffer,
		srcStageMask,
		dstStageMask,
		0,
		0, NULL,
		0, NULL,
		1, &imageMemoryBarrier);
}

void magma_vk_render_char(magma_vk_renderer_t *vk, int ch, float x,
		float y, float *vertex) {

	float *vert = vertex;

	
	float chw = 1.5f * 1.00f / (float)vk->width;
	float chh = 1.5f * 1.00f / (float)vk->height;


	float xpos = (x / (float)vk->width * 2.0f) - 1.0f;
	float ypos = (y / (float)vk->height * 2.0f) - 1.0f;
	
	magma_log_warn("X&Y: %f %f\nXPOS&YPOS: %f %f\n",
			x, y, xpos, ypos);

	vert[0] = xpos + vk->chardata[ch].xpos * chw;
	vert[1] = ypos + vk->chardata[ch].ypos * chh;
	vert[2] = vk->chardata[ch].xoff;
	vert[3] = vk->chardata[ch].yoff;

	vert[4] = xpos + vk->chardata[ch].xposmax * chw;
	vert[5] = ypos + vk->chardata[ch].ypos * chh;
	vert[6] = vk->chardata[ch].xoffmax;
	vert[7] = vk->chardata[ch].yoff;
	
	vert[8] = xpos + vk->chardata[ch].xpos * chw;
	vert[9] = ypos + vk->chardata[ch].yposmax * chh;
	vert[10] = vk->chardata[ch].xoff;
	vert[11] = vk->chardata[ch].yoffmax;

	vert[12] = xpos + vk->chardata[ch].xposmax * chw;
	vert[13] = ypos + vk->chardata[ch].yposmax * chh;	
	vert[14] = vk->chardata[ch].xoffmax;
	vert[15] = vk->chardata[ch].yoffmax;

}

void magma_vk_render_char_ext(magma_vk_renderer_t *vk, int ch, float x, float y) {

	float *vert = &vk->vertex[vk->vcount * 16];
	
	x *= 29;
	y *= 48;
	
	float chw = 1.5f * 1.00f / (float)vk->width;
	float chh = 1.5f * 1.00f / (float)vk->height;


	float xpos = (x / (float)vk->width * 2.0f) - 1.0f;
	float ypos = (y / (float)vk->height * 2.0f) - 1.0f;
	
	vert[0] = xpos + vk->chardata[ch].xpos * chw;
	vert[1] = ypos + vk->chardata[ch].ypos * chh;
	vert[2] = vk->chardata[ch].xoff;
	vert[3] = vk->chardata[ch].yoff;

	vert[4] = xpos + vk->chardata[ch].xposmax * chw;
	vert[5] = ypos + vk->chardata[ch].ypos * chh;
	vert[6] = vk->chardata[ch].xoffmax;
	vert[7] = vk->chardata[ch].yoff;
	
	vert[8] = xpos + vk->chardata[ch].xpos * chw;
	vert[9] = ypos + vk->chardata[ch].yposmax * chh;
	vert[10] = vk->chardata[ch].xoff;
	vert[11] = vk->chardata[ch].yoffmax;

	vert[12] = xpos + vk->chardata[ch].xposmax * chw;
	vert[13] = ypos + vk->chardata[ch].yposmax * chh;	
	vert[14] = vk->chardata[ch].xoffmax;
	vert[15] = vk->chardata[ch].yoffmax;
	
	vk->vcount++;
}


magma_buf_t *magma_vk_draw(magma_vk_renderer_t *vk) {
	static magma_buf_t buf;


	VkCommandBufferBeginInfo beginInfo = { 0 };
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	vkBeginCommandBuffer(vk->draw_buffer, &beginInfo);

	VkRenderPassBeginInfo renderPassInfo = {0};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = vk->render_pass;
	renderPassInfo.framebuffer = vk->vkfb;
	renderPassInfo.renderArea.offset.x = 0;
	renderPassInfo.renderArea.offset.y = 0;
	renderPassInfo.renderArea.extent.height = vk->height;
	renderPassInfo.renderArea.extent.width = vk->width;
	
	VkClearValue clearColor = {{{0.3f, 0.5f, 0.2f, .80f}}};
	renderPassInfo.clearValueCount = 1;
	renderPassInfo.pClearValues = &clearColor;

	vkCmdBeginRenderPass(vk->draw_buffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(vk->draw_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk->graphics_pipeline);
		vkCmdBindDescriptorSets(vk->draw_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk->pipeline_layout, 0, 1, &vk->desc_set, 0, NULL);


		VkViewport viewport = {0};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float) vk->width;
		viewport.height = (float) vk->height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(vk->draw_buffer, 0, 1, &viewport);

		VkRect2D scissor = {0};
		scissor.offset.y = 0;
		scissor.offset.x = 0;
		scissor.extent.height = vk->height;
		scissor.extent.width = vk->width;
		vkCmdSetScissor(vk->draw_buffer, 0, 1, &scissor);
		

		VkDeviceSize offsets = 0;
		vkCmdBindVertexBuffers(vk->draw_buffer, 0, 1, &vk->vertex_buffer, &offsets);
		vkCmdBindVertexBuffers(vk->draw_buffer, 1, 1, &vk->vertex_buffer, &offsets);	
		for(uint32_t i = 0; i < vk->vcount; i++) {
		vkCmdDraw(vk->draw_buffer, 4, 1, 4 * i, 0);
		}

	vkCmdEndRenderPass(vk->draw_buffer);

	vkEndCommandBuffer(vk->draw_buffer);

	VkSubmitInfo submitInfo = {0};
	VkFenceCreateInfo fenceInfo = {0};
	VkFence fence;

	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &vk->draw_buffer;
	vkCreateFence(vk->device, &fenceInfo, vk->alloc, &fence);
	vkQueueSubmit(vk->queue, 1, &submitInfo, fence);
	vkWaitForFences(vk->device, 1, &fence, VK_TRUE, UINT64_MAX);
	vkDestroyFence(vk->device, fence, vk->alloc);

	vkQueueWaitIdle(vk->queue);
	vkDeviceWaitIdle(vk->device);

	VkMemoryGetFdInfoKHR get_fd = { 0 };
	get_fd.memory = vk->src_mem;
	get_fd.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
	get_fd.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR;


	VkImageSubresource sub_resource = {0};

	sub_resource.aspectMask = VK_IMAGE_ASPECT_MEMORY_PLANE_0_BIT_EXT;
	VkSubresourceLayout sub_resource_layout;

	
	vkGetImageSubresourceLayout(vk->device, vk->vk_image, &sub_resource, &sub_resource_layout);
	

	buf.height = vk->height;
	buf.width = vk->width;
	buf.pitch = sub_resource_layout.rowPitch;
	buf.bpp = 32;
	buf.size = sub_resource_layout.size;

	VkMemoryGetFdInfoKHR info = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR,
		.memory = vk->src_mem,
		.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
		.pNext = NULL,
	};

	PFN_vkGetMemoryFdKHR function = (PFN_vkGetMemoryFdKHR)vkGetDeviceProcAddr(vk->device, "vkGetMemoryFdKHR");
	function(vk->device, &info, &buf.fd);

	magma_log_debug("FD: %d\n", buf.fd);	
	vk->vcount = 0;
	return &buf;
}

void magma_vk_create_framebuffer(magma_vk_renderer_t *vk) {

	VkFramebufferCreateInfo framebufferInfo = {0};
	framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferInfo.renderPass = vk->render_pass;
	framebufferInfo.attachmentCount = 1;
	framebufferInfo.pAttachments = &vk->vk_image_view;
	framebufferInfo.width = vk->width;
	framebufferInfo.height = vk->height;
	framebufferInfo.layers = 1;
	

	vkCreateFramebuffer(vk->device, &framebufferInfo, vk->alloc, &vk->vkfb);
}

void magma_vk_handle_resize(magma_vk_renderer_t *vk, uint32_t width, uint32_t height) {
	VkResult res;
	vk->height = height;
	vk->width = width;



	vkDestroyFramebuffer(vk->device, vk->vkfb, vk->alloc);

	vkFreeMemory(vk->device, vk->dst_mem, vk->alloc);

	vkFreeMemory(vk->device, vk->src_mem, vk->alloc);

	vkDestroyImage(vk->device, vk->dst_image, vk->alloc);

	vkDestroyImageView(vk->device, vk->vk_image_view, vk->alloc);

	vkDestroyImage(vk->device, vk->vk_image, vk->alloc);


	VkExternalMemoryImageCreateInfo extMemoryImage = {0};
	VkImageDrmFormatModifierListCreateInfoEXT drm = {0};
	uint64_t mod = DRM_FORMAT_MOD_LINEAR;
	VkExportMemoryAllocateInfo expAlloc = { 0 };
	

	drm.sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_LIST_CREATE_INFO_EXT;
	drm.drmFormatModifierCount = 1;
	drm.pDrmFormatModifiers = &mod;
	
	extMemoryImage.pNext = &drm;
	extMemoryImage.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
	extMemoryImage.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;

	
	expAlloc.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
	expAlloc.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;


	/*Src Memory:*/
	res = magma_vk_create_2dimage(vk->device, vk->height, vk->width, 
				VK_FORMAT_B8G8R8A8_SRGB, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
				VK_IMAGE_USAGE_TRANSFER_SRC_BIT, 0, VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT, 
				1, 1, &extMemoryImage, vk->alloc, &vk->vk_image);
	if(res) {
		
	}

	res = magma_vk_allocate_image(vk->device, vk->phy_dev, vk->vk_image, 
			0, &expAlloc, vk->alloc, &vk->src_mem);
	
	vkBindImageMemory(vk->device, vk->vk_image, vk->src_mem, 0);

	magma_vk_create_image_view(vk->device, VK_FORMAT_B8G8R8A8_SRGB, vk->vk_image,
			VK_IMAGE_VIEW_TYPE_2D, 0, 0, 0, 1, 1, VK_IMAGE_ASPECT_COLOR_BIT, 
			(VkComponentMapping){ 0 }, vk->alloc, &vk->vk_image_view);

	/*Dst Image*/
	res = magma_vk_create_2dimage(vk->device, vk->height, vk->width, 
				VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_USAGE_TRANSFER_DST_BIT,
				0, VK_IMAGE_TILING_LINEAR, 1, 1, NULL, vk->alloc, &vk->dst_image);
	if(res) {
		
	}

	res = magma_vk_allocate_image(vk->device, vk->phy_dev, vk->dst_image,
			VK_MEMORY_PROPERTY_HOST_CACHED_BIT | 
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, 
			NULL, vk->alloc, &vk->dst_mem);
	
	vkBindImageMemory(vk->device, vk->dst_image, vk->dst_mem, 0);

	magma_vk_create_framebuffer(vk);
}


VkResult magma_vk_create_sampler(VkDevice device, VkSampler *sampler);
magma_vk_renderer_t *magma_vk_renderer_init(magma_backend_t *backend) {
	VkResult res;
	magma_vk_renderer_t *vk = calloc(1, sizeof(*vk));
	if(!vk) {
		magma_log_error("calloc: Failed to allocate Vulkan renderer struct\n");
		goto error_alloc_vk_struct;
	}
	vk->width = 600;
	vk->height = 600;

#ifdef MAGMA_VK_DEBUG
	vk->alloc = magma_vk_allocator();
#else
	vk->alloc = NULL;
#endif /* ifdef MAGMA_VK_DEBUG */

	res = magma_vk_create_instance(backend, vk->alloc, &vk->instance);
	if(res) {
		magma_log_error("Failed to create vulkan interface %d\n", res);
		goto error_vk_create_instance;
	}

#ifdef MAGMA_VK_DEBUG
	res = magma_vk_create_debug_messenger(vk->instance, vk->alloc, &vk->debug_messenger);
	if(res) {
		magma_log_error("Failed to create debug messenger\n");
		goto error_vk_get_debug_msg;
	}
#endif

	res = magma_vk_get_physical_device(vk);
	if(res) {
		magma_log_error("Failed to create phsyical device\n");
		goto error_vk_get_phsyical_dev;
	}

	res = magma_vk_create_device(vk);
	if(res) {
		magma_log_error("Failed to create logical device\n");
		goto error_vk_create_device;
	}

	magma_vk_create_sampler(vk->device, &vk->sampler);


	VkExternalMemoryImageCreateInfo extMemoryImage = {0};
	VkImageDrmFormatModifierListCreateInfoEXT drm = {0};
	uint64_t mod = DRM_FORMAT_MOD_LINEAR;
	VkExportMemoryAllocateInfo expAlloc = { 0 };
	

	drm.sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_LIST_CREATE_INFO_EXT;
	drm.drmFormatModifierCount = 1;
	drm.pDrmFormatModifiers = &mod;
	
	extMemoryImage.pNext = &drm;
	extMemoryImage.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
	extMemoryImage.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;

	
	expAlloc.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
	expAlloc.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;


	/*Src Memory:*/
	res = magma_vk_create_2dimage(vk->device, vk->height, vk->width, 
				VK_FORMAT_B8G8R8A8_SRGB, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
				VK_IMAGE_USAGE_TRANSFER_SRC_BIT, 0, VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT, 
				1, 1, &extMemoryImage, vk->alloc, &vk->vk_image);
	if(res) {
		
	}

	res = magma_vk_allocate_image(vk->device, vk->phy_dev, vk->vk_image, 
			0, &expAlloc, vk->alloc, &vk->src_mem);
	
	vkBindImageMemory(vk->device, vk->vk_image, vk->src_mem, 0);

	magma_vk_create_image_view(vk->device, VK_FORMAT_B8G8R8A8_SRGB, vk->vk_image,
			VK_IMAGE_VIEW_TYPE_2D, 0, 0, 0, 1, 1, VK_IMAGE_ASPECT_COLOR_BIT, 
			(VkComponentMapping){ 0 }, vk->alloc, &vk->vk_image_view);

	/*Dst Image*/
	res = magma_vk_create_2dimage(vk->device, vk->height, vk->width, 
				VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_USAGE_TRANSFER_DST_BIT,
				0, VK_IMAGE_TILING_LINEAR, 1, 1, NULL, vk->alloc, &vk->dst_image);
	if(res) {
		
	}

	res = magma_vk_allocate_image(vk->device, vk->phy_dev, vk->vk_image, 
			VK_MEMORY_PROPERTY_HOST_CACHED_BIT | 
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, 
			NULL, vk->alloc, &vk->dst_mem);
	
	vkBindImageMemory(vk->device, vk->dst_image, vk->dst_mem, 0);


	/*Command Buffer/pool*/
	magma_vk_create_cmd_pool(vk->device, vk->indicies.graphics, 
			VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
			NULL, vk->alloc, &vk->command_pool);


	magma_vk_alloc_cmd_buffer(vk->device, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 
			vk->command_pool, NULL, 1, &vk->draw_buffer);

	magmaVkCreateRenderPass(vk);
	magmaVkCreatePipeline(vk);
	magma_vk_create_framebuffer(vk);

	return vk;
error_vk_create_device:
error_vk_get_phsyical_dev:
#ifdef MAGMA_VK_DEBUG
	vkDestroyDebugUtilsMessengerEXT(vk->instance, vk->debug_messenger, vk->alloc);
error_vk_get_debug_msg:
#endif
	vkDestroyInstance(vk->instance, vk->alloc);
error_vk_create_instance:
	free(vk);
error_alloc_vk_struct:
	return NULL;
}



void magma_vk_renderer_deinit(magma_vk_renderer_t *vk) {

	magma_log_warn("VK DEINIT\n");

	vkDestroyRenderPass(vk->device, vk->render_pass, vk->alloc);

	vkDestroyPipelineLayout(vk->device, vk->pipeline_layout, vk->alloc);

	vkDestroyPipeline(vk->device, vk->graphics_pipeline, vk->alloc);

	vkDestroyFramebuffer(vk->device, vk->vkfb, vk->alloc);

	vkFreeMemory(vk->device, vk->dst_mem, vk->alloc);

	vkFreeMemory(vk->device, vk->src_mem, vk->alloc);

	vkDestroyImage(vk->device, vk->dst_image, vk->alloc);

	vkDestroyImageView(vk->device, vk->vk_image_view, vk->alloc);

	vkDestroyImage(vk->device, vk->vk_image, vk->alloc);

	vkFreeCommandBuffers(vk->device, vk->command_pool, 1, &vk->draw_buffer);

	vkDestroyCommandPool(vk->device, vk->command_pool, vk->alloc);

	vkDestroyDevice(vk->device, vk->alloc);

#ifdef MAGMA_VK_DEBUG
	vkDestroyDebugUtilsMessengerEXT(vk->instance, vk->debug_messenger, vk->alloc);
#endif
	vkDestroyInstance(vk->instance, vk->alloc);

#ifdef MAGMA_VK_DEBUG
	magma_vk_allocator_print_totals();
#endif /* ifdef MACRO */

	free(vk);
}
