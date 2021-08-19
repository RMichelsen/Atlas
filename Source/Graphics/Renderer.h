#pragma once

#include "Graphics/VulkanAllocator.h"
#include "Graphics/RenderTypes.h"

#define MAX_FRAMES_IN_FLIGHT 3

struct PhysicalDevice {
	VkPhysicalDevice handle;
	VkPhysicalDeviceProperties2 properties;
	VkPhysicalDeviceMemoryProperties memory_properties;
	VkSurfaceCapabilitiesKHR surface_capabilities;
	uint32_t graphics_family_idx;
	uint32_t compute_family_idx;
};

struct LogicalDevice {
	VkDevice handle;
	VkQueue graphics_queue;
	VkQueue compute_queue;
};

struct Swapchain {
	VkSwapchainKHR handle;
	uint64_t image_count;
	VkFormat format;
	VkPresentModeKHR present_mode;
	VkExtent2D extent;
	VkImage *images;
	VkImageView *image_views;
};

struct FrameResources {
	VkCommandBuffer command_buffers[MAX_FRAMES_IN_FLIGHT];
	VkSemaphore image_available_semaphores[MAX_FRAMES_IN_FLIGHT];
	VkSemaphore render_finished_semaphores[MAX_FRAMES_IN_FLIGHT];
	VkFence fences[MAX_FRAMES_IN_FLIGHT];
};

struct DescriptorSet {
	VkDescriptorSet handle;
	VkDescriptorSetLayout layout;
	VkDescriptorPool pool;
};

struct Pipeline {
	VkPipeline handle;
	VkPipelineLayout layout;
};

struct GlyphResources {
	DescriptorSet descriptor_set;

	GlyphPushConstants glyph_push_constants;
	Image glyph_atlas;
	MappedBuffer glyph_lines_buffer;
	MappedBuffer glyph_offsets_buffer;

	Pipeline pipeline;
};

struct Vertex {
	float position[2];
	float uv[2];
};

struct Renderer {
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
};

Renderer RendererInitialize(HINSTANCE hinstance, HWND hwnd);
void RendererResize(Renderer *renderer);
void RendererUpdate(HWND hwnd, Renderer *renderer);
void RendererDestroy(Renderer *renderer);

