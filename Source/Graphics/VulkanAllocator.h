#pragma once

struct Image {
	VkImage handle;
	VkImageView view;
	VkDeviceMemory memory;
};

struct MappedBuffer {
	VkBuffer handle;
	void *data;
	VkDeviceMemory memory;
};

namespace VulkanAllocator {
Image CreateImage2D(VkDevice device, VkPhysicalDeviceMemoryProperties memory_properties,
					uint32_t width, uint32_t height, VkFormat format);
MappedBuffer CreateMappedBuffer(VkDevice device, VkPhysicalDeviceMemoryProperties memory_properties,
								uint64_t size);
}
