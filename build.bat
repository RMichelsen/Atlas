@echo off
setlocal
call "C:/Program Files (x86)/Microsoft Visual Studio/2022/Preview/VC/Auxiliary/Build/vcvarsall.bat" x64

set RootDirectory=%cd%
set RootDirectory=%RootDirectory:\=/%
set VULKAN_SDK=%VULKAN_SDK:\=/%

if NOT EXIST build mkdir build
pushd build

set FTIncludeDirectories= ^
  /I"../dependencies/freetype/include/"
set FTPreprocessorFlags= ^
    /DFT2_BUILD_LIBRARY
set FTCompilerFlags=/O2 /nologo /TC /c /MP%NUMBER_OF_PROCESSORS%
set FTSourceFiles= ^
    "%RootDirectory%/dependencies/freetype/src/autofit/autofit.c" ^
    "%RootDirectory%/dependencies/freetype/src/base/ftbase.c" ^
    "%RootDirectory%/dependencies/freetype/src/base/ftbbox.c" ^
    "%RootDirectory%/dependencies/freetype/src/base/ftbitmap.c" ^
    "%RootDirectory%/dependencies/freetype/src/base/ftdebug.c" ^
    "%RootDirectory%/dependencies/freetype/src/base/ftglyph.c" ^
    "%RootDirectory%/dependencies/freetype/src/base/ftinit.c" ^
    "%RootDirectory%/dependencies/freetype/src/base/ftsystem.c" ^
    "%RootDirectory%/dependencies/freetype/src/gzip/ftgzip.c" ^
    "%RootDirectory%/dependencies/freetype/src/psnames/psnames.c" ^
    "%RootDirectory%/dependencies/freetype/src/sfnt/sfnt.c" ^
    "%RootDirectory%/dependencies/freetype/src/truetype/truetype.c"

set IncludeDirectories= ^
  /I"%VULKAN_SDK%/Include/" ^
  /I"%RootDirectory%/dependencies/freetype/include/"
set PreprocessorFlags= ^
    /DVK_USE_PLATFORM_WIN32_KHR ^
    /D_CRT_SECURE_NO_WARNINGS
set CompilerFlags=/Od /nologo /TC /Zo /Z7 /FC /MP%NUMBER_OF_PROCESSORS%
set SourceFiles= ^
    "%RootDirectory%/src/main.c"
set LinkDirectories= ^
  /LIBPATH:"%VULKAN_SDK%/Lib/"
set Libraries= ^
  user32.lib ^
  gdi32.lib ^
  vulkan-1.lib ^
  freetype.lib

if NOT EXIST freetype.lib (
  cl %FTSourceFiles% %FTCompilerFlags% %FTPreprocessorFlags% %FTIncludeDirectories%
  link /lib autofit.obj ftbase.obj ftbbox.obj ftbitmap.obj ftdebug.obj ftglyph.obj ^
  ftinit.obj ftsystem.obj ftgzip.obj psnames.obj sfnt.obj truetype.obj /OUT:freetype.lib
  del /Q /F "*.obj"
)

set Shaders= ^
  "%RootDirectory%/src/shaders/fragment.frag" ^
  "%RootDirectory%/src/shaders/vertex.vert" ^
  "%RootDirectory%/src/shaders/write_texture_atlas.comp"

%VULKAN_SDK%\Bin\glslangValidator.exe "%RootDirectory%/src/shaders/fragment.frag" ^
  -g -V --target-env vulkan1.1 -o fragment.frag.spv
%VULKAN_SDK%\Bin\glslangValidator.exe "%RootDirectory%/src/shaders/vertex.vert" ^
  -g -V --target-env vulkan1.1 -o vertex.vert.spv
%VULKAN_SDK%\Bin\glslangValidator.exe "%RootDirectory%/src/shaders/write_texture_atlas.comp" ^
  -g -V --target-env vulkan1.1 -o write_texture_atlas.comp.spv

del *.pdb 1> NUL 2> NUL 
cl %SourceFiles% %CompilerFlags% %PreprocessorFlags% %IncludeDirectories% ^
   /link %LinkDirectories% %Libraries% /INCREMENTAL:no /OUT:Atlas.exe

popd

