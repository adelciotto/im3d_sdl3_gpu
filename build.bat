@echo off
setlocal enabledelayedexpansion
cd /D "%~dp0"
set root_dir=%CD:\=/%

:: --- Unpack Arguments -------------------------------------------------------
for %%a in (%*) do set "%%~a=1"
if not "%release%"=="1" set debug=1
if "%debug%"=="1" set release=0 && echo [debug mode]
if "%release%"=="1" set debug=0 && echo [release mode]

:: --- Unpack Command Line Build Arguments ------------------------------------
:: None for now...

:: --- Compile/Link Definitions -----------------------------------------------
set cl_common=/nologo /EHsc /std:c++17 /I..\src
set cl_debug_common=call cl /MDd /Zi /Od /DBUILD_DEBUG %cl_common%
set cl_release_common=call cl /MD /O2 %cl_common%
if "%debug%"=="1" set cl_compile=%cl_debug_common%
if "%release%"=="1" set cl_compile=%cl_release_common%

:: --- Example Compile/Link Definitions --------------------------------------
set cl_example_includes=/I..\extern\SDL3\win\include /I..\extern\HandmadeMath /I..\extern\imgui /I..\extern\im3d
set cl_example_debug=%cl_debug_common% /I..\extern\SDL3_shadercross\win\include %cl_example_includes%
set cl_example_release=%cl_release_common% %cl_example_includes%
set cl_example_link=/link ..\extern\SDL3\win\lib\x64\SDL3.lib shell32.lib /subsystem:console
if "%debug%"=="1" set cl_example_compile=%cl_example_debug%
if "%release%"=="1" set cl_example_compile=%cl_example_release%

:: --- Shader Compile Definitions ---------------------------------------------
set shadercross=call ..\extern\SDL3_shadercross\win\bin\shadercross.exe
set shadercross_vertex=%shadercross% -t vertex -DVERTEX_SHADER
set shadercross_fragment=%shadercross% -t fragment -DFRAGMENT_SHADER

:: --- Prep Directories -------------------------------------------------------
set build_dir_debug=build_debug
set build_dir_release=build_release
if "%debug%"=="1" set build_dir=%build_dir_debug%
if "%release%"=="1" set build_dir=%build_dir_release%
if not exist %build_dir% mkdir %build_dir%
if not exist %build_dir%\.shaders mkdir %build_dir%\.shaders

:: --- Build Everything -------------------------------------------------------
pushd %build_dir%

if "%shaders%"=="1" (
echo Compiling shaders...
%shadercross_vertex% -DPRIMITIVE_KIND_POINTS ..\src\im3d_sdl3_gpu.hlsl -o .shaders\im3d_points.vert.dxil || exit /b 1
%shadercross_fragment% -DPRIMITIVE_KIND_POINTS ..\src\im3d_sdl3_gpu.hlsl -o .shaders\im3d_points.frag.dxil || exit /b 1
%shadercross_vertex% -DPRIMITIVE_KIND_LINES ..\src\im3d_sdl3_gpu.hlsl -o .shaders\im3d_lines.vert.dxil || exit /b 1
%shadercross_fragment% -DPRIMITIVE_KIND_LINES ..\src\im3d_sdl3_gpu.hlsl -o .shaders\im3d_lines.frag.dxil || exit /b 1
%shadercross_vertex% -DPRIMITIVE_KIND_TRIANGLES ..\src\im3d_sdl3_gpu.hlsl -o .shaders\im3d_triangles.vert.dxil || exit /b 1
%shadercross_fragment% -DPRIMITIVE_KIND_TRIANGLES ..\src\im3d_sdl3_gpu.hlsl -o .shaders\im3d_triangles.frag.dxil || exit /b 1

%shadercross_vertex% -DPRIMITIVE_KIND_POINTS ..\src\im3d_sdl3_gpu.hlsl -o .shaders\im3d_points.vert.spv || exit /b 1
%shadercross_fragment% -DPRIMITIVE_KIND_POINTS ..\src\im3d_sdl3_gpu.hlsl -o .shaders\im3d_points.frag.spv || exit /b 1
%shadercross_vertex% -DPRIMITIVE_KIND_LINES ..\src\im3d_sdl3_gpu.hlsl -o .shaders\im3d_lines.vert.spv || exit /b 1
%shadercross_fragment% -DPRIMITIVE_KIND_LINES ..\src\im3d_sdl3_gpu.hlsl -o .shaders\im3d_lines.frag.spv || exit /b 1
%shadercross_vertex% -DPRIMITIVE_KIND_TRIANGLES ..\src\im3d_sdl3_gpu.hlsl -o .shaders\im3d_triangles.vert.spv || exit /b 1
%shadercross_fragment% -DPRIMITIVE_KIND_TRIANGLES ..\src\im3d_sdl3_gpu.hlsl -o .shaders\im3d_triangles.frag.spv || exit /b 1

%shadercross_vertex% -DPRIMITIVE_KIND_POINTS ..\src\im3d_sdl3_gpu.hlsl -o .shaders\im3d_points.vert.msl || exit /b 1
%shadercross_fragment% -DPRIMITIVE_KIND_POINTS ..\src\im3d_sdl3_gpu.hlsl -o .shaders\im3d_points.frag.msl || exit /b 1
%shadercross_vertex% -DPRIMITIVE_KIND_LINES ..\src\im3d_sdl3_gpu.hlsl -o .shaders\im3d_lines.vert.msl || exit /b 1
%shadercross_fragment% -DPRIMITIVE_KIND_LINES ..\src\im3d_sdl3_gpu.hlsl -o .shaders\im3d_lines.frag.msl || exit /b 1
%shadercross_vertex% -DPRIMITIVE_KIND_TRIANGLES ..\src\im3d_sdl3_gpu.hlsl -o .shaders\im3d_triangles.vert.msl || exit /b 1
%shadercross_fragment% -DPRIMITIVE_KIND_TRIANGLES ..\src\im3d_sdl3_gpu.hlsl -o .shaders\im3d_triangles.frag.msl || exit /b 1

echo Compiling shaders_to_c_arrays...
%cl_compile% ..\src\shaders_to_c_arrays.cpp -DOUT_DIR=\"%root_dir%/src\" /link /out:shaders_to_c_arrays.exe || exit /b 1

echo Exporting shaders to C arrays...
shaders_to_c_arrays.exe || exit /b 1
)

echo Compiling example...
%cl_example_compile% ..\src\main.cpp ^
                     ..\src\util.cpp ^
                     ..\src\im3d_sdl3_gpu.cpp ^
                     ..\extern\im3d\im3d.cpp ^
                     ..\extern\imgui\imgui.cpp ^
                     ..\extern\imgui\imgui_demo.cpp ^
                     ..\extern\imgui\imgui_draw.cpp ^
                     ..\extern\imgui\imgui_impl_sdl3.cpp ^
                     ..\extern\imgui\imgui_impl_sdlgpu3.cpp ^
                     ..\extern\imgui\imgui_tables.cpp ^
                     ..\extern\imgui\imgui_widgets.cpp ^
                     %cl_example_link% /out:im3d_sdl3_gpu_example.exe || exit /b 1

popd

:: --- Copy DLL's -------------------------------------------------------------
if not exist %build_dir%\SDL3.dll copy extern\SDL3\win\lib\x64\SDL3.dll %build_dir% >nul

echo Done^^!
