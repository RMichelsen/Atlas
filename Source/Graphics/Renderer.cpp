#include "PCH.h"
#include "Renderer.h"

#include "Graphics/GlyphRasterizer.h"
#include "Graphics/ShaderBytecode.h"

#ifndef NDEBUG
const char *LAYERS[] = { "VK_LAYER_KHRONOS_validation" };
const char *INSTANCE_EXTENSIONS[] = {
	VK_KHR_SURFACE_EXTENSION_NAME,
	VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
	VK_EXT_DEBUG_UTILS_EXTENSION_NAME
};
#else
const char *INSTANCE_EXTENSIONS[] = {
	VK_KHR_SURFACE_EXTENSION_NAME,
	VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
	VK_EXT_DEBUG_UTILS_EXTENSION_NAME
};
#endif
const char *DEVICE_EXTENSIONS[] = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME
};

#ifndef NDEBUG
VKAPI_ATTR VkBool32 VKAPI_CALL DebugMessengerCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
	VkDebugUtilsMessageTypeFlagsEXT message_type,
	const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
	void *user_data) {
	printf("%s\n", callback_data->pMessage);
	return VK_FALSE;
}

VkDebugUtilsMessengerEXT CreateDebugMessenger(VkInstance instance) {
	VkDebugUtilsMessengerCreateInfoEXT debug_utils_messenger_info = {
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
		.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
						   VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
						   VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
		.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
					   VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
					   VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
		.pfnUserCallback = DebugMessengerCallback
	};

	VkDebugUtilsMessengerEXT debug_messenger;
	VK_CHECK(((PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"))(instance, &debug_utils_messenger_info, nullptr, &debug_messenger));
	return debug_messenger;
}
#endif

// Function from Vulkan spec 1.0.183
uint32_t FindMemoryTypeIndex(PhysicalDevice physical_device, uint32_t memory_type_bits_requirements,
						VkMemoryPropertyFlags required_properties) {
	uint32_t memory_count = physical_device.memory_properties.memoryTypeCount;

	for(uint32_t memory_index = 0; memory_index < memory_count; ++memory_index) {
		VkMemoryPropertyFlags properties = physical_device.memory_properties.memoryTypes[memory_index].propertyFlags;
		bool is_required_memory_type = memory_type_bits_requirements & (1 << memory_index);
		bool has_required_properties = (properties & required_properties) == required_properties;

		if(is_required_memory_type && has_required_properties) {
			return memory_index;
		}
	}

	// Failed to find memory type
	assert(false);
	return UINT32_MAX;
}

VkInstance CreateInstance() {
	VkApplicationInfo application_info = {
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.apiVersion = VK_API_VERSION_1_2
	};

	VkValidationFeatureEnableEXT validation_feature_enable = VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT;
	VkValidationFeaturesEXT validation_features = {
		.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT,
		.enabledValidationFeatureCount = 1,
		.pEnabledValidationFeatures = &validation_feature_enable
	};

	VkInstanceCreateInfo instance_info = {
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pNext = &validation_features,
		.pApplicationInfo = &application_info,
#ifndef NDEBUG
		.enabledLayerCount = _countof(LAYERS),
		.ppEnabledLayerNames = LAYERS,
#endif
		.enabledExtensionCount = _countof(INSTANCE_EXTENSIONS),
		.ppEnabledExtensionNames = INSTANCE_EXTENSIONS
	};

	VkInstance instance;
	VK_CHECK(vkCreateInstance(&instance_info, nullptr, &instance));
	return instance;
}

VkSurfaceKHR CreateSurface(VkInstance instance, HINSTANCE h_instance, HWND hwnd) {
	VkWin32SurfaceCreateInfoKHR win32_surface_info = {
		.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
		.hinstance = h_instance,
		.hwnd = hwnd
	};

	VkSurfaceKHR surface;
	VK_CHECK(vkCreateWin32SurfaceKHR(instance, &win32_surface_info, nullptr, &surface));
	return surface;
}

PhysicalDevice CreatePhysicalDevice(VkInstance instance, VkSurfaceKHR surface) {
	uint32_t device_count = 0;
	vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
	assert(device_count > 0 && "No Vulkan 1.2 capable devices found");

	VkPhysicalDevice *physical_devices = (VkPhysicalDevice *)malloc(device_count * sizeof(VkPhysicalDevice));
	VK_CHECK(vkEnumeratePhysicalDevices(instance, &device_count, physical_devices));

	// Find the first discrete GPU
	VkPhysicalDeviceProperties properties;
	VkPhysicalDevice physical_device = VK_NULL_HANDLE;
	for(uint32_t i = 0; i < device_count; ++i) {
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

	uint32_t queue_family_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);
	assert(queue_family_count > 0);

	VkQueueFamilyProperties *queue_families = (VkQueueFamilyProperties *)malloc(queue_family_count * sizeof(VkQueueFamilyProperties));
	vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_families);

	uint32_t graphics_family_idx = UINT32_MAX;
	uint32_t compute_family_idx = UINT32_MAX;
	for(uint32_t i = 0; i < queue_family_count; ++i) {
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

	return PhysicalDevice {
		.handle = physical_device,
		.properties = device_properties,
		.memory_properties = memory_properties,
		.surface_capabilities = surface_capabilities,
		.graphics_family_idx = graphics_family_idx,
		.compute_family_idx = compute_family_idx
	};
}

LogicalDevice CreateLogicalDevice(PhysicalDevice physical_device) {
	float priority = 1.0f;
	VkDeviceQueueCreateInfo device_queue_infos[] = {
		{
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.queueFamilyIndex = physical_device.graphics_family_idx,
			.queueCount = 1,
			.pQueuePriorities = &priority
		},
		{
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.queueFamilyIndex = physical_device.compute_family_idx,
			.queueCount = 1,
			.pQueuePriorities = &priority
		}
	};

	VkDeviceCreateInfo device_info = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.pNext = nullptr,
		.queueCreateInfoCount =
			physical_device.graphics_family_idx == physical_device.compute_family_idx ?
			1 : _countof(device_queue_infos),
		.pQueueCreateInfos = device_queue_infos,
#ifndef NDEBUG
		.enabledLayerCount = _countof(LAYERS),
		.ppEnabledLayerNames = LAYERS,
#endif
		.enabledExtensionCount = _countof(DEVICE_EXTENSIONS),
		.ppEnabledExtensionNames = DEVICE_EXTENSIONS
	};

	VkDevice device;
	VkQueue graphics_queue;
	VkQueue compute_queue;
	VK_CHECK(vkCreateDevice(physical_device.handle, &device_info, nullptr, &device));
	vkGetDeviceQueue(device, physical_device.graphics_family_idx, 0, &graphics_queue);
	vkGetDeviceQueue(device, physical_device.compute_family_idx, 0, &compute_queue);

	return LogicalDevice {
		.handle = device,
		.graphics_queue = graphics_queue,
		.compute_queue = compute_queue
	};
}

VkCommandPool CreateCommandPool(PhysicalDevice physical_device, LogicalDevice logical_device) {
	VkCommandPoolCreateInfo command_pool_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT |
					VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
		.queueFamilyIndex = physical_device.graphics_family_idx
	};
	VkCommandPool command_pool;
	VK_CHECK(vkCreateCommandPool(logical_device.handle, &command_pool_info, nullptr, &command_pool));
	return command_pool;
}

Swapchain CreateSwapchain(HWND hwnd, VkSurfaceKHR surface, PhysicalDevice physical_device,
						  LogicalDevice logical_device, Swapchain *old_swapchain) {
	VK_CHECK(vkDeviceWaitIdle(logical_device.handle));

	RECT client_rect;
	GetClientRect(hwnd, &client_rect);
	VkExtent2D extent = {
		.width = (uint32_t)client_rect.right,
		.height = (uint32_t)client_rect.bottom
	};

	uint32_t desired_image_count = physical_device.surface_capabilities.minImageCount + 1;
	if(physical_device.surface_capabilities.maxImageCount > 0 &&
	   desired_image_count > physical_device.surface_capabilities.maxImageCount) {
		desired_image_count = physical_device.surface_capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR swapchain_info = {
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = surface,
		.minImageCount = desired_image_count,
		.imageFormat = VK_FORMAT_B8G8R8A8_SRGB,
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
	VK_CHECK(vkCreateSwapchainKHR(logical_device.handle, &swapchain_info, nullptr, &swapchain));

	// Delete old swapchain
	if(old_swapchain) {
		for(int i = 0; i < old_swapchain->image_count; ++i) {
			vkDestroyImageView(logical_device.handle, old_swapchain->image_views[i], nullptr);
		}

		vkDestroySwapchainKHR(logical_device.handle, old_swapchain->handle, nullptr);
		free(old_swapchain->images);
		free(old_swapchain->image_views);
	}

	uint32_t image_count = 0;
	VK_CHECK(vkGetSwapchainImagesKHR(logical_device.handle, swapchain, &image_count, nullptr));

	VkImage *images = (VkImage *)malloc(image_count * sizeof(VkImage));
	assert(images);
	VK_CHECK(vkGetSwapchainImagesKHR(logical_device.handle, swapchain, &image_count, images));

	VkImageView *image_views = (VkImageView *)malloc(image_count * sizeof(VkImageView));
	assert(image_views);
	for(uint32_t i = 0; i < image_count; ++i) {
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
		VK_CHECK(vkCreateImageView(logical_device.handle, &image_view_info, nullptr, &image_views[i]));
	}

	return Swapchain {
		.handle = swapchain,
		.image_count = image_count,
		.format = swapchain_info.imageFormat,
		.present_mode = swapchain_info.presentMode,
		.extent = extent,
		.images = images,
		.image_views = image_views
	};
}

FrameResources CreateFrameResources(LogicalDevice logical_device, VkCommandPool command_pool,
									Swapchain swapchain) {
	FrameResources frame_resources;

	VkSemaphoreCreateInfo semaphore_info = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
	};
	VkFenceCreateInfo fence_info = {
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.flags = VK_FENCE_CREATE_SIGNALED_BIT
	};
	VkCommandBufferAllocateInfo command_buffer_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = command_pool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1
	};

	for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
		VK_CHECK(vkAllocateCommandBuffers(logical_device.handle, &command_buffer_info, &frame_resources.command_buffers[i]));
		VK_CHECK(vkCreateSemaphore(logical_device.handle, &semaphore_info, nullptr, &frame_resources.image_available_semaphores[i]));
		VK_CHECK(vkCreateSemaphore(logical_device.handle, &semaphore_info, nullptr, &frame_resources.render_finished_semaphores[i]));
		VK_CHECK(vkCreateFence(logical_device.handle, &fence_info, nullptr, &frame_resources.fences[i]));
	}

	return frame_resources;
}

