#include "renderer.h"

#define GLYPH_ATLAS_SIZE 2048
#define MAX_TOTAL_GLYPH_LINES 65536
#define NUM_PRINTABLE_CHARS 95

#define VK_CHECK(x) if((x) != VK_SUCCESS) { 			\
	assert(false); 										\
	printf("Vulkan error: %s:%i", __FILE__, __LINE__); 	\
}

typedef struct TessellationContext {
	GlyphLine *lines;
	u32 num_lines;
	GlyphPoint last_point;
} TessellationContext;

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

const char *LAYERS[] = { "VK_LAYER_KHRONOS_validation" };
const char *INSTANCE_EXTENSIONS[] = {
	VK_KHR_SURFACE_EXTENSION_NAME,
#ifdef _WIN32
	VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#else
	VK_KHR_XCB_SURFACE_EXTENSION_NAME,
#endif
	VK_EXT_DEBUG_UTILS_EXTENSION_NAME
};
const char *DEVICE_EXTENSIONS[] = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

#ifndef NDEBUG
static VKAPI_ATTR VkBool32 VKAPI_CALL debug_messenger_callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
	VkDebugUtilsMessageTypeFlagsEXT message_type,
	const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
	void *user_data) {
	printf("%s\n", callback_data->pMessage);
	return VK_FALSE;
}

static VkDebugUtilsMessengerEXT create_debug_messenger(VkInstance instance) {
	VkDebugUtilsMessengerCreateInfoEXT debug_utils_messenger_info = {
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
		.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
						   VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
						   VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
		.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
					   VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
					   VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
		.pfnUserCallback = debug_messenger_callback
	};

	VkDebugUtilsMessengerEXT debug_messenger;
	VK_CHECK(((PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, 
		"vkCreateDebugUtilsMessengerEXT"))(instance, &debug_utils_messenger_info, NULL, &debug_messenger));
	return debug_messenger;
}
#endif

static VkInstance create_instance() {
	VkInstanceCreateInfo instance_info = {
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pNext = &(VkValidationFeaturesEXT) {
			.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT,
			.enabledValidationFeatureCount = 1,
			.pEnabledValidationFeatures = &(VkValidationFeatureEnableEXT) { 
				VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT 
			}
		},
		.pApplicationInfo = &(VkApplicationInfo) {
			.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
			.apiVersion = VK_API_VERSION_1_1
		},
#ifndef NDEBUG
		.enabledLayerCount = ARRAY_LENGTH(LAYERS),
		.ppEnabledLayerNames = LAYERS,
#endif
		.enabledExtensionCount = ARRAY_LENGTH(INSTANCE_EXTENSIONS),
		.ppEnabledExtensionNames = INSTANCE_EXTENSIONS
	};

	VkInstance instance;
	VK_CHECK(vkCreateInstance(&instance_info, NULL, &instance));
	return instance;
}

static VkSurfaceKHR create_surface(VkInstance instance, Window window) {
	VkSurfaceKHR surface;
#ifdef _WIN32
	VkWin32SurfaceCreateInfoKHR win32_surface_info = {
		.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
		.hinstance = window.instance,
		.hwnd = window.handle
	};
	VK_CHECK(vkCreateWin32SurfaceKHR(instance, &win32_surface_info, NULL, &surface));
#else
    VkXcbSurfaceCreateInfoKHR xcb_surface_info = {
        .sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,
        .connection = window.connection,
        .window = window.handle
    };
    VK_CHECK(vkCreateXcbSurfaceKHR(instance, &xcb_surface_info, NULL, &surface));
#endif
	return surface;
}

static PhysicalDevice create_physical_device(VkInstance instance, VkSurfaceKHR surface) {
	u32 device_count = 0;
	vkEnumeratePhysicalDevices(instance, &device_count, NULL);
	assert(device_count > 0 && "No Vulkan 1.2 capable devices found");

	VkPhysicalDevice *physical_devices = (VkPhysicalDevice *)malloc(device_count * sizeof(VkPhysicalDevice));
	VK_CHECK(vkEnumeratePhysicalDevices(instance, &device_count, physical_devices));

	// Find the first discrete GPU
	VkPhysicalDeviceProperties properties;
	VkPhysicalDevice physical_device = VK_NULL_HANDLE;
	for(u32 i = 0; i < device_count; ++i) {
		vkGetPhysicalDeviceProperties(physical_devices[i], &properties);
		if(properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
			continue;
		}
		physical_device = physical_devices[i];
		break;
	}
	assert(physical_device != VK_NULL_HANDLE);
	free(physical_devices);


	VkPhysicalDeviceProperties2 device_properties = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
	};

	VkSurfaceCapabilitiesKHR surface_capabilities;
	vkGetPhysicalDeviceProperties2(physical_device, &device_properties);
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &surface_capabilities);

	VkPhysicalDeviceMemoryProperties memory_properties;
	vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);

	u32 queue_family_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, NULL);
	assert(queue_family_count > 0);

	VkQueueFamilyProperties *queue_families = (VkQueueFamilyProperties *)malloc(queue_family_count * sizeof(VkQueueFamilyProperties));
	vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_families);

	u32 graphics_family_idx = UINT32_MAX;
	u32 compute_family_idx = UINT32_MAX;
	for(u32 i = 0; i < queue_family_count; ++i) {
		if(queue_families[i].queueCount == 0) {
			continue;
		}

		if(queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			graphics_family_idx = i;
		}
		if(queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
			compute_family_idx = i;
		}
	}

	VkBool32 supports_present = VK_FALSE;
	VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, graphics_family_idx, surface, &supports_present));
	assert(supports_present);

	return (PhysicalDevice) {
		.handle = physical_device,
		.properties = device_properties,
		.memory_properties = memory_properties,
		.surface_capabilities = surface_capabilities,
		.graphics_family_idx = graphics_family_idx,
		.compute_family_idx = compute_family_idx
	};
}

static LogicalDevice create_logical_device(PhysicalDevice physical_device) {
	VkDeviceQueueCreateInfo device_queue_infos[] = {
		{
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.queueFamilyIndex = physical_device.graphics_family_idx,
			.queueCount = 1,
			.pQueuePriorities = &(float) { 1.0f }
		},
		{
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.queueFamilyIndex = physical_device.compute_family_idx,
			.queueCount = 1,
			.pQueuePriorities = &(float) { 1.0f }
		}
	};

	VkDeviceCreateInfo device_info = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.pNext = NULL,
		.queueCreateInfoCount =
			physical_device.graphics_family_idx == physical_device.compute_family_idx ?
			1 : ARRAY_LENGTH(device_queue_infos),
		.pQueueCreateInfos = device_queue_infos,
#ifndef NDEBUG
		.enabledLayerCount = ARRAY_LENGTH(LAYERS),
		.ppEnabledLayerNames = LAYERS,
#endif
		.enabledExtensionCount = ARRAY_LENGTH(DEVICE_EXTENSIONS),
		.ppEnabledExtensionNames = DEVICE_EXTENSIONS,
		.pEnabledFeatures = &(VkPhysicalDeviceFeatures) {
			.shaderStorageImageWriteWithoutFormat = VK_TRUE,
			.shaderFloat64 = VK_TRUE,
			.shaderInt64 = VK_TRUE
		}
	};

	VkDevice device;
	VkQueue graphics_queue;
	VkQueue compute_queue;
	VK_CHECK(vkCreateDevice(physical_device.handle, &device_info, NULL, &device));
	vkGetDeviceQueue(device, physical_device.graphics_family_idx, 0, &graphics_queue);
	vkGetDeviceQueue(device, physical_device.compute_family_idx, 0, &compute_queue);

	return (LogicalDevice) {
		.handle = device,
		.graphics_queue = graphics_queue,
		.compute_queue = compute_queue
	};
}

