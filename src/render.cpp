#include "render.h"
#include "device.h"
#include "common/resource.h"
#include "GL/glew.h"
#include "json/cJSON.h"

// Internal files for structs & functions
#include "render_internal/mesh.h"
#include "render_internal/font.h"
#include "render_internal/texture.h"



//////////////////////////////////////////////////////////////////////////////
///     Render Data Definitions
//////////////////////////////////////////////////////////////////////////////




// Definition of _Font in render_internal/font.h
// Definition of _Mesh and _TextMesh in render_internal/mesh.h
// Definition of _Texture in render_internal/texture.h

namespace Render {
	struct RenderResource {
		RenderResource() : name(""), handle(-1) {}
		std::string		name;
		int				handle;
	};

	namespace UBO {
		struct Data {
			Data() : id(0) {}
			u32 id;
		};
	}

	namespace Texture {
		// loaded in Render::Init
		Handle DEFAULT_DIFFUSE = -1;
		Handle DEFAULT_NORMAL = -1;
	}

	namespace Shader {
		struct Data {
			Data() : id(0) {}
			u32 id;                             //!< GL Shader Program ID
			// bool proj_matrix_type;              //!< Type of projection matrix used in shader

			// Arrays of uniforms{blocks}, ordered as the Shader::Uniform{Block} enum
			GLint  uniform_locations[Shader::UNIFORM_N];   //!< Locations for the possible uniforms
			GLuint uniformblock_locations[Shader::UNIFORMBLOCK_N];
		};
	}

	struct Renderer {
		// Those index the shaders & meshes arrays. They are not equal to the real
		// GL id.
		// a value of -1 mean that nothing is currently bound.
		GLint           curr_GL_program;
		GLint			curr_GL_ubo;
		GLint           curr_GL_vao;
		GLint           curr_GL_texture[Texture::TARGET_N];
		Texture::Target curr_GL_texture_target;

		GLuint			shaderFunctionLibrary;

		/// View/Camera Matrix
		mat4f           view_matrix;

		bool 			GTMode;			// Ground Truth Raytracing mode
		bool			blendChange;	// To reset the backbuffer accumulation when moving the camera
		int				accumFrames;	// number of accumulation frames

		// Those arrays contain the GL-loaded resources and contains no explicit
		// duplicates.
		std::vector<Shader::Data> shaders;
		std::vector<FBO::Data> fbos;
		std::vector<UBO::Data> ubos;
		std::vector<Mesh::Data> meshes;
		std::vector<TextMesh::Data> textmeshes;
		std::vector<Texture::Data> textures;
		std::vector<Font::Data> fonts;
		std::vector<SpriteSheet::Data> spritesheets;


		// Loaded Resources. Indexes the above arrays with string identifiers
		// Those string names are the filenames of the resources
		std::vector<RenderResource> mesh_resources;
		std::vector<RenderResource> font_resources;
		std::vector<RenderResource> texture_resources;
		std::vector<RenderResource> spritesheets_resources;

		Mesh::Handle text_vao;
	};

	/// Unique instance of the Renderer, static to this file
	static Renderer *renderer = NULL;


	static void Clean() {
		renderer->curr_GL_program = -1;
		renderer->curr_GL_vao = -1;
		renderer->curr_GL_ubo = -1;
		for (int i = 0; i < Texture::TARGET_N; ++i)
			renderer->curr_GL_texture[i] = -1;
		renderer->curr_GL_texture_target = Texture::TARGET0;
		glActiveTexture(GL_TEXTURE0);   // default to 1st one

		renderer->shaderFunctionLibrary = 0;
		renderer->GTMode = false;
		renderer->blendChange = true;
		renderer->accumFrames = 1;

		renderer->mesh_resources.clear();
		renderer->font_resources.clear();
		renderer->texture_resources.clear();
		renderer->spritesheets_resources.clear();

		renderer->ubos.clear();
		renderer->fbos.clear();
		renderer->shaders.clear();
		renderer->meshes.clear();
		renderer->textmeshes.clear();
		renderer->textures.clear();
		renderer->fonts.clear();
		renderer->spritesheets.clear();
	}

