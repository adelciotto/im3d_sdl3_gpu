#!/bin/bash
set -e
cd "$(dirname "${BASH_SOURCE[0]}")"
source_dir="$(pwd)"

# --- Build SDL3 If Needed ---------------------------------------------------
sdl3_version="3.2.28"
sdl3_url="https://github.com/libsdl-org/SDL/releases/download/release-${sdl3_version}/SDL3-${sdl3_version}.tar.gz"
sdl3_install_dir="${source_dir}/extern/SDL3/linux"
sdl3_build_dir="${source_dir}/tmp/sdl3_build"

if [ ! -d "$sdl3_install_dir" ]; then
  echo "Building SDL3 ${sdl3_version}..."
  mkdir -p "$sdl3_build_dir" "$sdl3_install_dir"
  cd "$sdl3_build_dir"
  wget -O SDL3.tar.gz "$sdl3_url"
  tar -xzf SDL3.tar.gz
  cd "SDL3-${sdl3_version}"
  mkdir -p build
  cd build
  cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$sdl3_install_dir" \
    -DBUILD_SHARED_LIBS=ON
  cmake --build . --config Release --parallel $(nproc)
  cmake --install .
  cd "$source_dir"
  rm -rf tmp
  echo "SDL3 installed to ${sdl3_install_dir}"
else
  echo "SDL3 already present at ${sdl3_install_dir}"
fi

cd "$source_dir"

# --- Unpack Arguments -------------------------------------------------------
debug=0
release=0
shaders=0
for arg in "$@"; do
  if [ "$arg" == "debug" ]; then debug=1; fi
  if [ "$arg" == "release" ]; then release=1; fi
  if [ "$arg" == "shaders" ]; then shaders=1; fi
done
if [ $release -ne 1 ]; then debug=1; fi
if [ $debug -eq 1 ]; then release=0 && echo "[debug mode]"; fi
if [ $release -eq 1 ]; then debug=0 && echo "[release mode]"; fi

# --- Compile/Link Definitions -----------------------------------------------
cc_common="-std=c++17 -I../src"
cc_debug_common="g++ -g -O0 -DBUILD_DEBUG ${cc_common}"
cc_release_common="g++ -O2 ${cc_common}"
if [ $debug -eq 1 ]; then cc_compile="$cc_debug_common"; fi
if [ $release -eq 1 ]; then cc_compile="$cc_release_common"; fi

# --- Example Compile/Link Definitions ---------------------------------------
cc_example_includes="-I../extern/HandmadeMath -I../extern/SDL3/linux/include -I../extern/imgui -I../extern/im3d"
cc_example_debug="${cc_debug_common} -DRESOURCES_PATH=\"${source_dir}/\" -I../extern/SDL3_shadercross/linux/include ${cc_example_includes}"
cc_example_release="${cc_release_common} ${cc_example_includes}"
cc_example_link_common="-L../extern/SDL3/linux/lib -lSDL3 -Wl,-rpath,\$ORIGIN"
cc_example_link_debug="-L../extern/SDL3_shadercross/linux/lib -lSDL3_shadercross ${cc_example_link_common}"
cc_example_link_release="${cc_example_link_common}"
if [ $debug -eq 1 ]; then cc_example_compile="$cc_example_debug"; fi
if [ $release -eq 1 ]; then cc_example_compile="$cc_example_release"; fi
if [ $debug -eq 1 ]; then cc_example_link="$cc_example_link_debug"; fi
if [ $release -eq 1 ]; then cc_example_link="$cc_example_link_release"; fi

# --- Shader Compile Definitions ---------------------------------------------
shadercross="../extern/SDL3_shadercross/linux/bin/shadercross"
shadercross_vertex="$shadercross -t vertex -DVERTEX_SHADER"
shadercross_fragment="$shadercross -t fragment -DFRAGMENT_SHADER"

# --- Prep Directories -------------------------------------------------------
build_dir_debug="build_debug"
build_dir_release="build_release"
if [ $debug -eq 1 ]; then build_dir="$build_dir_debug"; fi
if [ $release -eq 1 ]; then build_dir="$build_dir_release"; fi
mkdir -p "$build_dir"
mkdir -p "$build_dir/.shaders"

# --- Build Everything -------------------------------------------------------
pushd "$build_dir" >/dev/null

