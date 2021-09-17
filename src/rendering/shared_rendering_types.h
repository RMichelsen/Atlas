#pragma once
#include <core/common_types.h>
#include <vulkan/vulkan.h>

#define VK_CHECK(x) if((x) != VK_SUCCESS) { 			\
	assert(false); 										\
	printf("Vulkan error: %s:%i", __FILE__, __LINE__); 	\
}

#define MAX_FRAMES_IN_FLIGHT 3

typedef struct PhysicalDevice {
	VkPhysicalDevice handle;
	VkPhysicalDeviceProperties2 properties;
	VkPhysicalDeviceMemoryProperties memory_properties;
	VkSurfaceCapabilitiesKHR surface_capabilities;
	u32 graphics_family_idx;
	u32 compute_family_idx;
} PhysicalDevice;

typedef struct LogicalDevice {
	VkDevice handle;
	VkQueue graphics_queue;
	VkQueue compute_queue;
} LogicalDevice;

typedef struct Swapchain {
	VkSwapchainKHR handle;
	u64 image_count;
	VkFormat format;
	VkPresentModeKHR present_mode;
	VkExtent2D extent;
	VkImage *images;
	VkImageView *image_views;
} Swapchain;

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
	u32 pos : 3;
	u32 uv : 3;
	u32 glyph_offset_x : 8;
	u32 glyph_offset_y : 5;
	u32 cell_offset_x : 8;
	u32 cell_offset_y : 5;
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
	VkCommandBuffer command_buffers[MAX_FRAMES_IN_FLIGHT];
	VkSemaphore image_available_semaphores[MAX_FRAMES_IN_FLIGHT];
	VkSemaphore render_finished_semaphores[MAX_FRAMES_IN_FLIGHT];
	VkFence fences[MAX_FRAMES_IN_FLIGHT];
	VkFramebuffer framebuffers[MAX_FRAMES_IN_FLIGHT];

	DescriptorSet descriptor_set;
	VkSampler texture_sampler;
	VkRenderPass render_pass;
	Pipeline graphics_pipeline;
	MappedBuffer vertex_buffer;

	GlyphResources glyph_resources;

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
	u32 offset;
	u32 num_lines;
	u64 padding;
} GlyphOffset;

typedef struct TesselatedGlyphs {
	Line *lines;
	u64 num_lines;
	GlyphOffset *glyph_offsets;
	u64 num_glyphs;
	GlyphPushConstants glyph_push_constants;
} TesselatedGlyphs;


