#pragma once

struct Line;
void RasterizeGlyphs(HWND hwnd, const wchar_t *font_name, VkCommandBuffer command_buffer,
					 VkPipelineLayout pipeline_layout,
					 Line *glyph_lines_gpu_mapped);