static VkCommandPool create_command_pool(PhysicalDevice physical_device, LogicalDevice logical_device) {
	VkCommandPoolCreateInfo command_pool_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT |
					VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
		.queueFamilyIndex = physical_device.graphics_family_idx
	};
	VkCommandPool command_pool;
	VK_CHECK(vkCreateCommandPool(logical_device.handle, &command_pool_info, NULL, &command_pool));
	return command_pool;
}

static Swapchain create_swapchain(Window window, VkSurfaceKHR surface, PhysicalDevice physical_device,
	LogicalDevice logical_device, Swapchain *old_swapchain) {
	VK_CHECK(vkDeviceWaitIdle(logical_device.handle));

#ifdef _WIN32
	RECT client_rect;
	GetClientRect(window.handle, &client_rect);
	VkExtent2D extent = {
		.width = (u32)client_rect.right,
		.height = (u32)client_rect.bottom
	};
#else
    xcb_get_geometry_reply_t *geometry = xcb_get_geometry_reply(
        window.connection, 
        xcb_get_geometry(window.connection, window.handle),
        NULL
    );
	VkExtent2D extent = {
		.width = (u32)geometry->width,
		.height = (u32)geometry->height
	};
#endif

	u32 desired_image_count = physical_device.surface_capabilities.minImageCount + 1;
	if(physical_device.surface_capabilities.maxImageCount > 0 &&
		desired_image_count > physical_device.surface_capabilities.maxImageCount) {
		desired_image_count = physical_device.surface_capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR swapchain_info = {
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = surface,
		.minImageCount = desired_image_count,
		.imageFormat = VK_FORMAT_B8G8R8A8_UNORM,
		.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
		.imageExtent = extent,
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.preTransform = physical_device.surface_capabilities.currentTransform,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR,
		.clipped = VK_TRUE,
		.oldSwapchain = old_swapchain ? old_swapchain->handle : VK_NULL_HANDLE
	};

	VkSwapchainKHR swapchain;
	VK_CHECK(vkCreateSwapchainKHR(logical_device.handle, &swapchain_info, NULL, &swapchain));

	// Delete old swapchain
	if(old_swapchain) {
		for(int i = 0; i < old_swapchain->image_count; ++i) {
			vkDestroyImageView(logical_device.handle, old_swapchain->image_views[i], NULL);
		}

		vkDestroySwapchainKHR(logical_device.handle, old_swapchain->handle, NULL);
		free(old_swapchain->images);
		free(old_swapchain->image_views);
	}

	u32 image_count = 0;
	VK_CHECK(vkGetSwapchainImagesKHR(logical_device.handle, swapchain, &image_count, NULL));

	VkImage *images = (VkImage *)malloc(image_count * sizeof(VkImage));
	assert(images);
	VK_CHECK(vkGetSwapchainImagesKHR(logical_device.handle, swapchain, &image_count, images));

	VkImageView *image_views = (VkImageView *)malloc(image_count * sizeof(VkImageView));
	assert(image_views);
	for(u32 i = 0; i < image_count; ++i) {
		VkImageViewCreateInfo image_view_info = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = images[i],
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = swapchain_info.imageFormat,
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
		VK_CHECK(vkCreateImageView(logical_device.handle, &image_view_info, NULL, &image_views[i]));
	}

	return (Swapchain) {
		.handle = swapchain,
		.image_count = image_count,
		.format = swapchain_info.imageFormat,
		.present_mode = swapchain_info.presentMode,
		.extent = extent,
		.images = images,
		.image_views = image_views
	};
}

static void initialize_frame_resources(Renderer *renderer) {
	VkSemaphoreCreateInfo semaphore_info = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
	};
	VkFenceCreateInfo fence_info = {
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.flags = VK_FENCE_CREATE_SIGNALED_BIT
	};
	VkCommandBufferAllocateInfo command_buffer_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = renderer->command_pool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1
	};

	for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
		VK_CHECK(vkAllocateCommandBuffers(renderer->logical_device.handle, &command_buffer_info, &renderer->command_buffers[i]));
		VK_CHECK(vkCreateSemaphore(renderer->logical_device.handle, &semaphore_info, NULL, &renderer->image_available_semaphores[i]));
		VK_CHECK(vkCreateSemaphore(renderer->logical_device.handle, &semaphore_info, NULL, &renderer->render_finished_semaphores[i]));
		VK_CHECK(vkCreateFence(renderer->logical_device.handle, &fence_info, NULL, &renderer->fences[i]));
		renderer->framebuffers[i] = VK_NULL_HANDLE;
	}
}

static VkRenderPass create_render_pass(LogicalDevice logical_device, Swapchain swapchain) {
	VkRenderPassCreateInfo render_pass_info = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = &(VkAttachmentDescription) {
			.format = swapchain.format,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
		},
		.subpassCount = 1,
		.pSubpasses = &(VkSubpassDescription) {
			.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.colorAttachmentCount = 1,
			.pColorAttachments = &(VkAttachmentReference) {
				.attachment = 0,
				.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			}
		},
		.dependencyCount = 1,
		.pDependencies = &(VkSubpassDependency) {
			.srcSubpass = VK_SUBPASS_EXTERNAL,
			.dstSubpass = 0,
			.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.srcAccessMask = 0,
			.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
							 VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
		}
	};

	VkRenderPass render_pass;
	VK_CHECK(vkCreateRenderPass(logical_device.handle, &render_pass_info, NULL, &render_pass));
	return render_pass;
}

static VkShaderModule create_shader_module(VkDevice device, ShaderType shader_type, const char *shader_source) {
	char path[FILENAME_MAX] = { 0 };
	strcat(path, shader_source);
	strcat(path, ".spv");
    
    FILE *file = fopen(path, "rb");
    assert(file);

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);

    void *bytecode = malloc(file_size);
    fread(bytecode, file_size, 1, file);
    fclose(file);

	VkShaderModuleCreateInfo shader_module_info = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = file_size,
		.pCode = bytecode
	};

	VkShaderModule shader_module = VK_NULL_HANDLE;
	vkCreateShaderModule(device, &shader_module_info, NULL, &shader_module);

	free(bytecode);
	return shader_module;
}


