#pragma once

enum ShaderType {
	SHADER_TYPE_COMPUTE,
	SHADER_TYPE_VERTEX,
	SHADER_TYPE_FRAGMENT
};

namespace ShaderCompiler {
	VkShaderModule CompileShader(VkDevice device, ShaderType shader_type, const char *shader_source);
}