VkRenderPass CreateRenderPass(LogicalDevice device, Swapchain swapchain) {
	VkAttachmentDescription attachments[] = {
		{
			.format = swapchain.format,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		}
	};
	VkAttachmentReference color_attachment_reference = {
		.attachment = 0,
		.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	};

	VkSubpassDescription subpass_description = {
		.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
		.colorAttachmentCount = 1,
		.pColorAttachments = &color_attachment_reference
	};

	VkSubpassDependency subpass_dependency = {
		.srcSubpass = VK_SUBPASS_EXTERNAL,
		.dstSubpass = 0,
		.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		.srcAccessMask = 0,
		.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
						 VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
	};

	VkRenderPassCreateInfo render_pass_info = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount = _countof(attachments),
		.pAttachments = attachments,
		.subpassCount = 1,
		.pSubpasses = &subpass_description,
		.dependencyCount = 1,
		.pDependencies = &subpass_dependency
	};

	VkRenderPass render_pass;
	VK_CHECK(vkCreateRenderPass(device.handle, &render_pass_info, nullptr, &render_pass));
	return render_pass;
}

Pipeline CreateRasterizationPipeline(VkInstance instance, LogicalDevice device,
									 Swapchain swapchain, VkRenderPass render_pass) {
	VkPipelineLayoutCreateInfo layout_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO
	};
	VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
	VK_CHECK(vkCreatePipelineLayout(device.handle, &layout_info, nullptr, &pipeline_layout));

	VkShaderModule shader_module = VK_NULL_HANDLE;

	VkPipelineShaderStageCreateInfo shader_stage_infos[] = {
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.pName = "main"
		},
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.pName = "main"
		},
	};

	VkShaderModuleCreateInfo vertex_shader_module_info = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = _countof(VERTEX_SHADER_BYTECODE) * sizeof(uint32_t),
		.pCode = VERTEX_SHADER_BYTECODE
	};
	VkShaderModuleCreateInfo fragment_shader_module_info = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = _countof(FRAGMENT_SHADER_BYTECODE) * sizeof(uint32_t),
		.pCode = FRAGMENT_SHADER_BYTECODE
	};
	vkCreateShaderModule(device.handle, &vertex_shader_module_info, 
						 nullptr, &shader_stage_infos[0].module);
	vkCreateShaderModule(device.handle, &fragment_shader_module_info, 
						 nullptr, &shader_stage_infos[1].module);

	VkVertexInputBindingDescription vertex_input_binding_description = {
		.binding = 0,
		.stride = sizeof(float) * 4,
		.inputRate = VK_VERTEX_INPUT_RATE_VERTEX
	};
	VkVertexInputAttributeDescription vertex_input_attribute_description = {
		.location = 0,
		.binding = 0,
		.format = VK_FORMAT_R32G32B32A32_SFLOAT,
		.offset = 0
	};
	VkPipelineVertexInputStateCreateInfo vertex_input_state_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions = &vertex_input_binding_description,
		.vertexAttributeDescriptionCount = 1,
		.pVertexAttributeDescriptions = &vertex_input_attribute_description
	};

	VkPipelineInputAssemblyStateCreateInfo input_assembly_state_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
	};

	VkViewport viewport = {
		.width = (float)swapchain.extent.width,
		.height = (float)swapchain.extent.height,
		.minDepth = 0.0f,
		.maxDepth = 1.0f
	};
	VkRect2D scissor = {
		.extent = swapchain.extent
	};
	VkPipelineViewportStateCreateInfo viewport_state_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.pViewports = &viewport,
		.scissorCount = 1,
		.pScissors = &scissor
	};

	VkPipelineRasterizationStateCreateInfo rasterization_state_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.cullMode = VK_CULL_MODE_BACK_BIT,
		.frontFace = VK_FRONT_FACE_CLOCKWISE,
		.lineWidth = 1.0f
	};

	VkPipelineMultisampleStateCreateInfo multisample_state_info = {
	   .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
	   .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
	};

	VkPipelineDepthStencilStateCreateInfo depth_stencil_state_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.minDepthBounds = 0.0f,
		.maxDepthBounds = 1.0f
	};

	VkPipelineColorBlendAttachmentState color_blend_attachment_state = {
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
						  VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
	};
	VkPipelineColorBlendStateCreateInfo color_blend_state_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = &color_blend_attachment_state
	};

	VkGraphicsPipelineCreateInfo graphics_pipeline_info = {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.stageCount = _countof(shader_stage_infos),
		.pStages = shader_stage_infos,
		.pVertexInputState = &vertex_input_state_info,
		.pInputAssemblyState = &input_assembly_state_info,
		.pViewportState = &viewport_state_info,
		.pRasterizationState = &rasterization_state_info,
		.pMultisampleState = &multisample_state_info,
		.pDepthStencilState = &depth_stencil_state_info,
		.pColorBlendState = &color_blend_state_info,
		.layout = pipeline_layout,
		.renderPass = render_pass,
		.subpass = 0
	};

	VkPipeline pipeline;
	VK_CHECK(vkCreateGraphicsPipelines(device.handle, VK_NULL_HANDLE, 1, &graphics_pipeline_info, 
									   nullptr, &pipeline));

	vkDestroyShaderModule(device.handle, shader_stage_infos[0].module, nullptr);
	vkDestroyShaderModule(device.handle, shader_stage_infos[1].module, nullptr);

	return Pipeline {
		.handle = pipeline,
		.layout = pipeline_layout
	};
}