static Pipeline create_rasterization_pipeline(VkInstance instance, LogicalDevice logical_device,
	Swapchain swapchain, VkRenderPass render_pass,
	DescriptorSet descriptor_set) {

	VkPipelineLayoutCreateInfo layout_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = &descriptor_set.layout,
		.pushConstantRangeCount = 1,
		.pPushConstantRanges = &(VkPushConstantRange) {
			.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
			.offset = 0,
			.size = sizeof(GraphicsPushConstants)
		}
	};
	VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
	VK_CHECK(vkCreatePipelineLayout(logical_device.handle, &layout_info, NULL, &pipeline_layout));

	VkPipelineShaderStageCreateInfo shader_stage_infos[] = {
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = create_shader_module(logical_device.handle, SHADER_TYPE_VERTEX, "vertex.vert"),
			.pName = "main"
		},
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = create_shader_module(logical_device.handle, SHADER_TYPE_FRAGMENT, "fragment.frag"),
			.pName = "main"
		},
	};

	VkGraphicsPipelineCreateInfo graphics_pipeline_info = {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.stageCount = ARRAY_LENGTH(shader_stage_infos),
		.pStages = shader_stage_infos,
		.pVertexInputState = &(VkPipelineVertexInputStateCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			.vertexBindingDescriptionCount = 1,
			.pVertexBindingDescriptions = &(VkVertexInputBindingDescription) {
				.binding = 0,
				.stride = sizeof(Vertex),
				.inputRate = VK_VERTEX_INPUT_RATE_VERTEX
			},
			.vertexAttributeDescriptionCount = 1,
			.pVertexAttributeDescriptions = &(VkVertexInputAttributeDescription) {
				.location = 0,
				.binding = 0,
				.format = VK_FORMAT_R32G32_UINT,
				.offset = 0
			}
		},
		.pInputAssemblyState = &(VkPipelineInputAssemblyStateCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
		},
		.pViewportState = &(VkPipelineViewportStateCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			.viewportCount = 1,
			.pViewports = &(VkViewport) {
				.width = (float)swapchain.extent.width,
				.height = (float)swapchain.extent.height,
				.minDepth = 0.0f,
				.maxDepth = 1.0f
			},
			.scissorCount = 1,
			.pScissors = &(VkRect2D) {
				.extent = swapchain.extent
			}
		},
		.pRasterizationState = &(VkPipelineRasterizationStateCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.polygonMode = VK_POLYGON_MODE_FILL,
			.cullMode = VK_CULL_MODE_NONE,
			.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
			.lineWidth = 1.0f
		},
		.pMultisampleState = &(VkPipelineMultisampleStateCreateInfo) {
		   .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		   .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
		},
		.pDepthStencilState = &(VkPipelineDepthStencilStateCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			.minDepthBounds = 0.0f,
			.maxDepthBounds = 1.0f
		},
		.pColorBlendState = &(VkPipelineColorBlendStateCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.attachmentCount = 1,
			.pAttachments = &(VkPipelineColorBlendAttachmentState) {
				.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
					VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
			}
		},
		.layout = pipeline_layout,
		.renderPass = render_pass,
		.subpass = 0
	};

	VkPipeline pipeline;
	VK_CHECK(vkCreateGraphicsPipelines(logical_device.handle, VK_NULL_HANDLE, 1, &graphics_pipeline_info,
		NULL, &pipeline));

	vkDestroyShaderModule(logical_device.handle, shader_stage_infos[0].module, NULL);
	vkDestroyShaderModule(logical_device.handle, shader_stage_infos[1].module, NULL);

	return (Pipeline) {
		.handle = pipeline,
		.layout = pipeline_layout
	};
}

static VkCommandBuffer start_one_time_command_buffer(LogicalDevice logical_device, VkCommandPool command_pool) {
	VkCommandBufferAllocateInfo command_buffer_allocate_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = command_pool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1
	};

	VkCommandBuffer command_buffer;
	VK_CHECK(vkAllocateCommandBuffers(logical_device.handle, &command_buffer_allocate_info,
		&command_buffer));

	VkCommandBufferBeginInfo command_buffer_begin_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	};
	VK_CHECK(vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info));
	return command_buffer;
}

static void transition_glyph_image(LogicalDevice logical_device, VkCommandBuffer command_buffer, Image image) {
	VkImageMemoryBarrier memory_barrier = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcAccessMask = 0,
		.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout = VK_IMAGE_LAYOUT_GENERAL,
		.image = image.handle,
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.levelCount = 1,
			.layerCount = 1
		}
	};

	vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL,
		0, NULL, 1, &memory_barrier);

}

static void end_one_time_command_buffer(LogicalDevice logical_device, VkCommandBuffer command_buffer,
	VkCommandPool command_pool) {
	VK_CHECK(vkEndCommandBuffer(command_buffer));

	VkSubmitInfo submit_info = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = 1,
		.pCommandBuffers = &command_buffer
	};

	VkFenceCreateInfo fence_info = {
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO
	};
	VkFence fence;
	VK_CHECK(vkCreateFence(logical_device.handle, &fence_info, NULL, &fence));

	VK_CHECK(vkQueueSubmit(logical_device.graphics_queue, 1, &submit_info, fence));
	VK_CHECK(vkWaitForFences(logical_device.handle, 1, &fence, VK_TRUE, UINT64_MAX));

	vkFreeCommandBuffers(logical_device.handle, command_pool, 1, &command_buffer);
	vkDestroyFence(logical_device.handle, fence, NULL);
}

// Function from Vulkan spec 1.0.183
static u32 find_memory_type_index(VkPhysicalDeviceMemoryProperties memory_properties,
	u32 memory_type_bits_requirements,
	VkMemoryPropertyFlags required_properties) {
	u32 memory_count = memory_properties.memoryTypeCount;

	for(u32 memory_index = 0; memory_index < memory_count; ++memory_index) {
		VkMemoryPropertyFlags properties = memory_properties.memoryTypes[memory_index].propertyFlags;
		bool is_required_memory_type = memory_type_bits_requirements & (1 << memory_index);
		bool has_required_properties = (properties & required_properties) == required_properties;

		if(is_required_memory_type && has_required_properties) {
			return (u32)memory_index;
		}
	}

	// Failed to find memory type
	assert(false);
	return UINT32_MAX;
}