	bool Init() {
		Assert(!renderer);

		renderer = new Renderer();
		Clean();

		renderer->mesh_resources.reserve(64);
		renderer->texture_resources.reserve(64);
		renderer->font_resources.reserve(8);
		renderer->spritesheets_resources.reserve(8);

		// Create TextVao, it occupies the 1st vao slot
		Mesh::Desc text_vao_desc("TextVAO", true, 0, nullptr, 0, nullptr);
		renderer->text_vao = Mesh::Build(text_vao_desc);

		if (renderer->text_vao < 0) {
			LogErr("Error creating text VAO.");
			delete renderer;
			return false;
		}

		if(!ReloadShaders()) {
			return false;
		}

		// Create FBO for gBuffer pass
		FBO::Desc fdesc;
		fdesc.size = GetDevice().windowSize;	// TODO : resize gBuffer when window change size
		fdesc.textures.push_back(Texture::RGB8U);	// ObjectIDs
		fdesc.textures.push_back(Texture::R8U);	// Depth
		fdesc.textures.push_back(Texture::RGB8U);	// Normals
		fdesc.textures.push_back(Texture::RGB8U);	// World Pos

		FBO::Handle fboh = FBO::Build(fdesc);
		if(fboh < 0) {
			LogErr("Error creating gBuffer.");
			return false;
		}

		// Create Default white texture 
		Texture::Desc t_desc;
		t_desc.name = "data/default_diff.png";
		Texture::DEFAULT_DIFFUSE = Texture::Build(t_desc);
		if(Texture::DEFAULT_DIFFUSE < 0) {
			LogErr("Error creating default diffuse texture.");
			return false;
		}

		t_desc.name = "data/default_nrm.png";
		Texture::DEFAULT_NORMAL = Texture::Build(t_desc);
		if(Texture::DEFAULT_NORMAL < 0) {
			LogErr("Error creating default normal texture.");
			return false;
		}

		// Initialize Freetype
		if (FT_Init_FreeType(&Font::Library)) {
			LogErr("Error initializeing Freetype library.");
			return false;
		}

		LogInfo("Renderer successfully initialized.");

		return true;
	}

	void Destroy() {
		if(renderer) {
			for (u32 i = 0; i < renderer->shaders.size(); ++i)
				Shader::Destroy(i);

			for(u32 i = 0; i < renderer->ubos.size(); ++i)
				UBO::Destroy(i);

			for(u32 i = 0; i < renderer->fbos.size(); ++i)
				FBO::Destroy(i);

			for (u32 i = 0; i < renderer->meshes.size(); ++i)
				Mesh::Destroy(i);

			for (u32 i = 0; i < renderer->textmeshes.size(); ++i)
				TextMesh::Destroy(i);

			for (u32 i = 0; i < renderer->textures.size(); ++i)
				Texture::Destroy(i);

			for (u32 i = 0; i < renderer->fonts.size(); ++i)
				Font::Destroy(i);

			for (u32 i = 0; i < renderer->spritesheets.size(); ++i)
				SpriteSheet::Destroy(i);

			Clean();

			// Destroy Freetype
			FT_Done_FreeType(Font::Library);

			delete renderer;

			// D(LogInfo("Renderer destroyed.");)
		}
	}

	bool RecompileShaderLibrary(bool inited) {
		static std::string shaderLibraryPath = "data/shaders/lib.glsl";

		if(inited && renderer->shaderFunctionLibrary > 0) {
			glDeleteShader(renderer->shaderFunctionLibrary);
			renderer->shaderFunctionLibrary = 0;
		}

		std::string src;
		if(!Resource::ReadFile(src, shaderLibraryPath)) {
			LogErr("Failed to read shader library (", shaderLibraryPath, ")");
			return false;
		}

		renderer->shaderFunctionLibrary = Shader::BuildShader(src.c_str(), GL_FRAGMENT_SHADER);
		if(!renderer->shaderFunctionLibrary) {
			LogErr("Failed to build shader library.");
			return false;
		}

		return true;
	}