VkCommandBuffer StartOneTimeCommandBuffer(LogicalDevice logical_device, VkCommandPool command_pool) {
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

void TransitionGlyphImage(LogicalDevice logical_device, VkCommandBuffer command_buffer, Image image) {
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
						 VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr,
						 0, nullptr, 1, &memory_barrier);

}

void EndOneTimeCommandBuffer(LogicalDevice logical_device, VkCommandBuffer command_buffer,
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
	VK_CHECK(vkCreateFence(logical_device.handle, &fence_info, nullptr, &fence));

	VK_CHECK(vkQueueSubmit(logical_device.graphics_queue, 1, &submit_info, fence));
	VK_CHECK(vkWaitForFences(logical_device.handle, 1, &fence, VK_TRUE, UINT64_MAX));

	vkFreeCommandBuffers(logical_device.handle, command_pool, 1, &command_buffer);
	vkDestroyFence(logical_device.handle, fence, nullptr);
}

GlyphResources CreateGlyphResources(HWND hwnd, VkInstance instance, PhysicalDevice physical_device,
									LogicalDevice logical_device, VkCommandPool command_pool) {
	VkDescriptorPoolSize pool_sizes[] = {
		{
			.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			.descriptorCount = 1
		},
		{
			.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.descriptorCount = 1
		}
	};
	VkDescriptorPoolCreateInfo descriptor_pool_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.maxSets = 1,
		.poolSizeCount = _countof(pool_sizes),
		.pPoolSizes = pool_sizes
	};
	VkDescriptorPool descriptor_pool;
	VK_CHECK(vkCreateDescriptorPool(logical_device.handle, &descriptor_pool_info,
									nullptr, &descriptor_pool));

	VkDescriptorSetLayoutBinding descriptor_set_layout_bindings[2] = {
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
		}
	};
	VkDescriptorSetLayoutCreateInfo descriptor_set_layout_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = _countof(descriptor_set_layout_bindings),
		.pBindings = descriptor_set_layout_bindings
	};
	VkDescriptorSetLayout descriptor_set_layout;
	VK_CHECK(vkCreateDescriptorSetLayout(logical_device.handle, &descriptor_set_layout_info,
										 nullptr, &descriptor_set_layout));

	VkDescriptorSetAllocateInfo descriptor_set_allocate_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = descriptor_pool,
		.descriptorSetCount = 1,
		.pSetLayouts = &descriptor_set_layout
	};
	VkDescriptorSet descriptor_set;
	VK_CHECK(vkAllocateDescriptorSets(logical_device.handle, &descriptor_set_allocate_info, &descriptor_set));

	Image glyph_atlas = VulkanAllocator::CreateImage2D(logical_device.handle, physical_device.memory_properties,
													   1024, 1024, VK_FORMAT_R32_SFLOAT);

	MappedBuffer glyph_buffer = VulkanAllocator::CreateMappedBuffer(logical_device.handle,
																	physical_device.memory_properties,
																	sizeof(GlyphInformation));

	VkDescriptorImageInfo descriptor_image_info = {
		.imageView = glyph_atlas.view,
		.imageLayout = VK_IMAGE_LAYOUT_GENERAL
	};
	VkDescriptorBufferInfo descriptor_buffer_info = {
		.buffer = glyph_buffer.handle,
		.range = VK_WHOLE_SIZE
	};
	VkWriteDescriptorSet write_descriptor_sets[] = {
		{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = descriptor_set,
			.dstBinding = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			.pImageInfo = &descriptor_image_info
		},
		{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = descriptor_set,
			.dstBinding = 1,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.pBufferInfo = &descriptor_buffer_info
		}
	};

	vkUpdateDescriptorSets(logical_device.handle, _countof(write_descriptor_sets), 
						   write_descriptor_sets, 0, nullptr);

	VkPushConstantRange push_constant_range = {
		.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
		.offset = 0,
		.size = sizeof(GlyphPushConstants)
	};
	VkPipelineLayoutCreateInfo layout_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = &descriptor_set_layout,
		.pushConstantRangeCount = 1,
		.pPushConstantRanges = &push_constant_range
	};
	VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
	VK_CHECK(vkCreatePipelineLayout(logical_device.handle, &layout_info, nullptr, &pipeline_layout));

	VkPipelineShaderStageCreateInfo shader_stage_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_COMPUTE_BIT,
		.pName = "main"
	};

	VkShaderModuleCreateInfo shader_module_info = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = _countof(COMPUTE_SHADER_BYTECODE) * sizeof(uint32_t),
		.pCode = COMPUTE_SHADER_BYTECODE
	};
	vkCreateShaderModule(logical_device.handle, &shader_module_info, nullptr, &shader_stage_info.module);

	VkComputePipelineCreateInfo compute_pipeline_info {
		.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		.stage = shader_stage_info,
		.layout = pipeline_layout,
	};
	VkPipeline pipeline;
	vkCreateComputePipelines(logical_device.handle, nullptr, 1, &compute_pipeline_info, nullptr, &pipeline);

	vkDestroyShaderModule(logical_device.handle, shader_stage_info.module, nullptr);

	VkCommandBuffer command_buffer = StartOneTimeCommandBuffer(logical_device, command_pool);

	TransitionGlyphImage(logical_device, command_buffer, glyph_atlas);

	//vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
	//vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout,
	//						0, 1, &descriptor_set, 0, nullptr);
	//RasterizeGlyphs(hwnd, L"Consolas", command_buffer, pipeline_layout, (GlyphInformation *)glyph_buffer.data);

	EndOneTimeCommandBuffer(logical_device, command_buffer, command_pool);

	return GlyphResources {
		.descriptor_pool = descriptor_pool,
		.descriptor_set_layout = descriptor_set_layout,
		.descriptor_set = descriptor_set,
		.glyph_atlas = glyph_atlas,
		.glyph_buffer = glyph_buffer,
		.glyph_generation_pipeline = Pipeline {
			.handle = pipeline,
			.layout = pipeline_layout
		}
	};
}