static VkDeviceMemory allocate_memory(VkDevice device, VkPhysicalDeviceMemoryProperties memory_properties,
	VkDeviceSize size, u32 memory_type_bits_requirements,
	VkMemoryPropertyFlags memory_property_flags) {
	VkMemoryAllocateInfo memory_allocate_info = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = size,
		.memoryTypeIndex = find_memory_type_index(memory_properties, memory_type_bits_requirements,
											   memory_property_flags)
	};

	VkDeviceMemory memory = VK_NULL_HANDLE;
	VK_CHECK(vkAllocateMemory(device, &memory_allocate_info, NULL, &memory));
	return memory;
}

static u64 align_up(u64 value, u64 alignment) {
	return ((value + alignment - 1) / alignment) * alignment;
}

static Image create_image_2d(VkDevice device, VkPhysicalDeviceMemoryProperties memory_properties,
	u32 width, u32 height, VkFormat format) {
	VkImageCreateInfo image_info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = format,
		.extent = (VkExtent3D) {
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
	VK_CHECK(vkCreateImage(device, &image_info, NULL, &image));

	VkMemoryRequirements memory_requirements;
	vkGetImageMemoryRequirements(device, image, &memory_requirements);

	u64 aligned_size = align_up(memory_requirements.size, memory_requirements.alignment);
	VkDeviceMemory memory = allocate_memory(device, memory_properties, aligned_size,
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

	return (Image) {
		.handle = image,
		.view = image_view,
		.memory = memory
	};
}

static MappedBuffer create_mapped_buffer(VkDevice device, VkPhysicalDeviceMemoryProperties memory_properties,
	VkBufferUsageFlags usage, u64 size) {
	VkBufferCreateInfo buffer_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = size,
		.usage = usage,
	};
	VkBuffer buffer;
	VK_CHECK(vkCreateBuffer(device, &buffer_info, NULL, &buffer));

	VkMemoryRequirements memory_requirements;
	vkGetBufferMemoryRequirements(device, buffer, &memory_requirements);

	u64 aligned_size = align_up(memory_requirements.size, memory_requirements.alignment);
	VkDeviceMemory memory = allocate_memory(device, memory_properties, aligned_size,
		memory_requirements.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
		VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	VK_CHECK(vkBindBufferMemory(device, buffer, memory, 0));

	void *mapped_data = NULL;
	VK_CHECK(vkMapMemory(device, memory, 0, aligned_size, 0, &mapped_data));

	return (MappedBuffer) {
		.handle = buffer,
			.data = mapped_data,
			.memory = memory
	};
}

static float fixed_to_float(signed long x) {
	return (float)x / 64.0f;
}

// https://members.loria.fr/samuel.hornus/quadratic-arc-length.html
static float get_bezier_arc_length(GlyphPoint a, GlyphPoint b, GlyphPoint c) {
	float Bx = b.x - a.x;
	float By = b.y - a.y;
	float Fx = c.x - b.x;
	float Fy = c.y - b.y;
	float Ax = Fx - Bx;
	float Ay = Fy - By;
	float A_dot_B = Ax * Bx + Ay * By;
	float A_dot_F = Ax * Fx + Ay * Fy;
	float B_magn = sqrt(Bx * Bx + By * By);
	float F_magn = sqrt(Fx * Fx + Fy * Fy);
	float A_magn = sqrt(Ax * Ax + Ay * Ay);

	if (B_magn == 0.0f || F_magn == 0.0f || A_magn == 0.0f) {
		return 0.0f;
	}

	float l1 = (F_magn * A_dot_F - B_magn * A_dot_B) / (A_magn * A_magn);
	float l2 = ((B_magn * B_magn) / A_magn) - ((A_dot_B * A_dot_B) / (A_magn * A_magn * A_magn));
	float l3 = logf(A_magn * F_magn + A_dot_F) - logf(A_magn * B_magn + A_dot_B);

	if (isnan(l3)) {
		return 0.0f;
	}

	return l1 + l2 * l3;
}

static void add_straight_line(GlyphPoint p1, GlyphPoint p2, TessellationContext *context) {
    assert(context->num_lines < MAX_TOTAL_GLYPH_LINES);
	context->lines[context->num_lines++] = p1.y > p2.y ?
		(GlyphLine) { p1, p2 } : (GlyphLine) { p2, p1 };
	return;
}

static void add_quadratic_spline(GlyphPoint p1, GlyphPoint p2, GlyphPoint p3, TessellationContext *context) {
	if ((p1.x == p2.x && p2.x == p3.x) || (p1.y == p2.y && p2.y == p3.y)) {
		add_straight_line(p1, p3, context);
		return;
	}

	float length = get_bezier_arc_length(p1, p2, p3);
	u32 number_of_steps = length > 0.0f ? (u32)ceil(length / 0.5f) : 25;
	float step_fraction = 1.0f / number_of_steps;

	for(u32 i = 0; i < number_of_steps; ++i) {
		float t1 = i * step_fraction;
		float t2 = (i + 1) * step_fraction;
		float ax = (1 - t1) * ((1 - t1) * p1.x + t1 * p2.x) + t1 * ((1 - t1) * p2.x + t1 * p3.x);
		float ay = (1 - t1) * ((1 - t1) * p1.y + t1 * p2.y) + t1 * ((1 - t1) * p2.y + t1 * p3.y);
		float bx = (1 - t2) * ((1 - t2) * p1.x + t2 * p2.x) + t2 * ((1 - t2) * p2.x + t2 * p3.x);
		float by = (1 - t2) * ((1 - t2) * p1.y + t2 * p2.y) + t2 * ((1 - t2) * p2.y + t2 * p3.y);

		assert(context->num_lines < MAX_TOTAL_GLYPH_LINES);
		context->lines[context->num_lines++] =
			ay > by ? (GlyphLine) { { ax, ay }, { bx, by } } :
			(GlyphLine) { { bx, by }, { ax, ay }
		};
	}
}

static int outline_move_to(const FT_Vector *to, void *user) {
	TessellationContext *context = (TessellationContext *)user;
	float x = fixed_to_float(to->x);
	float y = fixed_to_float(to->y);
	context->last_point = (GlyphPoint) { x, y };
	return 0;
}
static int outline_line_to(const FT_Vector* to, void *user) {
	TessellationContext *context = (TessellationContext *)user;
	float x = fixed_to_float(to->x);
	float y = fixed_to_float(to->y);
	add_straight_line(
		context->last_point,
		(GlyphPoint) { x, y },
		context
	);
	context->last_point = (GlyphPoint) { x, y };
	return 0;
}
static int outline_conic_to(const FT_Vector* control, const FT_Vector* to, void *user) {
	TessellationContext *context = (TessellationContext *)user;
	float cx = fixed_to_float(control->x);
	float cy = fixed_to_float(control->y);
	float x = fixed_to_float(to->x);
	float y = fixed_to_float(to->y);
	add_quadratic_spline(
		context->last_point,
		(GlyphPoint) { cx, cy },
		(GlyphPoint) { x, y },
		context
	);
	context->last_point = (GlyphPoint) { x, y };
	return 0;
}
static int outline_cubic_to(const FT_Vector* control1, const FT_Vector* control2, 
	const FT_Vector* to, void *user) {
	assert(false);
	return 1;
}

static FT_Outline_Funcs outline_funcs = {
	.move_to = outline_move_to,
	.line_to = outline_line_to,
	.conic_to = outline_conic_to,
	.cubic_to = outline_cubic_to
};

static int cmp_glyph_lines(const void* l1, const void* l2) {
	float l1y = (*(GlyphLine *)l1).a.y;
	float l2y = (*(GlyphLine *)l2).a.y;
	return (l2y > l1y) - (l2y < l1y);
}

static TessellatedGlyphs tessellate_glyphs(const char *font_path, u32 font_size) {
	FT_Library freetype_library;
	FT_Error err = FT_Init_FreeType(&freetype_library);
	assert(!err);

	FT_Face freetype_face;
	err = FT_New_Face(freetype_library, font_path, 0, &freetype_face);
	assert(!err);

	// TODO: Support variable .ttf fonts with overlapping outlines.
	// https://github.com/microsoft/cascadia-code/issues/350

	err = FT_Set_Char_Size(freetype_face, (font_size << 6) * 3, font_size << 6, 0, 0);
	assert(!err);

	TessellationContext tessellation_context = {
		.lines = (GlyphLine *)malloc(MAX_TOTAL_GLYPH_LINES * sizeof(GlyphLine)),
		.num_lines = 0
	};
	GlyphOffset *glyph_offsets = (GlyphOffset *)malloc(NUM_PRINTABLE_CHARS * sizeof(GlyphOffset));

	// For the space character an empty entry is fine
	glyph_offsets[0] = (GlyphOffset) { 0 };

	for(u32 c = 0x21; c <= 0x7E; ++c) {
		u32 index = c - 0x20;

		u32 offset = tessellation_context.num_lines;

		err = FT_Load_Char(freetype_face, c, FT_LOAD_TARGET_LIGHT);
		assert(!err);

		err = FT_Outline_Decompose(&freetype_face->glyph->outline, &outline_funcs, &tessellation_context);
		assert(!err);

		u32 num_lines_in_glyph = tessellation_context.num_lines - offset;
		qsort(&tessellation_context.lines[offset], num_lines_in_glyph, sizeof(GlyphLine), cmp_glyph_lines);

		glyph_offsets[index] = (GlyphOffset) {
			.offset = offset,
			.num_lines = tessellation_context.num_lines - offset
		};
	}

	float glyph_width = freetype_face->glyph->linearHoriAdvance / 65536.0f;
	float glyph_height = (float)(freetype_face->size->metrics.height >> 6);
	float descender = (float)(freetype_face->size->metrics.descender >> 6);

	// Pre-rasterization pass to adjust coordinates to be +Y down
	// and to find the minimum y coordinate so the glyph atlas can take
	// an early out in case a pixel is strictly above the glyph outlines.
	for (u32 c = 0x21; c <= 0x7E; ++c) {
		u32 index = c - 0x20;
		u32 offset = glyph_offsets[index].offset;
		float min_y = glyph_height;
		for (u32 i = 0; i < glyph_offsets[index].num_lines; ++i) {
			tessellation_context.lines[offset + i].a.y = 
				glyph_height - (tessellation_context.lines[offset + i].a.y - descender);
			tessellation_context.lines[offset + i].b.y = 
				glyph_height - (tessellation_context.lines[offset + i].b.y - descender);
			if (tessellation_context.lines[offset + i].a.y < min_y) {
				min_y = tessellation_context.lines[offset + i].a.y;
			}
			if (tessellation_context.lines[offset + i].b.y < min_y) {
				min_y = tessellation_context.lines[offset + i].b.y;
			}
		}
	}

	GlyphMetrics glyph_metrics = {
		.glyph_width = glyph_width,
		.glyph_height = glyph_height,
		.cell_width = (u32)ceil(glyph_width) + 1,
		.cell_height = (u32)ceil(glyph_height) + 1
	};

	FT_Done_Face(freetype_face);
	FT_Done_FreeType(freetype_library);

	return (TessellatedGlyphs) {
		.lines = tessellation_context.lines,
		.num_lines = tessellation_context.num_lines,
		.glyph_offsets = glyph_offsets,
		.num_glyphs = NUM_PRINTABLE_CHARS,
		.metrics = glyph_metrics
	};
}

static GlyphResources create_glyph_resources(Window window, VkInstance instance, 
    PhysicalDevice physical_device, LogicalDevice logical_device) {
	VkDescriptorPoolSize pool_sizes[] = {
		{
			.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			.descriptorCount = 1
		},
		{
			.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.descriptorCount = 2
		}
	};
	VkDescriptorPoolCreateInfo descriptor_pool_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.maxSets = 1,
		.poolSizeCount = ARRAY_LENGTH(pool_sizes),
		.pPoolSizes = pool_sizes
	};
	VkDescriptorPool descriptor_pool;
	VK_CHECK(vkCreateDescriptorPool(logical_device.handle, &descriptor_pool_info,
		NULL, &descriptor_pool));

	VkDescriptorSetLayoutBinding descriptor_set_layout_bindings[] = {
		{
			.binding = 0,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT
		},
		{
			.binding = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT
		},
		{
			.binding = 2,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT
		}
	};
	VkDescriptorSetLayoutCreateInfo descriptor_set_layout_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = ARRAY_LENGTH(descriptor_set_layout_bindings),
		.pBindings = descriptor_set_layout_bindings
	};
	VkDescriptorSetLayout descriptor_set_layout;
	VK_CHECK(vkCreateDescriptorSetLayout(logical_device.handle, &descriptor_set_layout_info,
		NULL, &descriptor_set_layout));

	VkDescriptorSetAllocateInfo descriptor_set_allocate_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = descriptor_pool,
		.descriptorSetCount = 1,
		.pSetLayouts = &descriptor_set_layout
	};
	VkDescriptorSet descriptor_set;
	VK_CHECK(vkAllocateDescriptorSets(logical_device.handle, &descriptor_set_allocate_info, &descriptor_set));

	Image glyph_atlas = create_image_2d(logical_device.handle, physical_device.memory_properties,
		GLYPH_ATLAS_SIZE, GLYPH_ATLAS_SIZE, VK_FORMAT_R16_UINT);

