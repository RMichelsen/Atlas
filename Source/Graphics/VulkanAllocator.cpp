#include "PCH.h"
#include "VulkanAllocator.h"

// Function from Vulkan spec 1.0.183
static inline uint32_t FindMemoryTypeIndex(VkPhysicalDeviceMemoryProperties memory_properties, 
							 uint32_t memory_type_bits_requirements, 
							 VkMemoryPropertyFlags required_properties) {
	uint32_t memory_count = memory_properties.memoryTypeCount;

	for(uint32_t memory_index = 0; memory_index < memory_count; ++memory_index) {
		VkMemoryPropertyFlags properties = memory_properties.memoryTypes[memory_index].propertyFlags;
		bool is_required_memory_type = memory_type_bits_requirements & (1 << memory_index);
		bool has_required_properties = (properties & required_properties) == required_properties;

		if(is_required_memory_type && has_required_properties) {
			return (uint32_t)memory_index;
		}
	}

	// Failed to find memory type
	assert(false);
	return UINT32_MAX;
}

static inline VkDeviceMemory AllocateMemory(VkDevice device, VkPhysicalDeviceMemoryProperties memory_properties,
							  VkDeviceSize size, uint32_t memory_type_bits_requirements,
							  VkMemoryPropertyFlags memory_property_flags) {
	VkMemoryAllocateInfo memory_allocate_info = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = size,
		.memoryTypeIndex = FindMemoryTypeIndex(memory_properties, memory_type_bits_requirements,
											   memory_property_flags)
	};

	VkDeviceMemory memory = VK_NULL_HANDLE;
	VK_CHECK(vkAllocateMemory(device, &memory_allocate_info, nullptr, &memory));
	return memory;
}

static inline uint64_t AlignUp(uint64_t value, uint64_t alignment) {
	return ((value + alignment - 1) / alignment) * alignment;
}

namespace VulkanAllocator {
Image CreateImage2D(VkDevice device, VkPhysicalDeviceMemoryProperties memory_properties,
						   uint32_t width, uint32_t height, VkFormat format) {
	VkImageCreateInfo image_info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = format,
		.extent = VkExtent3D {
			.width = width,
			.height = height,
			.depth = 1
		},
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
	};
	VkImage image;
	VK_CHECK(vkCreateImage(device, &image_info, nullptr, &image));

	VkMemoryRequirements memory_requirements;
	vkGetImageMemoryRequirements(device, image, &memory_requirements);

	uint64_t aligned_size = AlignUp(memory_requirements.size, memory_requirements.alignment);
	VkDeviceMemory memory = AllocateMemory(device, memory_properties, aligned_size, 
										   memory_requirements.memoryTypeBits, 
										   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK(vkBindImageMemory(device, image, memory, 0));

	VkImageViewCreateInfo image_view_info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = image_info.format,
		.components = {
			.r = VK_COMPONENT_SWIZZLE_R,
			.g = VK_COMPONENT_SWIZZLE_G,
			.b = VK_COMPONENT_SWIZZLE_B,
			.a = VK_COMPONENT_SWIZZLE_A
		},
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, 
			.levelCount = 1,
			.layerCount = 1
		}
	};
	VkImageView image_view;
	VK_CHECK(vkCreateImageView(device, &image_view_info, NULL, &image_view));

	return Image {
		.handle = image,
		.view = image_view,
		.memory = memory
	};
}

MappedBuffer CreateMappedBuffer(VkDevice device, VkPhysicalDeviceMemoryProperties memory_properties, 
								VkBufferUsageFlags usage, uint64_t size) {
	VkBufferCreateInfo buffer_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = size,
		.usage = usage,
	};
	VkBuffer buffer;
	VK_CHECK(vkCreateBuffer(device, &buffer_info, nullptr, &buffer));

	VkMemoryRequirements memory_requirements;
	vkGetBufferMemoryRequirements(device, buffer, &memory_requirements);

	uint64_t aligned_size = AlignUp(memory_requirements.size, memory_requirements.alignment);
	VkDeviceMemory memory = AllocateMemory(device, memory_properties, aligned_size, 
										   memory_requirements.memoryTypeBits, 
										   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
										   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	VK_CHECK(vkBindBufferMemory(device, buffer, memory, 0));

	void *mapped_data = nullptr;
	VK_CHECK(vkMapMemory(device, memory, 0, aligned_size, 0, &mapped_data));

	return MappedBuffer {
		.handle = buffer,
		.data = mapped_data,
		.memory = memory
	};
}
}