	bool ReloadShaders() {
		static bool inited = false;

		// Compile shader function libraries
		if(!RecompileShaderLibrary(inited))
			return false;

		int shader_slot = -1;

		if(inited && renderer->shaders[Shader::SHADER_2D_UI].id > 0) {
			Destroy(Shader::SHADER_2D_UI);
			shader_slot = Shader::SHADER_2D_UI;
		}

		// Load standard shaders in right order
		Shader::Desc sd_ui_shader;
		sd_ui_shader.vertex_file = "data/shaders/ui.vs";
		sd_ui_shader.fragment_file = "data/shaders/ui.fs";
		sd_ui_shader.attribs[0] = Shader::Desc::Attrib("in_position", 0);
		sd_ui_shader.attribs[1] = Shader::Desc::Attrib("in_texcoord", 2);

		sd_ui_shader.uniforms.push_back(Shader::Desc::Uniform("ProjMatrix", Shader::UNIFORM_PROJMATRIX));
		sd_ui_shader.uniforms.push_back(Shader::Desc::Uniform("ModelMatrix", Shader::UNIFORM_MODELMATRIX));
		sd_ui_shader.uniforms.push_back(Shader::Desc::Uniform("DiffuseTex", Shader::UNIFORM_TEXTURE0));
		sd_ui_shader.uniforms.push_back(Shader::Desc::Uniform("text_color", Shader::UNIFORM_TEXTCOLOR));
		sd_ui_shader.shaderSlot = shader_slot;

		Shader::Handle shader_ui = Shader::Build(sd_ui_shader);
		if (shader_ui != Shader::SHADER_2D_UI) {
			LogErr("Error loading UI shader.");
			return false;
		}
		Shader::Bind(shader_ui);
		Shader::SendInt(Shader::UNIFORM_TEXTURE0, Texture::TARGET0);

		shader_slot = -1;

		if(inited && renderer->shaders[Shader::SHADER_3D_MESH].id > 0) {
			Destroy(Shader::SHADER_3D_MESH);
			shader_slot = Shader::SHADER_3D_MESH;
		}

		Shader::Desc sd_mesh;
		sd_mesh.vertex_file = "data/shaders/mesh_vert.glsl";
		sd_mesh.fragment_file = "data/shaders/mesh_frag.glsl";
		sd_mesh.attribs[0] = Shader::Desc::Attrib("in_position", 0);
		sd_mesh.attribs[1] = Shader::Desc::Attrib("in_normal", 1);
		sd_mesh.attribs[2] = Shader::Desc::Attrib("in_texcoord", 2);
		sd_mesh.attribs[3] = Shader::Desc::Attrib("in_tangent", 3);
		sd_mesh.attribs[4] = Shader::Desc::Attrib("in_binormal", 4);
		sd_mesh.attribs[5] = Shader::Desc::Attrib("in_color", 5);

		sd_mesh.uniforms.push_back(Shader::Desc::Uniform("ModelMatrix", Shader::UNIFORM_MODELMATRIX));
		sd_mesh.uniforms.push_back(Shader::Desc::Uniform("ViewMatrix", Shader::UNIFORM_VIEWMATRIX));
		sd_mesh.uniforms.push_back(Shader::Desc::Uniform("ProjMatrix", Shader::UNIFORM_PROJMATRIX));
		sd_mesh.uniforms.push_back(Shader::Desc::Uniform("DiffuseTex", Shader::UNIFORM_TEXTURE0));
		sd_mesh.uniforms.push_back(Shader::Desc::Uniform("SpecularTex", Shader::UNIFORM_TEXTURE1));
		sd_mesh.uniforms.push_back(Shader::Desc::Uniform("NormalTex", Shader::UNIFORM_TEXTURE2));
		sd_mesh.uniforms.push_back(Shader::Desc::Uniform("AOTex", Shader::UNIFORM_TEXTURE3));
		sd_mesh.uniforms.push_back(Shader::Desc::Uniform("ltc_mat", Shader::UNIFORM_TEXTURE4));
		sd_mesh.uniforms.push_back(Shader::Desc::Uniform("ltc_amp", Shader::UNIFORM_TEXTURE5));
		sd_mesh.uniforms.push_back(Shader::Desc::Uniform("eyePosition", Shader::UNIFORM_EYEPOS));
		sd_mesh.uniforms.push_back(Shader::Desc::Uniform("nPointLights", Shader::UNIFORM_NPOINTLIGHTS));
		sd_mesh.uniforms.push_back(Shader::Desc::Uniform("nAreaLights", Shader::UNIFORM_NAREALIGHTS));
		sd_mesh.uniforms.push_back(Shader::Desc::Uniform("GTMode", Shader::UNIFORM_GROUNDTRUTH));
		sd_mesh.uniforms.push_back(Shader::Desc::Uniform("globalTime", Shader::UNIFORM_GLOBALTIME));
		sd_mesh.uniformblocks.push_back(Shader::Desc::UniformBlock("Material", Shader::UNIFORMBLOCK_MATERIAL));
		sd_mesh.uniformblocks.push_back(Shader::Desc::UniformBlock("PointLights", Shader::UNIFORMBLOCK_POINTLIGHTS));
		sd_mesh.uniformblocks.push_back(Shader::Desc::UniformBlock("AreaLights", Shader::UNIFORMBLOCK_AREALIGHTS));
		sd_mesh.shaderSlot = shader_slot;
		sd_mesh.linkedLibraries.push_back(renderer->shaderFunctionLibrary);


		Shader::Handle shader_mesh = Shader::Build(sd_mesh);
		if (shader_mesh != Shader::SHADER_3D_MESH) {
			LogErr("Error loading Mesh shader.");
			return false;
		}
		Shader::Bind(shader_mesh);
		Shader::SendInt(Shader::UNIFORM_TEXTURE0, Texture::TARGET0);
		Shader::SendInt(Shader::UNIFORM_TEXTURE1, Texture::TARGET1);
		Shader::SendInt(Shader::UNIFORM_TEXTURE2, Texture::TARGET2);
		Shader::SendInt(Shader::UNIFORM_TEXTURE3, Texture::TARGET3);
		Shader::SendInt(Shader::UNIFORM_TEXTURE4, Texture::TARGET4);
		Shader::SendInt(Shader::UNIFORM_TEXTURE5, Texture::TARGET5);
		Shader::SendInt(Shader::UNIFORM_GROUNDTRUTH, (int)renderer->GTMode);
		Shader::SendFloat(Shader::UNIFORM_GLOBALTIME, 0.f);	// Default : not using ground truth raytracing

		shader_slot = -1;

		if(inited && renderer->shaders[Shader::SHADER_GBUFFERPASS].id > 0) {
			Destroy(Shader::SHADER_GBUFFERPASS);
			shader_slot = Shader::SHADER_GBUFFERPASS;
		}

		Shader::Desc sd_gbuf;
		sd_gbuf.vertex_file = "data/shaders/gBufferPass_vert.glsl";
		sd_gbuf.fragment_file = "data/shaders/gBufferPass_frag.glsl";
		sd_gbuf.attribs[0] = Shader::Desc::Attrib("in_position", 0);
		sd_gbuf.attribs[1] = Shader::Desc::Attrib("in_normal", 1);
		sd_gbuf.attribs[2] = Shader::Desc::Attrib("in_texcoord", 2);
		sd_gbuf.attribs[3] = Shader::Desc::Attrib("in_tangent", 3);
		sd_gbuf.attribs[4] = Shader::Desc::Attrib("in_binormal", 4);
		sd_gbuf.attribs[5] = Shader::Desc::Attrib("in_color", 5);

		sd_gbuf.uniforms.push_back(Shader::Desc::Uniform("ModelMatrix", Shader::UNIFORM_MODELMATRIX));
		sd_gbuf.uniforms.push_back(Shader::Desc::Uniform("ViewMatrix", Shader::UNIFORM_VIEWMATRIX));
		sd_gbuf.uniforms.push_back(Shader::Desc::Uniform("ProjMatrix", Shader::UNIFORM_PROJMATRIX));
		sd_gbuf.uniforms.push_back(Shader::Desc::Uniform("ObjectID", Shader::UNIFORM_OBJECTID));
		sd_gbuf.shaderSlot = shader_slot;


		Shader::Handle shader_gbuf = Shader::Build(sd_gbuf);
		if (shader_gbuf != Shader::SHADER_GBUFFERPASS) {
			LogErr("Error loading GBuffer shader.");
			return false;
		}
		Shader::Bind(shader_gbuf);


		inited = true;
		return true;
	}

