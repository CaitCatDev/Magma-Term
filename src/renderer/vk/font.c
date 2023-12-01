#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <magma/logger/log.h>
#include <magma/private/renderer/vk.h>
#include <magma/renderer/vk.h>

int vkglyph_check_bit(const FT_GlyphSlot glyph, const int x, const int y)
{
    int pitch = glyph->bitmap.pitch;
    unsigned char *row = &glyph->bitmap.buffer[pitch * y];
    char cValue = row[x >> 3];

    return (cValue & (128 >> (x & 7))) != 0;
}

VkResult magma_vk_create_sampler(VkDevice device, VkSampler *sampler) {

	VkSamplerCreateInfo samplerInfo = {0};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.compareOp = VK_COMPARE_OP_NEVER;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 1.0f;
	samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

	return vkCreateSampler(device, &samplerInfo, NULL, sampler);
}

VkResult magma_vk_create_desc_pool(VkDevice device, VkDescriptorPool *pool) {
	VkDescriptorPoolSize poolsize = { 0 };
	VkDescriptorPoolCreateInfo info = { 0 };

	poolsize.descriptorCount = 1;
	poolsize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

	info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	info.poolSizeCount = 1;
	info.maxSets = 1;
	info.pPoolSizes = &poolsize;

	return vkCreateDescriptorPool(device, &info, NULL, pool);
}

VkResult magma_vk_allocate_desc_set(magma_vk_renderer_t *vk) {
	VkDescriptorSetAllocateInfo alloc = {0};
	VkDescriptorImageInfo info = {0};

	alloc.pSetLayouts = &vk->desc_layout;
	alloc.descriptorSetCount = 1;
	alloc.descriptorPool = vk->pool;
	alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;

	vkAllocateDescriptorSets(vk->device, &alloc, &vk->desc_set);

	info.sampler = vk->sampler;
	info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	info.imageView = vk->font_text_view;

	VkWriteDescriptorSet writeset = {0};

	writeset.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeset.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writeset.pImageInfo = &info;
	writeset.descriptorCount = 1;
	writeset.dstSet = vk->desc_set;
	vkUpdateDescriptorSets(vk->device, 1, &writeset, 0, NULL);
	return VK_SUCCESS;
}

void insertImageMemoryBarrier(
	VkCommandBuffer cmdbuffer,
	VkImage image,
	VkAccessFlags srcAccessMask,
	VkAccessFlags dstAccessMask,
	VkImageLayout oldImageLayout,
	VkImageLayout newImageLayout,
	VkPipelineStageFlags srcStageMask,
	VkPipelineStageFlags dstStageMask,
	VkImageSubresourceRange subresourceRange);

uint32_t magma_vk_mem_index(VkPhysicalDevice phydevice, 
		uint32_t type, VkMemoryPropertyFlags properties);