Renderer RendererInitialize(HINSTANCE hinstance, HWND hwnd) {
	VkInstance instance = CreateInstance();
	VkSurfaceKHR surface = CreateSurface(instance, hinstance, hwnd);

#ifndef NDEBUG
	VkDebugUtilsMessengerEXT debug_messenger = CreateDebugMessenger(instance);
#endif

	PhysicalDevice physical_device = CreatePhysicalDevice(instance, surface);
	LogicalDevice logical_device = CreateLogicalDevice(physical_device);
	Swapchain swapchain = CreateSwapchain(hwnd, surface, physical_device, logical_device, nullptr);
	VkCommandPool command_pool = CreateCommandPool(physical_device, logical_device);
	FrameResources frame_resources = CreateFrameResources(logical_device, command_pool, swapchain);

	VkRenderPass render_pass = CreateRenderPass(logical_device, swapchain);
	Pipeline graphics_pipeline = CreateRasterizationPipeline(instance, logical_device, swapchain, 
															 render_pass);

	GlyphResources glyph_resources = CreateGlyphResources(hwnd, instance, physical_device, logical_device,
														  command_pool);

	return Renderer {
		.hwnd = hwnd,
		.instance = instance,
		.surface = surface,
		.logical_device = logical_device,
		.physical_device = physical_device,
		.swapchain = swapchain,
		.command_pool = command_pool,
		.frame_resources = frame_resources,
		.render_pass = render_pass,
		.graphics_pipeline = graphics_pipeline,
		.glyph_resources = glyph_resources,
#ifndef NDEBUG
		.debug_messenger = debug_messenger
#endif
	};
}

