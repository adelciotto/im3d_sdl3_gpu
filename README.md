# SDL3 GPU SHADERS CROSS COMPILE EXAMPLE

This repository contains example code for cross compiling shaders at both build-time and run-time using SDL3 and SDL3_shadercross.
In debug mode the shaders are live reloaded on any changes.

[Demo Video](https://anthonydelciotto.com/media/sdl3_gpu_shaders_cross_compile.mp4)

## Build

### Windows

Building the code can be done in a terminal which is equipped with the ability to call MSVC command line.

This is generally done by calling `vcvarsall.bat x64`, which is included in the Microsoft C/C++ Build Tools. This script is automatically called by the `x64 Native Tools Command Prompt for VS <year>` variant of the vanilla `cmd.exe`. If you've installed the build tools, this command prompt may be easily located by searching for `Native` from the Windows Start Menu search. The same script is also called automatically in the `Developer Command Prompt for VS <year>` profile of the terminal app.

You can ensure that the MSVC compiler is accessible from your command line by running:

```
cl
```

If everything is set up correctly, you should have output very similar to the following:

```
Microsoft (R) C/C++ Optimizing Compiler Version 19.44.35216 for x64
Copyright (C) Microsoft Corporation.  All rights reserved.

usage: cl [ option... ] filename... [ /link linkoption... ]
```

Now just run `build.bat` script.


```
build release
```


You should see the following output:

```
[release mode]
Compiling shaders...
Compiling source files...
sdl3_gpu_shaders_cross_compile.cpp
imgui.cpp
imgui_demo.cpp
imgui_draw.cpp
imgui_impl_sdl3.cpp
imgui_impl_sdlgpu3.cpp
imgui_tables.cpp
imgui_widgets.cpp
Generating Code...
Done!
```


If everything worked correctly, there will be a `build_release` folder in the root level of the project, and it will contain a freshly-built `sdl3_gpu_shaders_cross_compile.exe`.

This `sdl3_gpu_shaders_cross_compile.exe` has been built in release mode. If you'd like to modify the source, debug it and live-reload shaders, you can just run `build.bat` with no arguments for a debug build. The executable will be in a `build_debug` folder.

### Linux

Building the code can be done in a terminal. The build script will automatically download and build SDL3.

Because we have to first build SDL3 on Linux, please ensure you install it's dependencies by following the instructions for your Linux distribution here: https://github.com/libsdl-org/SDL/blob/main/docs/README-linux.md

Now just run the `build_linux.sh` script.

```
./build_linux.sh release
```


On the first run SDL3 will be downloaded and compiled to `extern/SDL3/linux`. Once that has completed you should see the following output:

```
[release mode]
Compiling shaders...
Compiling source files...
Done!
```


If everything worked correctly, there will be a `build_release` folder in the root level of the project, and it will contain a freshly-built `sdl3_gpu_shaders_cross_compile` executable.

This `sdl3_gpu_shaders_cross_compile` has been built in release mode. If you'd like to modify the source, debug it and live-reload shaders, you can just run `build_linux.sh` with no arguments for a debug build. The executable will be in a `build_debug` folder.

## Dependencies / Tools

* [HandmadeMath](https://github.com/HandmadeMath/HandmadeMath)
* [SDL3](https://wiki.libsdl.org/SDL3/FrontPage)
* [ImGui](https://github.com/ocornut/imgui)
* [SDL_shadercross](https://github.com/libsdl-org/SDL_shadercross) for compiling shaders.