if [ $shaders -eq 1 ]; then
  echo "Compiling shaders..."
  $shadercross_vertex -DPRIMITIVE_KIND_POINTS ../src/im3d.hlsl -o .shaders/im3d_points.vert.dxil || exit 1
  $shadercross_fragment -DPRIMITIVE_KIND_POINTS ../src/im3d.hlsl -o .shaders/im3d_points.frag.dxil || exit 1
  $shadercross_vertex -DPRIMITIVE_KIND_LINES ../src/im3d.hlsl -o .shaders/im3d_lines.vert.dxil || exit 1
  $shadercross_fragment -DPRIMITIVE_KIND_LINES ../src/im3d.hlsl -o .shaders/im3d_lines.frag.dxil || exit 1
  $shadercross_vertex -DPRIMITIVE_KIND_TRIANGLES ../src/im3d.hlsl -o .shaders/im3d_triangles.vert.dxil || exit 1
  $shadercross_fragment -DPRIMITIVE_KIND_TRIANGLES ../src/im3d.hlsl -o .shaders/im3d_triangles.frag.dxil || exit 1

  $shadercross_vertex -DPRIMITIVE_KIND_POINTS ../src/im3d.hlsl -o .shaders/im3d_points.vert.spv || exit 1
  $shadercross_fragment -DPRIMITIVE_KIND_POINTS ../src/im3d.hlsl -o .shaders/im3d_points.frag.spv || exit 1
  $shadercross_vertex -DPRIMITIVE_KIND_LINES ../src/im3d.hlsl -o .shaders/im3d_lines.vert.spv || exit 1
  $shadercross_fragment -DPRIMITIVE_KIND_LINES ../src/im3d.hlsl -o .shaders/im3d_lines.frag.spv || exit 1
  $shadercross_vertex -DPRIMITIVE_KIND_TRIANGLES ../src/im3d.hlsl -o .shaders/im3d_triangles.vert.spv || exit 1
  $shadercross_fragment -DPRIMITIVE_KIND_TRIANGLES ../src/im3d.hlsl -o .shaders/im3d_triangles.frag.spv || exit 1

  $shadercross_vertex -DPRIMITIVE_KIND_POINTS ../src/im3d.hlsl -o .shaders/im3d_points.vert.msl || exit 1
  $shadercross_fragment -DPRIMITIVE_KIND_POINTS ../src/im3d.hlsl -o .shaders/im3d_points.frag.msl || exit 1
  $shadercross_vertex -DPRIMITIVE_KIND_LINES ../src/im3d.hlsl -o .shaders/im3d_lines.vert.msl || exit 1
  $shadercross_fragment -DPRIMITIVE_KIND_LINES ../src/im3d.hlsl -o .shaders/im3d_lines.frag.msl || exit 1
  $shadercross_vertex -DPRIMITIVE_KIND_TRIANGLES ../src/im3d.hlsl -o .shaders/im3d_triangles.vert.msl || exit 1
  $shadercross_fragment -DPRIMITIVE_KIND_TRIANGLES ../src/im3d.hlsl -o .shaders/im3d_triangles.frag.msl || exit 1

  echo "Compiling shaders_to_c_arrays..."
  $cc_compile ../src/shaders_to_c_arrays.cpp -DOUT_DIR="\"${source_dir}/src\"" -o shaders_to_c_arrays || exit 1

  echo "Exporting shaders to C arrays..."
  ./shaders_to_c_arrays || exit 1
fi

echo "Compiling example..."
$cc_example_compile ../src/example/main.cpp \
  ../src/im3d_sdl3_gpu.cpp \
  ../extern/im3d/im3d.cpp \
  ../extern/imgui/imgui.cpp \
  ../extern/imgui/imgui_demo.cpp \
  ../extern/imgui/imgui_draw.cpp \
  ../extern/imgui/imgui_impl_sdl3.cpp \
  ../extern/imgui/imgui_impl_sdlgpu3.cpp \
  ../extern/imgui/imgui_tables.cpp \
  ../extern/imgui/imgui_widgets.cpp \
  $cc_example_link -o im3d_sdl3_gpu_example || exit 1

popd >/dev/null

# --- Copy .so's -------------------------------------------------------------
if [ ! -f "$build_dir/libSDL3.so.0" ]; then
  cp -t "$build_dir/" extern/SDL3/linux/lib/libSDL3.so.0* 2>/dev/null || true
fi

echo "Done!"