void RendererResize(Renderer *renderer) {
	renderer->swapchain = CreateSwapchain(renderer->hwnd, renderer->surface, renderer->physical_device,
										  renderer->logical_device, &renderer->swapchain);
}

void RendererUpdate(HWND hwnd, Renderer *renderer) {
	static uint32_t resource_index = 0;
	FrameResources *frame_resources = &renderer->frame_resources;

	VK_CHECK(vkWaitForFences(renderer->logical_device.handle, 1, &frame_resources->fences[resource_index], VK_TRUE, UINT64_MAX));
	VK_CHECK(vkResetFences(renderer->logical_device.handle, 1, &frame_resources->fences[resource_index]));

	uint32_t image_index;
	VkResult result = vkAcquireNextImageKHR(renderer->logical_device.handle, renderer->swapchain.handle, UINT64_MAX,
											frame_resources->image_available_semaphores[resource_index], VK_NULL_HANDLE,
											&image_index);
	if(result == VK_ERROR_OUT_OF_DATE_KHR) {
		RendererResize(renderer);
		return;
	}
	VK_CHECK(result);

	VkCommandBufferBeginInfo command_buffer_begin_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};
	VK_CHECK(vkBeginCommandBuffer(frame_resources->command_buffers[resource_index], &command_buffer_begin_info));

	static VkFramebuffer framebuffers[MAX_FRAMES_IN_FLIGHT] = { VK_NULL_HANDLE };
	if(framebuffers[resource_index] != VK_NULL_HANDLE) {
		vkDestroyFramebuffer(renderer->logical_device.handle, framebuffers[resource_index], nullptr);
		framebuffers[resource_index] = VK_NULL_HANDLE;
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
								 nullptr, &framebuffers[resource_index]));

	VkClearValue clear_values[] = {
		{ .color = { .float32 = { 0.15f, 0.15f, 0.3f, 1.0f } } }
	};

	VkRenderPassBeginInfo render_pass_begin_info = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass = renderer->render_pass,
		.framebuffer = framebuffers[resource_index],
		.renderArea = {
			.extent = {
				.width = renderer->swapchain.extent.width,
				.height = renderer->swapchain.extent.height
			}
		},
		.clearValueCount = _countof(clear_values),
		.pClearValues = clear_values
	};
	vkCmdBeginRenderPass(frame_resources->command_buffers[resource_index], 
						 &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdEndRenderPass(frame_resources->command_buffers[resource_index]);

	vkCmdBindPipeline(frame_resources->command_buffers[resource_index], VK_PIPELINE_BIND_POINT_COMPUTE, renderer->glyph_resources.glyph_generation_pipeline.handle);
	vkCmdBindDescriptorSets(frame_resources->command_buffers[resource_index], VK_PIPELINE_BIND_POINT_COMPUTE, renderer->glyph_resources.glyph_generation_pipeline.layout,
							0, 1, &renderer->glyph_resources.descriptor_set, 0, nullptr);
	RasterizeGlyphs(hwnd, L"Consolas", frame_resources->command_buffers[resource_index], renderer->glyph_resources.glyph_generation_pipeline.layout, 
					(GlyphInformation *)renderer->glyph_resources.glyph_buffer.data);

	VK_CHECK(vkEndCommandBuffer(frame_resources->command_buffers[resource_index]));

	VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkSubmitInfo submit_info = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &frame_resources->image_available_semaphores[resource_index],
		.pWaitDstStageMask = &wait_stage,
		.commandBufferCount = 1,
		.pCommandBuffers = &frame_resources->command_buffers[resource_index],
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &frame_resources->render_finished_semaphores[resource_index]
	};

	VK_CHECK(vkQueueSubmit(renderer->logical_device.graphics_queue, 1, &submit_info,
						   frame_resources->fences[resource_index]));

	VkPresentInfoKHR present_info = {
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &frame_resources->render_finished_semaphores[resource_index],
		.swapchainCount = 1,
		.pSwapchains = &renderer->swapchain.handle,
		.pImageIndices = &image_index,
	};

	result = vkQueuePresentKHR(renderer->logical_device.graphics_queue, &present_info);
	if(!((result == VK_SUCCESS) || (result == VK_SUBOPTIMAL_KHR))) {
		if(result == VK_ERROR_OUT_OF_DATE_KHR) {
			RendererResize(renderer);
			return;
		}
	}
	VK_CHECK(result);

	resource_index = (resource_index + 1) % MAX_FRAMES_IN_FLIGHT;
}

