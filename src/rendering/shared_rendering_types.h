#pragma once
#include <core/common_types.h>
#include <vulkan/vulkan.h>

#define VK_CHECK(x) if((x) != VK_SUCCESS) { 			\
	assert(false); 										\
	printf("Vulkan error: %s:%i", __FILE__, __LINE__); 	\
}

#define MAX_FRAMES_IN_FLIGHT 3
#define NUM_PRINTABLE_CHARS 95

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
	float glyph_width;
	float glyph_height;
	u32 cell_width;
	u32 cell_height;
} GlyphPushConstants;

typedef struct GlyphMetrics {
	float glyph_width;
	float glyph_height;
	u32 cell_width;
	u32 cell_height;
} GlyphMetrics;

typedef struct GlyphAtlas {
	Image atlas;
	GlyphMetrics metrics;
} GlyphAtlas;

typedef struct GlyphResources {
	DescriptorSet descriptor_set;

	GlyphAtlas glyph_atlas;

	MappedBuffer glyph_lines_buffer;
	MappedBuffer glyph_offsets_buffer;

	Pipeline pipeline;
} GlyphResources;

typedef struct GraphicsPushConstants {
	float display_size[2];
	float glyph_width;
	float glyph_height;
	u32 cell_width;
	u32 cell_height;
	u32 glyph_atlas_size;
} GraphicsPushConstants;

typedef struct Vertex {
	u64 pos : 3;
	u64 uv : 3;
	u64 glyph_offset_x : 13;
	u64 glyph_offset_y : 13;
	u64 cell_offset_x : 16;
	u64 cell_offset_y : 16;
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

	u32 active_vertex_count;

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
	GlyphMetrics metrics;
} TesselatedGlyphs;

typedef enum DrawCommandType {
	DRAW_COMMAND_TEXT,
	DRAW_COMMAND_RECT
} DrawCommandType;

typedef struct DrawCommandText {
	const char *content;
	u32 row;
	u32 col;
} DrawCommandText;

typedef struct DrawCommandRect {
	u32 dummy;
} DrawCommandRect;

typedef struct DrawCommand {
	DrawCommandType type;
	union {
		DrawCommandText text;
		DrawCommandRect rect;
	};
} DrawCommand;

typedef struct DrawCommands {
	DrawCommand *commands;
	u32 num_commands;
} DrawCommands;
