#pragma once

#include <assert.h>
#include <math.h>
#include <stdio.h>

#include <Windows.h>

#include <vulkan/vulkan.h>

#define VK_CHECK(x) if((x) != VK_SUCCESS) { 			\
	assert(FALSE); 										\
	printf("Vulkan error: %s:%i", __FILE__, __LINE__); 	\
}

