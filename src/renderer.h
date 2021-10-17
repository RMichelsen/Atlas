#pragma once

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

typedef struct GlyphPoint {
	float x;
	float y;
} GlyphPoint;

typedef struct GlyphLine {
	GlyphPoint a;
	GlyphPoint b;
} GlyphLine;

typedef struct GlyphOffset {
	u32 offset;
	u32 num_lines;
	float min_y;
	u32 padding;
} GlyphOffset;

typedef struct GlyphMetrics {
	float glyph_width;
	float glyph_height;
	u32 cell_width;
	u32 cell_height;
} GlyphMetrics;

typedef struct TessellatedGlyphs {
	GlyphLine *lines;
	u64 num_lines;
	GlyphOffset *glyph_offsets;
	u64 num_glyphs;
	GlyphMetrics metrics;
} TessellatedGlyphs;

typedef struct GlyphPushConstants {
	float glyph_width;
	float glyph_height;
	u32 cell_width;
	u32 cell_height;
} GlyphPushConstants;

typedef struct GlyphAtlas {
	Image atlas;
	GlyphMetrics metrics;
	MappedBuffer lines_buffer;
	MappedBuffer offsets_buffer;
} GlyphAtlas;

typedef struct GlyphResources {
	DescriptorSet descriptor_set;
	Pipeline pipeline;

	GlyphAtlas glyph_atlas;
} GlyphResources;

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

static Renderer renderer_initialize(HINSTANCE hinstance, HWND hwnd);
static void renderer_destroy(Renderer *renderer);

static void renderer_resize(Renderer *renderer);
static void renderer_update_draw_lists(Renderer *renderer, DrawList *draw_lists, u32 num_draw_lists);
static void renderer_present(Renderer *renderer);

static u32 renderer_get_number_of_lines_on_screen(Renderer *renderer);

