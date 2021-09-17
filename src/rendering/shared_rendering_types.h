#pragma once
#include <core/common_types.h>
#include <vulkan/vulkan.h>

#define VK_CHECK(x) if((x) != VK_SUCCESS) { 			\
	assert(FALSE); 										\
	printf("Vulkan error: %s:%i", __FILE__, __LINE__); 	\
}

#define MAX_FRAMES_IN_FLIGHT 3

typedef struct PhysicalDevice {
	VkPhysicalDevice handle;
	VkPhysicalDeviceProperties2 properties;
	VkPhysicalDeviceMemoryProperties memory_properties;
	VkSurfaceCapabilitiesKHR surface_capabilities;
	uint32_t graphics_family_idx;
	uint32_t compute_family_idx;
} PhysicalDevice;

typedef struct LogicalDevice {
	VkDevice handle;
	VkQueue graphics_queue;
	VkQueue compute_queue;
} LogicalDevice;

typedef struct Swapchain {
	VkSwapchainKHR handle;
	uint64_t image_count;
	VkFormat format;
	VkPresentModeKHR present_mode;
	VkExtent2D extent;
	VkImage *images;
	VkImageView *image_views;
} Swapchain;

typedef struct FrameResources {
	VkCommandBuffer command_buffers[MAX_FRAMES_IN_FLIGHT];
	VkSemaphore image_available_semaphores[MAX_FRAMES_IN_FLIGHT];
	VkSemaphore render_finished_semaphores[MAX_FRAMES_IN_FLIGHT];
	VkFence fences[MAX_FRAMES_IN_FLIGHT];
} FrameResources;

typedef struct DescriptorSet {
	VkDescriptorSet handle;
	VkDescriptorSetLayout layout;
	VkDescriptorPool pool;
} DescriptorSet;

typedef struct Pipeline {
	VkPipeline handle;
	VkPipelineLayout layout;
} Pipeline;

typedef struct Image {
	VkImage handle;
	VkImageView view;
	VkDeviceMemory memory;
} Image;

typedef struct MappedBuffer {
	VkBuffer handle;
	void *data;
	VkDeviceMemory memory;
} MappedBuffer;

typedef struct GlyphPushConstants {
	int glyph_width;
	int glyph_height;
	int ascent;
	int descent;
} GlyphPushConstants;

typedef struct GlyphResources {
	DescriptorSet descriptor_set;

	GlyphPushConstants glyph_push_constants;
	Image glyph_atlas;
	MappedBuffer glyph_lines_buffer;
	MappedBuffer glyph_offsets_buffer;

	Pipeline pipeline;
} GlyphResources;

typedef struct GraphicsPushConstants {
	int glyph_width;
	int glyph_height;
	float glyph_width_to_height_ratio;
	float font_size;
} GraphicsPushConstants;

typedef struct Vertex {
	uint32_t pos : 3;
	uint32_t uv : 3;
	uint32_t glyph_offset_x : 8;
	uint32_t glyph_offset_y : 5;
	uint32_t cell_offset_x : 8;
	uint32_t cell_offset_y : 5;
} Vertex;

typedef enum ShaderType {
	SHADER_TYPE_COMPUTE,
	SHADER_TYPE_VERTEX,
	SHADER_TYPE_FRAGMENT
} ShaderType;

typedef struct Renderer {
	HWND hwnd;

	VkInstance instance;
	VkSurfaceKHR surface;

	LogicalDevice logical_device;
	PhysicalDevice physical_device;
	Swapchain swapchain;

	VkCommandPool command_pool;
	FrameResources frame_resources;

	DescriptorSet descriptor_set;
	VkSampler texture_sampler;
	VkRenderPass render_pass;
	Pipeline graphics_pipeline;

	GlyphResources glyph_resources;

	MappedBuffer vertex_buffer;

#ifndef NDEBUG
	VkDebugUtilsMessengerEXT debug_messenger;
#endif
} Renderer;

typedef struct Point {
	float x;
	float y;
} Point;

typedef struct Line {
	Point a;
	Point b;
} Line;

typedef struct GlyphOffset {
	uint32_t offset;
	uint32_t num_lines;
	uint64_t padding;
} GlyphOffset;

typedef struct TesselatedGlyphs {
	Line *lines;
	uint64_t num_lines;
	GlyphOffset *glyph_offsets;
	uint64_t num_glyphs;
	GlyphPushConstants glyph_push_constants;
} TesselatedGlyphs;