void RendererDestroy(Renderer *renderer) {
	VK_CHECK(vkDeviceWaitIdle(renderer->logical_device.handle));

	for(uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
		vkDestroySemaphore(renderer->logical_device.handle, renderer->frame_resources.image_available_semaphores[i], nullptr);
		vkDestroySemaphore(renderer->logical_device.handle, renderer->frame_resources.render_finished_semaphores[i], nullptr);
		vkDestroyFence(renderer->logical_device.handle, renderer->frame_resources.fences[i], nullptr);
	}

	for(uint32_t i = 0; i < renderer->swapchain.image_count; ++i) {
		vkDestroyImageView(renderer->logical_device.handle, renderer->swapchain.image_views[i], nullptr);
	}
	vkDestroySwapchainKHR(renderer->logical_device.handle, renderer->swapchain.handle, nullptr);
	free(renderer->swapchain.images);
	free(renderer->swapchain.image_views);

	vkDestroyCommandPool(renderer->logical_device.handle, renderer->command_pool, nullptr);
	vkDestroyDevice(renderer->logical_device.handle, nullptr);

#ifndef NDEBUG
	((PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(renderer->instance, "vkDestroyDebugUtilsMessengerEXT"))(renderer->instance, renderer->debug_messenger, nullptr);
#endif
	vkDestroySurfaceKHR(renderer->instance, renderer->surface, nullptr);
	vkDestroyInstance(renderer->instance, nullptr);
}

