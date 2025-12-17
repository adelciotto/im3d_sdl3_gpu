#pragma once

#include <HandmadeMath.h>
#include <SDL3/SDL.h>

// Deprecated behavior.
#define IM3D_USE_DEPRECATED_DRAW_CONE 1 // Enable deprecated DrawCone() and DrawConeFilled() API (deprecated v1.18).

// User-defined assertion handler (default is cassert assert()).
#define IM3D_ASSERT(e) SDL_assert(e)

// User-defined malloc/free. Define both or neither (default is cstdlib malloc()/free()).
#define IM3D_MALLOC(size) SDL_malloc(size)
#define IM3D_FREE(ptr) SDL_free(ptr)

// User-defined API declaration (e.g. __declspec(dllexport)).
//#define IM3D_API

// Use a thread-local context pointer.
//#define IM3D_THREAD_LOCAL_CONTEXT_PTR 1

// Use row-major internal matrix layout.
//#define IM3D_MATRIX_ROW_MAJOR 1

// Force vertex data alignment (default is 4 bytes).
#define IM3D_VERTEX_ALIGNMENT 16

// Enable internal culling for primitives (everything drawn between Begin*()/End()). The application must set a culling frustum via AppData.
#define IM3D_CULL_PRIMITIVES 1

// Enable internal culling for gizmos. The application must set a culling frustum via AppData.
#define IM3D_CULL_GIZMOS 1

// Set a layer ID for all gizmos to use internally.
//#define IM3D_GIZMO_LAYER_ID 0xD4A1B5

// Conversion to/from application math types.
#define IM3D_VEC2_APP \
	Vec2(HMM_Vec2 _v)        { x = _v.X; y = _v.Y; } \
	operator HMM_Vec2() const { return HMM_V2(x, y); }
#define IM3D_VEC3_APP \
	Vec3(HMM_Vec3 _v)        { x = _v.X; y = _v.Y; z = _v.Z; } \
	operator HMM_Vec3() const { return HMM_V3(x, y, z); }
#define IM3D_VEC4_APP \
	Vec4(HMM_Vec4 _v)        { x = _v.X; y = _v.Y; z = _v.Z; w = _v.W; } \
	operator HMM_Vec4() const { return HMM_V4(x, y, z, w); }
#define IM3D_MAT3_APP \
	Mat3(HMM_Mat3 _m)        { for (int i = 0; i < 9; ++i) m[i] = _m.Elements[i / 3][i % 3]; } \
	operator HMM_Mat3() const { HMM_Mat3 ret; for (int i = 0; i < 9; ++i) ret.Elements[i / 3][i % 3] = m[i]; return ret; }
#define IM3D_MAT4_APP \
	Mat4(HMM_Mat4 _m)        { for (int i = 0; i < 16; ++i) m[i] = _m.Elements[i / 4][i % 4]; } \
	operator HMM_Mat4() const { HMM_Mat4 ret; for (int i = 0; i < 16; ++i) ret.Elements[i / 4][i % 4] = m[i]; return ret; }