#ifdef _WIN32
	TessellatedGlyphs tessellated_glyphs = tessellate_glyphs("C:/Windows/Fonts/consola.ttf", 26);
#else
	TessellatedGlyphs tessellated_glyphs = tessellate_glyphs("/usr/share/fonts/truetype/ubuntu/UbuntuMono-R.ttf", 30);
#endif

	u32 chars_per_row = (u32)(GLYPH_ATLAS_SIZE / tessellated_glyphs.metrics.glyph_width);
	u32 chars_per_col = (u32)(GLYPH_ATLAS_SIZE / tessellated_glyphs.metrics.glyph_height);
	assert(chars_per_row * chars_per_col >= NUM_PRINTABLE_CHARS);

	MappedBuffer glyph_lines_buffer = create_mapped_buffer(logical_device.handle,
		physical_device.memory_properties,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		tessellated_glyphs.num_lines * sizeof(GlyphLine));
	memcpy(glyph_lines_buffer.data, tessellated_glyphs.lines, tessellated_glyphs.num_lines * sizeof(GlyphLine));
	free(tessellated_glyphs.lines);
	MappedBuffer glyph_offsets_buffer = create_mapped_buffer(logical_device.handle,
		physical_device.memory_properties,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		tessellated_glyphs.num_glyphs * sizeof(GlyphOffset));
	memcpy(glyph_offsets_buffer.data, tessellated_glyphs.glyph_offsets, tessellated_glyphs.num_glyphs * sizeof(GlyphOffset));
	free(tessellated_glyphs.glyph_offsets);

	VkPipelineLayoutCreateInfo layout_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = &descriptor_set_layout,
		.pushConstantRangeCount = 1,
		.pPushConstantRanges = &(VkPushConstantRange) {
			.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
			.offset = 0,
			.size = sizeof(GlyphPushConstants)
		}
	};
	VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
	VK_CHECK(vkCreatePipelineLayout(logical_device.handle, &layout_info, NULL, &pipeline_layout));

	VkPipelineShaderStageCreateInfo shader_stage_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_COMPUTE_BIT,
		.module = create_shader_module(logical_device.handle, SHADER_TYPE_COMPUTE, "write_texture_atlas.comp"),
		.pName = "main"
	};

	VkComputePipelineCreateInfo compute_pipeline_info = {
		.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		.stage = shader_stage_info,
		.layout = pipeline_layout
	};
	VkPipeline pipeline;
	vkCreateComputePipelines(logical_device.handle, NULL, 1, &compute_pipeline_info, NULL, &pipeline);

	vkDestroyShaderModule(logical_device.handle, shader_stage_info.module, NULL);

	return (GlyphResources) {
		.descriptor_set = {
			.handle = descriptor_set,
			.layout = descriptor_set_layout,
			.pool = descriptor_pool
		},
		.pipeline = {
			.handle = pipeline,
			.layout = pipeline_layout
		},
		.glyph_atlas = {
			.atlas = glyph_atlas,
			.metrics = tessellated_glyphs.metrics,
			.lines_buffer = glyph_lines_buffer,
			.offsets_buffer = glyph_offsets_buffer
		}
	};
}

