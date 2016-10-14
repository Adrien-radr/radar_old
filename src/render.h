#pragma once

#include "common/common.h"
#include "brdf.h"

#include "render_internal/mesh.h"
#include "render_internal/texture.h"
#include "render_internal/shader.h"
#include "render_internal/framebuffer.h"
#include "render_internal/font.h"


namespace Render
{
	bool Init();
	void Destroy();
	int GetCurrentShader();
	int GetCurrentMesh();

	bool ReloadShaders();
	void StartTextRendering();
	void StartPolygonRendering();

	void StartGBufferPass();
	void StopGBufferPass();

	void ToggleGTRaytracing();
	void ResetGTAccumulation();
	void AccumulateGT();
	void UpdateView( const mat4f &viewMatrix, const vec3f &eyePos );
}
