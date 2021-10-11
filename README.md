# Atlas
Atlas is a font renderer written in Vulkan and C. It may later become a text editor.

# Font rendering
- The [FreeType] library is used to provide hinted outlines that are rasterized using standard scanline rasterization.
- Coverage-based anti-aliasing and subpixel anti-aliasing is implemented. The approach is based loosely on [this](https://superluminal.eu/16xaa-font-rendering-using-coverage-masks-part-i/) excellent blog series.