static VkSampler create_texture_sampler(LogicalDevice logical_device) {
	VkSamplerCreateInfo sampler_info = {
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter = VK_FILTER_NEAREST,
		.minFilter = VK_FILTER_NEAREST,
		.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
	};

	VkSampler texture_sampler;
	vkCreateSampler(logical_device.handle, &sampler_info, NULL, &texture_sampler);
	return texture_sampler;
}

static DescriptorSet create_descriptor_set(LogicalDevice logical_device) {
	VkDescriptorPoolCreateInfo descriptor_pool_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.maxSets = 1,
		.poolSizeCount = 1,
		.pPoolSizes = &(VkDescriptorPoolSize) {
			.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.descriptorCount = 1
		}
	};
	VkDescriptorPool descriptor_pool;
	VK_CHECK(vkCreateDescriptorPool(logical_device.handle, &descriptor_pool_info,
		NULL, &descriptor_pool));

	VkDescriptorSetLayoutCreateInfo descriptor_set_layout_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = 1,
		.pBindings = &(VkDescriptorSetLayoutBinding) {
			.binding = 0,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
		}
	};
	VkDescriptorSetLayout descriptor_set_layout;
	VK_CHECK(vkCreateDescriptorSetLayout(logical_device.handle, &descriptor_set_layout_info,
		NULL, &descriptor_set_layout));

	VkDescriptorSetAllocateInfo descriptor_set_allocate_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = descriptor_pool,
		.descriptorSetCount = 1,
		.pSetLayouts = &descriptor_set_layout
	};
	VkDescriptorSet descriptor_set;
	VK_CHECK(vkAllocateDescriptorSets(logical_device.handle, &descriptor_set_allocate_info, &descriptor_set));

	return (DescriptorSet) {
		.handle = descriptor_set,
		.layout = descriptor_set_layout,
		.pool = descriptor_pool
	};
}

static void write_descriptors(LogicalDevice logical_device, GlyphResources *glyph_resources,
	DescriptorSet descriptor_set, VkSampler texture_sampler) {
	VkWriteDescriptorSet write_descriptor_sets[] = {
		{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = glyph_resources->descriptor_set.handle,
			.dstBinding = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			.pImageInfo = &(VkDescriptorImageInfo) {
				.imageView = glyph_resources->glyph_atlas.atlas.view,
				.imageLayout = VK_IMAGE_LAYOUT_GENERAL
			}
		},
		{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = glyph_resources->descriptor_set.handle,
			.dstBinding = 1,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.pBufferInfo = &(VkDescriptorBufferInfo) {
				.buffer = glyph_resources->glyph_atlas.lines_buffer.handle,
				.range = VK_WHOLE_SIZE
			}
		},
		{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = glyph_resources->descriptor_set.handle,
			.dstBinding = 2,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.pBufferInfo = &(VkDescriptorBufferInfo) {
				.buffer = glyph_resources->glyph_atlas.offsets_buffer.handle,
				.range = VK_WHOLE_SIZE
			}
		}
	};

	vkUpdateDescriptorSets(logical_device.handle, ARRAY_LENGTH(write_descriptor_sets),
		write_descriptor_sets, 0, NULL);

	VkWriteDescriptorSet write_descriptor_set = {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = descriptor_set.handle,
		.dstBinding = 0,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.pImageInfo = &(VkDescriptorImageInfo) {
			.sampler = texture_sampler,
			.imageView = glyph_resources->glyph_atlas.atlas.view,
			.imageLayout = VK_IMAGE_LAYOUT_GENERAL
		}
	};
	vkUpdateDescriptorSets(logical_device.handle, 1, &write_descriptor_set, 0, NULL);
}

static void rasterize_glyphs(LogicalDevice logical_device, GlyphResources *glyph_resources, VkCommandPool command_pool) {
	VkCommandBuffer command_buffer = start_one_time_command_buffer(logical_device, command_pool);

	transition_glyph_image(logical_device, command_buffer, glyph_resources->glyph_atlas.atlas);

	vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, glyph_resources->pipeline.handle);
	vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, glyph_resources->pipeline.layout,
		0, 1, &glyph_resources->descriptor_set.handle, 0, NULL);

	GlyphPushConstants push_constants = {
		.glyph_width = glyph_resources->glyph_atlas.metrics.glyph_width,
		.glyph_height = glyph_resources->glyph_atlas.metrics.glyph_height,
		.cell_width = glyph_resources->glyph_atlas.metrics.cell_width,
		.cell_height = glyph_resources->glyph_atlas.metrics.cell_height
	};

	vkCmdPushConstants(command_buffer, glyph_resources->pipeline.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
		sizeof(GlyphPushConstants), &push_constants);
	vkCmdDispatch(command_buffer, GLYPH_ATLAS_SIZE, GLYPH_ATLAS_SIZE, 1);

	end_one_time_command_buffer(logical_device, command_buffer, command_pool);
}