VkResult magma_vk_make_vertex(magma_vk_renderer_t *vk) {
	VkBufferCreateInfo buffer = { 0 };


	buffer.size = (sizeof(float) * 4) * 500;
	buffer.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

	vkCreateBuffer(vk->device, &buffer, NULL, &vk->vertex_buffer);

	VkMemoryRequirements memReqs;
	VkMemoryAllocateInfo allocInfo = {0};

	vkGetBufferMemoryRequirements(vk->device, vk->vertex_buffer, &memReqs);
	allocInfo.allocationSize = memReqs.size;
	allocInfo.memoryTypeIndex = magma_vk_mem_index(vk->phy_dev, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;

	vkAllocateMemory(vk->device, &allocInfo, NULL, &vk->vertbuf_mem);
	return vkBindBufferMemory(vk->device, vk->vertex_buffer, vk->vertbuf_mem, 0);
}

int magma_vk_font_bitmap_init(magma_vk_renderer_t *vk, FT_Face face) {
	VkImageCreateInfo info = { 0 };
	uint32_t *data;
	VkImage stage;
	VkDeviceMemory stagemem;

	int tex_width = 1;
	int max_dim = (1 + (face->size->metrics.height >> 6)) * ceilf(sqrtf(UINT16_MAX));
	while(tex_width < max_dim) tex_width <<= 1;
	int tex_height = tex_width;

	VkImageSubresource sub_resource = {0};

	sub_resource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	VkSubresourceLayout sub_resource_layout;

	magma_vk_make_vertex(vk);
	vkMapMemory(vk->device, vk->vertbuf_mem, 0, VK_WHOLE_SIZE, 0, (void **)&vk->vertex);

	/*Dst Image*/
	magma_vk_create_2dimage(vk->device, tex_height, tex_width, 
				VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
				0, VK_IMAGE_TILING_LINEAR, 1, 1, NULL, vk->alloc, &stage);

	magma_vk_allocate_image(vk->device, vk->phy_dev, stage, 
			VK_MEMORY_PROPERTY_HOST_CACHED_BIT | 
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, 
			NULL, vk->alloc, &stagemem);
	
	vkBindImageMemory(vk->device, stage, stagemem, 0);

	float *vert =vk->vertex;



	vkGetImageSubresourceLayout(vk->device, stage, &sub_resource, &sub_resource_layout);

	magma_log_fatal("PITCH: %d\n", sub_resource_layout.rowPitch);
	vkMapMemory(vk->device, stagemem, 0, sub_resource_layout.size, 0, (void *)&data);
	int pen_x, pen_y = 0;

	memset(data, 0, sub_resource_layout.size);

	for(int i = 65; i < 66; ++i){
		uint32_t gi = FT_Get_Char_Index(face, i);
		FT_Load_Glyph(face, gi, FT_LOAD_DEFAULT);
		FT_Render_Glyph(face->glyph, FT_RENDER_MODE_MONO);;
		FT_Bitmap* bmp = &face->glyph->bitmap;

		if(pen_x + bmp->width >= (unsigned int)tex_width){
			pen_x = 0;
			pen_y += ((face->size->metrics.height >> 6) + 1);
		}

		for(uint32_t row = 0; row < bmp->rows; ++row){
			for(uint32_t col = 0; col < bmp->width; ++col){
				int x = pen_x + col;
				int y = pen_y + row + (face->size->face->height >> 6);
				if(vkglyph_check_bit(face->glyph, col, row)) {
				data[y * (sub_resource_layout.rowPitch / 4) + x] = 0xfff8f8f8;
				vert[0] = -1.0 ;
				vert[1] = -1.0 ;
				vert[2] = ((float)pen_x / tex_width);
				vert[3] = ((float)pen_y / tex_height);

				vert[4] = -.7;
				vert[5] = -1.0f ;
				vert[6] = ((float)x / tex_width);
				vert[7] = ((float)pen_y + (face->size->metrics.height >> 6)) / tex_height;
				
				vert[8] = -.90;
				vert[9] = -.0;
				vert[10] = ((float)pen_x * (face->size->metrics.max_advance >> 6)) / tex_width;
				vert[11] = ((float)pen_y * (face->size->metrics.height >> 6)) / tex_height;

				vert[12] = -.71;
				vert[13] = -0.90f;
				vert[14] = ((float)x + (face->size->metrics.max_advance >> 6)) / tex_width;
				vert[15] = ((float)y) / tex_height;
				for(int i = 0; i < 16; i++) {
					printf("Vertex (%d): %f\n", i, vert[i]);
				}
				}
			}
		}

		pen_x += bmp->width + 4;
	}


	vkUnmapMemory(vk->device, stagemem);
	info.extent.height = tex_height;
	info.extent.width = tex_width;
	info.extent.depth = 1;
	info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	info.format = VK_FORMAT_B8G8R8A8_UNORM;
	info.samples = VK_SAMPLE_COUNT_1_BIT;
	info.tiling = VK_IMAGE_TILING_OPTIMAL;
	info.imageType = VK_IMAGE_TYPE_2D;
	info.arrayLayers = 1;
	info.mipLevels = 1;
	info.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	(void) info;

	/*Src Memory:*/
	magma_vk_create_2dimage(vk->device, tex_height, tex_width, 
				VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT |
				VK_IMAGE_USAGE_TRANSFER_DST_BIT,
				0, VK_IMAGE_TILING_OPTIMAL, 1, 1, NULL, vk->alloc, &vk->font_text);
	magma_vk_allocate_image(vk->device, vk->phy_dev, vk->font_text, 
			0, NULL, vk->alloc, &vk->font_mem);

	vkBindImageMemory(vk->device, vk->font_text, vk->font_mem, 0);

	magma_vk_create_image_view(vk->device, VK_FORMAT_B8G8R8A8_UNORM, vk->font_text, 
			VK_IMAGE_VIEW_TYPE_2D, 0, 0, 0, 1, 1, VK_IMAGE_ASPECT_COLOR_BIT, (VkComponentMapping){0},
			vk->alloc, &vk->font_text_view);


	VkCommandBufferBeginInfo cmd = { 0 };
	cmd.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	vkBeginCommandBuffer(vk->draw_buffer, &cmd);

	VkImageSubresourceRange ResRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

	insertImageMemoryBarrier(
		vk->draw_buffer,
		vk->font_text,
		0,
		VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		ResRange
		);

	insertImageMemoryBarrier(
		vk->draw_buffer,
		stage,
		0,
		VK_ACCESS_TRANSFER_READ_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		ResRange
		);


	VkImageCopy imageCopyRegion = {0};
	imageCopyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageCopyRegion.srcSubresource.layerCount = 1;
	imageCopyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageCopyRegion.dstSubresource.layerCount = 1;
	imageCopyRegion.extent.width = tex_width;
	imageCopyRegion.extent.height = tex_height;
	imageCopyRegion.extent.depth = 1;

	vkCmdCopyImage(vk->draw_buffer, stage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 
			vk->font_text, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageCopyRegion);

	insertImageMemoryBarrier(
		vk->draw_buffer,
		vk->font_text,
		0,
		VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		ResRange
		);



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

	vkResetCommandBuffer(vk->draw_buffer, 0);



	magma_vk_create_desc_pool(vk->device, &vk->pool);
	magma_vk_allocate_desc_set(vk);
	




	return 0;
}
