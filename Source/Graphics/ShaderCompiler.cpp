#include "PCH.h"
#include "ShaderCompiler.h"

#define WIN_CHECK(x) if(FAILED(x)) { 					\
	assert(FALSE); 										\
	printf("HRESULT error: %s:%i", __FILE__, __LINE__); \
}

const wchar_t *GetShaderTargetString(ShaderType shader_type) {
	switch(shader_type) {
	case SHADER_TYPE_COMPUTE:
		return L"-T cs_6_5";
	case SHADER_TYPE_VERTEX:
		return L"-T vs_6_5";
	case SHADER_TYPE_FRAGMENT:
		return L"-T ps_6_5";
	}
	return L"";
}

const wchar_t *GetShaderTargetEntryPoint(ShaderType shader_type) {
	switch(shader_type) {
	case SHADER_TYPE_COMPUTE:
		return L"-E CSMain";
	case SHADER_TYPE_VERTEX:
		return L"-E VSMain";
	case SHADER_TYPE_FRAGMENT:
		return L"-E PSMain";
	}
	return L"";
}

namespace ShaderCompiler {
VkShaderModule CompileShader(VkDevice device, ShaderType shader_type, const char *shader_source) {
	IDxcCompiler3 *dxc_compiler;
	WIN_CHECK(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxc_compiler)));

	IDxcLibrary *dxc_library;
	WIN_CHECK(DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&dxc_library)));

	IDxcIncludeHandler *dxc_include_handler;
	WIN_CHECK(dxc_library->CreateIncludeHandler(&dxc_include_handler));

	DxcBuffer source = {
		.Ptr = shader_source,
		.Size = strlen(shader_source),
		.Encoding = 0
	};

	// Compile the shader
	const wchar_t *args[] = { 
		GetShaderTargetString(shader_type),
		GetShaderTargetEntryPoint(shader_type),
		L"-spirv", 
		L"-fspv-target-env=vulkan1.2" 
	};
	IDxcOperationResult *result;
	dxc_compiler->Compile(
		&source,
		args,
		_countof(args),
		dxc_include_handler,
		IID_PPV_ARGS(&result)
	);


	HRESULT hr;
	result->GetStatus(&hr);
	if(FAILED(hr)) {
		IDxcBlobEncoding *error;
		WIN_CHECK(result->GetErrorBuffer(&error));

		// Convert error blob to a string
		char *message = (char *)malloc(error->GetBufferSize() + 1);
		memcpy(message, error->GetBufferPointer(), error->GetBufferSize());
		message[error->GetBufferSize()] = '\0';

		printf("%s\n", message);
		return VK_NULL_HANDLE;
	}

	IDxcBlob *blob = NULL;
	WIN_CHECK(result->GetResult(&blob));

	uint64_t buffer_size = blob->GetBufferSize();
	VkShaderModuleCreateInfo shader_module_info = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = buffer_size,
		.pCode = (uint32_t *)malloc(buffer_size)
	};
	memcpy((uint32_t *)shader_module_info.pCode, blob->GetBufferPointer(), shader_module_info.codeSize);

	// Cleanup
	dxc_compiler->Release();
	dxc_library->Release();
	dxc_include_handler->Release();
	result->Release();
	blob->Release();

	VkShaderModule shader_module = VK_NULL_HANDLE;
	vkCreateShaderModule(device, &shader_module_info, NULL, &shader_module);

	free((void *)shader_module_info.pCode);
	return shader_module;
}
}