static Renderer renderer_initialize(Window window) {
	VkInstance instance = create_instance();
	VkSurfaceKHR surface = create_surface(instance, window);

#ifndef NDEBUG
	VkDebugUtilsMessengerEXT debug_messenger = create_debug_messenger(instance);
#endif

	PhysicalDevice physical_device = create_physical_device(instance, surface);
	LogicalDevice logical_device = create_logical_device(physical_device);
	Swapchain swapchain = create_swapchain(window, surface, physical_device, logical_device, NULL);
	VkCommandPool command_pool = create_command_pool(physical_device, logical_device);
	VkSampler texture_sampler = create_texture_sampler(logical_device);
	VkRenderPass render_pass = create_render_pass(logical_device, swapchain);
	DescriptorSet descriptor_set = create_descriptor_set(logical_device);
	Pipeline graphics_pipeline = create_rasterization_pipeline(instance, logical_device, swapchain,
		render_pass, descriptor_set);
	GlyphResources glyph_resources = create_glyph_resources(window, instance, physical_device, logical_device);

	write_descriptors(logical_device, &glyph_resources, descriptor_set, texture_sampler);
	rasterize_glyphs(logical_device, &glyph_resources, command_pool);

	MappedBuffer vertex_buffer = create_mapped_buffer(logical_device.handle,
		physical_device.memory_properties,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		sizeof(Vertex) * 1024 * 1024 * 64);

	Renderer renderer = {
		.window = window,
		.instance = instance,
		.surface = surface,
		.logical_device = logical_device,
		.physical_device = physical_device,
		.swapchain = swapchain,
		.command_pool = command_pool,
		.descriptor_set = descriptor_set,
		.texture_sampler = texture_sampler,
		.render_pass = render_pass,
		.graphics_pipeline = graphics_pipeline,
		.vertex_buffer = vertex_buffer,
		.active_vertex_count = 0,
		.glyph_resources = glyph_resources,
#ifndef NDEBUG
		.debug_messenger = debug_messenger
#endif
	};

	initialize_frame_resources(&renderer);
	return renderer;
}


