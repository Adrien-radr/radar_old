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

	//bool ReloadShaders();
	void StartTextRendering();
	void ClearBuffers();

	void StartGBufferPass();
	void StopGBufferPass();

	void UpdateView( const mat4f &viewMatrix );
	void UpdateProjectionMatrix2D( const mat4f &proj );
	void UpdateProjectionMatrix3D( const mat4f &proj );
}