	int GetCurrentShader() { return renderer->curr_GL_program; }
	int GetCurrentMesh() { return renderer->curr_GL_vao; }
	int GetCurrentTexture(Texture::Target t) { return renderer->curr_GL_texture[t]; }
	void StartTextRendering() {
		Shader::Bind(Shader::SHADER_2D_UI);
		Mesh::Bind(renderer->text_vao);	// bind general text vao
	}

	void StartGBufferPass() {
		FBO::Bind(0);	// gBuffer is always the 1st created fbo
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		Shader::Bind(Render::Shader::SHADER_GBUFFERPASS);
	}

	void StopGBufferPass() {
		glBindFramebuffer(GL_FRAMEBUFFER, 0); // use the default FB again
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}

	void StartPolygonRendering() {
		
		// glClear(GL_ACCUM_BUFFER_BIT);
		// glClearColor(0.0f, 0.0f, 0.0f, 0.f);

		glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
		// Clear the backbuffers if not in accumulating mode
		if(!renderer->GTMode || renderer->blendChange) {
			renderer->accumFrames = 1;
			// glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
			// glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			// glBlendFunc(GL_ONE, GL_ONE);
			// glEnable(GL_BLEND);
		} else {
			// add last frame's to the accum
			// glAccum(GL_ACCUM, renderer->accumFrames / (float)(renderer->accumFrames+1));
        	// glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
			// glClear(GL_DEPTH_BUFFER_BIT);
			// glBlendFunc(GL_ONE, GL_ONE);
			++renderer->accumFrames;
		}
		// Shader::Bind(Shader::SHADER_3D_MESH);
		//Mesh::Bind(0);
	}