static void renderer_destroy(Renderer *renderer) {
	VkDevice device = renderer->logical_device.handle;

	VK_CHECK(vkDeviceWaitIdle(device));

	for(u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
		vkDestroySemaphore(device, renderer->image_available_semaphores[i], NULL);
		vkDestroySemaphore(device, renderer->render_finished_semaphores[i], NULL);
		vkDestroyFence(device, renderer->fences[i], NULL);
		vkDestroyFramebuffer(device, renderer->framebuffers[i], NULL);
	}

	for(u32 i = 0; i < renderer->swapchain.image_count; ++i) {
		vkDestroyImageView(device, renderer->swapchain.image_views[i], NULL);
	}
	vkDestroySwapchainKHR(device, renderer->swapchain.handle, NULL);
	free(renderer->swapchain.images);
	free(renderer->swapchain.image_views);

	vkDestroyCommandPool(device, renderer->command_pool, NULL);

	// Destroy rasterization resources
	vkDestroyDescriptorSetLayout(device, renderer->descriptor_set.layout, NULL);
	vkDestroyDescriptorPool(device, renderer->descriptor_set.pool, NULL);
	vkDestroySampler(device, renderer->texture_sampler, NULL);
	vkDestroyRenderPass(device, renderer->render_pass, NULL);
	vkDestroyPipelineLayout(device, renderer->graphics_pipeline.layout, NULL);
	vkDestroyPipeline(device, renderer->graphics_pipeline.handle, NULL);
	vkDestroyBuffer(device, renderer->vertex_buffer.handle, NULL);
	vkFreeMemory(device, renderer->vertex_buffer.memory, NULL);

	// Destroy Vulkan glyph resources
	vkDestroyDescriptorSetLayout(device, renderer->glyph_resources.descriptor_set.layout, NULL);
	vkDestroyDescriptorPool(device, renderer->glyph_resources.descriptor_set.pool, NULL);
	vkDestroyImageView(device, renderer->glyph_resources.glyph_atlas.atlas.view, NULL);
	vkDestroyImage(device, renderer->glyph_resources.glyph_atlas.atlas.handle, NULL);
	vkFreeMemory(device, renderer->glyph_resources.glyph_atlas.atlas.memory, NULL);
	vkDestroyBuffer(device, renderer->glyph_resources.glyph_atlas.lines_buffer.handle, NULL);
	vkFreeMemory(device, renderer->glyph_resources.glyph_atlas.lines_buffer.memory, NULL);
	vkDestroyBuffer(device, renderer->glyph_resources.glyph_atlas.offsets_buffer.handle, NULL);
	vkFreeMemory(device, renderer->glyph_resources.glyph_atlas.offsets_buffer.memory, NULL);
	vkDestroyPipelineLayout(device, renderer->glyph_resources.pipeline.layout, NULL);
	vkDestroyPipeline(device, renderer->glyph_resources.pipeline.handle, NULL);

	vkDestroyDevice(device, NULL);

#ifndef NDEBUG
	((PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(renderer->instance,
		"vkDestroyDebugUtilsMessengerEXT"))(renderer->instance, renderer->debug_messenger, NULL);
#endif
	vkDestroySurfaceKHR(renderer->instance, renderer->surface, NULL);
	vkDestroyInstance(renderer->instance, NULL);
}

static void renderer_resize(Renderer *renderer) {
	renderer->swapchain = create_swapchain(renderer->window, renderer->surface, renderer->physical_device,
		renderer->logical_device, &renderer->swapchain);
}

// Note: this function frees the draw commands once they have been processed!
static void renderer_update_draw_lists(Renderer *renderer, DrawList *draw_lists, u32 num_draw_lists) {
	renderer->active_vertex_count = 0;

	Vertex *vertex_data = (Vertex *)renderer->vertex_buffer.data;

	u32 glyphs_per_row = GLYPH_ATLAS_SIZE / renderer->glyph_resources.glyph_atlas.metrics.cell_width;
	for(u32 i = 0; i < num_draw_lists; ++i) {
		DrawList draw_list = draw_lists[i];
		for(u32 j = 0; j < draw_list.num_commands; ++j) {
			DrawCommand command = draw_list.commands[j];
			if(command.type == DRAW_COMMAND_TEXT) {
				for(u32 k = 0; k < command.text.length; ++k) {
					u32 glyph_index = (u32)command.text.content[k] - 0x20;
					for(int h = 0; h < 6; ++h) {
						vertex_data[renderer->active_vertex_count++] = (Vertex) {
							.pos = h,
							.uv = h,
							.glyph_offset_x = glyph_index % glyphs_per_row,
							.glyph_offset_y = glyph_index / glyphs_per_row,
							.cell_offset_x = command.text.column + k,
							.cell_offset_y = command.text.row
						};
					}
				}
			}
			else if(command.type == DRAW_COMMAND_NUMBER) {
				u32 number = command.number.num;

				u32 digits_in_number = (u32)log10(number) + 1;
				u32 k = 1;
				do {
					u32 glyph_index = 0x30 + (number % 10) - 0x20;
					for(int h = 0; h < 6; ++h) {
						vertex_data[renderer->active_vertex_count++] = (Vertex) {
							.pos = h,
							.uv = h,
							.glyph_offset_x = glyph_index % glyphs_per_row,
							.glyph_offset_y = glyph_index / glyphs_per_row,
							.cell_offset_x = command.number.column + (digits_in_number - k),
							.cell_offset_y = command.number.row
						};
					}

					number /= 10;
					++k;
				} while(number > 0);
			}
		}

		free(draw_list.commands);
	}
}

static void renderer_present(Renderer *renderer) {
	static u32 resource_index = 0;

	VK_CHECK(vkWaitForFences(renderer->logical_device.handle, 1, &renderer->fences[resource_index], VK_TRUE, UINT64_MAX));
	VK_CHECK(vkResetFences(renderer->logical_device.handle, 1, &renderer->fences[resource_index]));

	u32 image_index;
	VkResult result = vkAcquireNextImageKHR(renderer->logical_device.handle, renderer->swapchain.handle, 
		UINT64_MAX, renderer->image_available_semaphores[resource_index], VK_NULL_HANDLE,
		&image_index);
	if(result == VK_ERROR_OUT_OF_DATE_KHR) {
		renderer_resize(renderer);
		return;
	}
	VK_CHECK(result);

	VkCommandBufferBeginInfo command_buffer_begin_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};
	VK_CHECK(vkBeginCommandBuffer(renderer->command_buffers[resource_index], &command_buffer_begin_info));

	if(renderer->framebuffers[resource_index] != VK_NULL_HANDLE) {
		vkDestroyFramebuffer(renderer->logical_device.handle, renderer->framebuffers[resource_index], NULL);
		renderer->framebuffers[resource_index] = VK_NULL_HANDLE;
	}

	VkFramebufferCreateInfo framebuffer_info = {
		.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.renderPass = renderer->render_pass,
		.attachmentCount = 1,
		.pAttachments = &renderer->swapchain.image_views[image_index],
		.width = renderer->swapchain.extent.width,
		.height = renderer->swapchain.extent.height,
		.layers = 1
	};
	VK_CHECK(vkCreateFramebuffer(renderer->logical_device.handle, &framebuffer_info,
		NULL, &renderer->framebuffers[resource_index]));

	VkClearValue clear_values[] = {
		{.color = {.float32 = { 0.15625f, 0.15625f, 0.15625f, 1.0f } } }
	};

	VkRenderPassBeginInfo render_pass_begin_info = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass = renderer->render_pass,
		.framebuffer = renderer->framebuffers[resource_index],
		.renderArea = {
			.extent = {
				.width = renderer->swapchain.extent.width,
				.height = renderer->swapchain.extent.height
			}
		},
		.clearValueCount = ARRAY_LENGTH(clear_values),
		.pClearValues = clear_values
	};
	vkCmdBeginRenderPass(renderer->command_buffers[resource_index],
		&render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdBindPipeline(renderer->command_buffers[resource_index], VK_PIPELINE_BIND_POINT_GRAPHICS,
		renderer->graphics_pipeline.handle);
	vkCmdBindDescriptorSets(renderer->command_buffers[resource_index], VK_PIPELINE_BIND_POINT_GRAPHICS,
		renderer->graphics_pipeline.layout, 0, 1, &renderer->descriptor_set.handle, 0, NULL);

	VkDeviceSize offsets = 0;
	vkCmdBindVertexBuffers(renderer->command_buffers[resource_index], 0, 1, &renderer->vertex_buffer.handle, &offsets);

	GraphicsPushConstants graphics_push_constants = {
		.display_size = {
			(float)renderer->swapchain.extent.width,
			(float)renderer->swapchain.extent.height
		},
		.glyph_atlas_size = GLYPH_ATLAS_SIZE,
		.glyph_width = renderer->glyph_resources.glyph_atlas.metrics.glyph_width,
		.glyph_height = renderer->glyph_resources.glyph_atlas.metrics.glyph_height,
		.cell_width = renderer->glyph_resources.glyph_atlas.metrics.cell_width,
		.cell_height = renderer->glyph_resources.glyph_atlas.metrics.cell_height
	};
	vkCmdPushConstants(renderer->command_buffers[resource_index], renderer->graphics_pipeline.layout,
		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
		sizeof(GraphicsPushConstants), &graphics_push_constants);

	vkCmdDraw(renderer->command_buffers[resource_index], renderer->active_vertex_count, 1, 0, 0);

	vkCmdEndRenderPass(renderer->command_buffers[resource_index]);

	VK_CHECK(vkEndCommandBuffer(renderer->command_buffers[resource_index]));

	VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkSubmitInfo submit_info = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &renderer->image_available_semaphores[resource_index],
		.pWaitDstStageMask = &wait_stage,
		.commandBufferCount = 1,
		.pCommandBuffers = &renderer->command_buffers[resource_index],
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &renderer->render_finished_semaphores[resource_index]
	};

	VK_CHECK(vkQueueSubmit(renderer->logical_device.graphics_queue, 1, &submit_info,
		renderer->fences[resource_index]));

	VkPresentInfoKHR present_info = {
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &renderer->render_finished_semaphores[resource_index],
		.swapchainCount = 1,
		.pSwapchains = &renderer->swapchain.handle,
		.pImageIndices = &image_index,
	};

	result = vkQueuePresentKHR(renderer->logical_device.graphics_queue, &present_info);
	if(!((result == VK_SUCCESS) || (result == VK_SUBOPTIMAL_KHR))) {
		if(result == VK_ERROR_OUT_OF_DATE_KHR) {
			renderer_resize(renderer);
			return;
		}
	}
	VK_CHECK(result);

	resource_index = (resource_index + 1) % MAX_FRAMES_IN_FLIGHT;
}

u32 renderer_get_number_of_lines_on_screen(Renderer *renderer) {
	return (u32)ceil((float)renderer->swapchain.extent.height / 
		renderer->glyph_resources.glyph_atlas.metrics.cell_height);
}

