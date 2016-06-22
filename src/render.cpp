#include "render.h"
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
		Handle DEFAULT_TEXTURE = -1;
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
		GLint           curr_GL_texture[Texture::TexTarget_N];
		Texture::Target curr_GL_texture_target;

		/// View/Camera Matrix
		mat4f           view_matrix;

		// Those arrays contain the GL-loaded resources and contains no explicit
		// duplicates.
		std::vector<Shader::Data> shaders;
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
		for (int i = 0; i < Texture::TexTarget_N; ++i)
			renderer->curr_GL_texture[i] = -1;
		renderer->curr_GL_texture_target = Texture::TexTarget0;
		glActiveTexture(GL_TEXTURE0);   // default to 1st one

		renderer->mesh_resources.clear();
		renderer->font_resources.clear();
		renderer->texture_resources.clear();
		renderer->spritesheets_resources.clear();

		renderer->ubos.clear();
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


		Shader::Handle shader_ui = Shader::Build(sd_ui_shader);
		if (shader_ui != Shader::SHADER_2D_UI) {
			LogErr("Error loading UI shader.");
			return false;
		}
		Shader::Bind(shader_ui);
		Shader::SendInt(Shader::UNIFORM_TEXTURE0, Texture::TexTarget0);

		Shader::Desc sd_mesh;
		sd_mesh.vertex_file = "data/shaders/mesh.vs";
		sd_mesh.fragment_file = "data/shaders/mesh.fs";
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
		sd_mesh.uniforms.push_back(Shader::Desc::Uniform("eyePosition", Shader::UNIFORM_EYEPOS));
		sd_mesh.uniformblocks.push_back(Shader::Desc::UniformBlock("Material", Shader::UNIFORMBLOCK_MATERIAL));


		Shader::Handle shader_mesh = Shader::Build(sd_mesh);
		if (shader_mesh != Shader::SHADER_3D_MESH) {
			LogErr("Error loading Mesh shader.");
			return false;
		}
		Shader::Bind(shader_mesh);
		Shader::SendInt(Shader::UNIFORM_TEXTURE0, Texture::TexTarget0);
		Shader::SendInt(Shader::UNIFORM_TEXTURE1, Texture::TexTarget1);
		Shader::SendInt(Shader::UNIFORM_TEXTURE2, Texture::TexTarget2);

		// Create Default white texture 
		Texture::Desc t_desc;
		t_desc.name = "data/dummy.png";
		Texture::DEFAULT_TEXTURE = Texture::Build(t_desc);
		if(Texture::DEFAULT_TEXTURE < 0) {
			LogErr("Error creating default dummy texture.");
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

	int GetCurrentShader() { return renderer->curr_GL_program; }
	int GetCurrentMesh() { return renderer->curr_GL_vao; }
	int GetCurrentTexture(Texture::Target t) { return renderer->curr_GL_texture[t]; }
	void StartTextRendering() {
		Shader::Bind(Shader::SHADER_2D_UI);
		Mesh::Bind(renderer->text_vao);	// bind general text vao
	}
	void StartPolygonRendering() {
		Shader::Bind(Shader::SHADER_3D_MESH);
		//Mesh::Bind(0);
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
				LogDebug("Mesh resource ", name, " found. Returning its handle.");
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