	void ToggleGTRaytracing() {
		renderer->GTMode = !renderer->GTMode;
		Shader::Bind(Shader::SHADER_3D_MESH);
		Shader::SendInt(Shader::UNIFORM_GROUNDTRUTH, (int)renderer->GTMode);
		ResetGTAccumulation();
	}

	void ResetGTAccumulation() {
		renderer->blendChange = true;
		// glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // reset backbuffer accumulation for 1 frame
	}

	void AccumulateGT() {
		if(renderer->GTMode && renderer->blendChange) {
			renderer->blendChange = false;
			// glBlendFunc(GL_ONE, GL_ONE);
			// glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		}
		if(renderer->GTMode) {

		// glAccum(GL_ACCUM, 1.f / (float)(renderer->accumFrames));
		// glAccum(GL_RETURN, 1.f);
		}

			// glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}

	void UpdateView(const mat4f &viewMatrix, const vec3f &eyePos) {
		for(int i = Shader::_SHADER_3D_PROJECTION_START; i < Shader::_SHADER_3D_PROJECTION_END; ++i) {
			Shader::Bind(i);
			Shader::SendMat4(Shader::UNIFORM_VIEWMATRIX, viewMatrix);
			Shader::SendVec3(Shader::UNIFORM_EYEPOS, eyePos);
		}

		// view changed, reset blend mode for Raytracing
		if(renderer->GTMode) {
			ResetGTAccumulation();
		}
	}

	// @param addSlot : if true, actively search for an empty slot to store the unexisting resource
	static bool FindResource(std::vector<RenderResource> &resources, const std::string &name, int &free_index, bool addSlot = true) {
		free_index = -1;

		// search for resource. return it if it exists
		for (u32 i = 0; i < resources.size(); ++i) {
			if (addSlot && resources[i].handle == -1 && free_index == -1) {
				free_index = i;
				break;
			}
			if (resources[i].name == name) {
				// LogDebug("Mesh resource ", name, " found. Returning its handle.");
				free_index = resources[i].handle;
				return true;
			}
		}

		// if mesh resource wasnt found and no free slot either, create a new resource slot
		if (addSlot && free_index == -1) {
			free_index = (int)resources.size();
			resources.push_back(RenderResource());
		}
		return false;
	}

	static void AddResource(std::vector<RenderResource> &resources, int index,
							const std::string &name, int handle) {
		LogDebug("Adding ", name, " to render resources.");
		resources[index].name = name;
		resources[index].handle = handle;
	}


	// Function definitions for all render types
	#include "render_internal/shader.inl"
	#include "render_internal/texture.inl"
	#include "render_internal/font.inl"
	#include "render_internal/mesh.inl"
}
