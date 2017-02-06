#include "render.h"
#include "device.h"
#include "common/resource.h"
#include "common/SHEval.h"
#include "GL/glew.h"
#include "json/cJSON.h"

#include <algorithm>
#include <ft2build.h>
#include FT_FREETYPE_H


namespace Render
{
	struct RenderResource
	{
		RenderResource() : name( "" ), handle( -1 ) {}
		std::string		name;
		int				handle;
	};

	struct Renderer
	{
		// Those index the shaders & meshes arrays. They are not equal to the real
		// GL id.
		// a value of -1 mean that nothing is currently bound.
		GLint           curr_GL_program;
		GLint			curr_GL_ubo;
		GLint           curr_GL_vao;
		GLint           curr_GL_texture[Texture::_TARGET_N];
		GLint			curr_GL_cubemap_texture;
		Texture::Target curr_GL_texture_target;


		/// View/Camera Matrix
		mat4f           view_matrix;

		// Those arrays contain the GL-loaded resources and contains no explicit
		// duplicates.
		std::vector<FBO::_internal::Data> fbos;
		std::vector<Shader::_internal::Data> shaders;
		std::vector<UBO::_internal::Data> ubos;
		std::vector<Mesh::_internal::Data> meshes;
		std::vector<TextMesh::_internal::Data> textmeshes;
		std::vector<Texture::_internal::Data> textures;
		std::vector<Font::_internal::Data> fonts;
		std::vector<SpriteSheet::_internal::Data> spritesheets;


		std::vector<Shader::Handle> shaders_proj3d; //!< list of shaders using 3D projection matrix
		std::vector<Shader::Handle> shaders_proj2d; //!< list of shaders using 2d projection matrix

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


	static void Clean()
	{
		renderer->curr_GL_program = -1;
		renderer->curr_GL_vao = -1;
		renderer->curr_GL_ubo = -1;
		for ( int i = 0; i < Texture::_TARGET_N; ++i )
			renderer->curr_GL_texture[i] = -1;
		renderer->curr_GL_cubemap_texture = -1;
		renderer->curr_GL_texture_target = Texture::TARGET0;
		glActiveTexture( GL_TEXTURE0 );   // default to 1st one

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

	bool Init()
	{
		Assert( !renderer );

		renderer = new Renderer();
		Clean();

		renderer->mesh_resources.reserve( 64 );
		renderer->texture_resources.reserve( 64 );
		renderer->font_resources.reserve( 8 );
		renderer->spritesheets_resources.reserve( 8 );

		// Create TextVao, it occupies the 1st vao slot
		Mesh::Desc text_vao_desc( "TextVAO", true, 0, nullptr, 0, nullptr );
		renderer->text_vao = Mesh::Build( text_vao_desc );

		if ( renderer->text_vao < 0 )
		{
			LogErr( "Error creating text VAO." );
			delete renderer;
			return false;
		}

#if 0
		if ( !ReloadShaders() )
		{
			return false;
		}
#endif
		// Create FBO for gBuffer pass
		FBO::Desc fdesc;
		fdesc.size = GetDevice().windowSize;	// TODO : resize gBuffer when window change size
		fdesc.textures.push_back( Texture::RGB16F );	// ObjectIDs
		fdesc.textures.push_back( Texture::R32F );	// Depth
		fdesc.textures.push_back( Texture::RGB32F );	// Normals
		fdesc.textures.push_back( Texture::RGB32F );	// World Pos

		FBO::Handle fboh = FBO::Build( fdesc );
		if ( fboh < 0 )
		{
			LogErr( "Error creating gBuffer." );
			return false;
		}

		// Create Default white texture 
		Texture::Desc t_desc;
		t_desc.name[0] = "../radar/data/default_diff.png";
		Texture::DEFAULT_DIFFUSE = Texture::Build( t_desc );
		if ( Texture::DEFAULT_DIFFUSE < 0 )
		{
			LogErr( "Error creating default diffuse texture." );
			return false;
		}

		t_desc.name[0] = "../radar/data/default_nrm.png";
		Texture::DEFAULT_NORMAL = Texture::Build( t_desc );
		if ( Texture::DEFAULT_NORMAL < 0 )
		{
			LogErr( "Error creating default normal texture." );
			return false;
		}

		// Initialize Freetype
		if ( !Font::InitFontLibrary() )
		{
			LogErr( "Error initializeing Freetype library." );
			return false;
		}

		// Init Skybox Shader
		Shader::Desc sd_skybox;
		sd_skybox.vertex_file = "../radar/data/shaders/skybox_vert.glsl";
		sd_skybox.fragment_file = "../radar/data/shaders/skybox_frag.glsl";
		sd_skybox.attribs[0] = Shader::Desc::Attrib( "in_position", 0 );

		sd_skybox.uniforms.push_back( Shader::Desc::Uniform( "ViewMatrix", Shader::UNIFORM_VIEWMATRIX ) );
		sd_skybox.uniforms.push_back( Shader::Desc::Uniform( "ProjMatrix", Shader::UNIFORM_PROJMATRIX ) );
		sd_skybox.projType = Shader::PROJECTION_3D;

		Shader::Handle shader_skybox = Shader::Build( sd_skybox );
		if ( shader_skybox != Shader::SHADER_SKYBOX )
		{
			LogErr( "Error loading Skybox shader." );
			return false;
		}
		Shader::Bind( shader_skybox );
		Shader::SendInt( Shader::UNIFORM_TEXTURE0, Texture::TARGET0 );

		// Get those projection matrix in our shaders
		GetDevice().UpdateProjection();

		LogInfo( "Renderer successfully initialized." );

		return true;
	}

	void Destroy()
	{
		if ( renderer )
		{
			for ( u32 i = 0; i < renderer->shaders.size(); ++i )
				Shader::Destroy( i );

			for ( u32 i = 0; i < renderer->ubos.size(); ++i )
				UBO::Destroy( i );

			for ( u32 i = 0; i < renderer->fbos.size(); ++i )
				FBO::Destroy( i );

			for ( u32 i = 0; i < renderer->meshes.size(); ++i )
				Mesh::Destroy( i );

			for ( u32 i = 0; i < renderer->textmeshes.size(); ++i )
				TextMesh::Destroy( i );

			for ( u32 i = 0; i < renderer->textures.size(); ++i )
				Texture::Destroy( i );

			for ( u32 i = 0; i < renderer->fonts.size(); ++i )
				Font::Destroy( i );

			for ( u32 i = 0; i < renderer->spritesheets.size(); ++i )
				SpriteSheet::Destroy( i );

			Clean();

			// Destroy Freetype
			Font::DestroyFontLibrary();

			delete renderer;

			// D(LogInfo("Renderer destroyed.");)
		}
	}
#if 0 // UI SHADER

	if ( inited && shaders[Shader::SHADER_2D_UI].id > 0 )
	{
		Destroy( Shader::SHADER_2D_UI );
		shader_slot = Shader::SHADER_2D_UI;
	}

	// Load standard shaders in right order
	Shader::Desc sd_ui_shader;
	sd_ui_shader.vertex_file = "../radar/data/shaders/ui.vs";
	sd_ui_shader.fragment_file = "../radar/data/shaders/ui.fs";
	sd_ui_shader.attribs[0] = Shader::Desc::Attrib( "in_position", 0 );
	sd_ui_shader.attribs[1] = Shader::Desc::Attrib( "in_texcoord", 2 );

	sd_ui_shader.uniforms.push_back( Shader::Desc::Uniform( "ProjMatrix", Shader::UNIFORM_PROJMATRIX ) );
	sd_ui_shader.uniforms.push_back( Shader::Desc::Uniform( "ModelMatrix", Shader::UNIFORM_MODELMATRIX ) );
	sd_ui_shader.uniforms.push_back( Shader::Desc::Uniform( "DiffuseTex", Shader::UNIFORM_TEXTURE0 ) );
	sd_ui_shader.uniforms.push_back( Shader::Desc::Uniform( "text_color", Shader::UNIFORM_TEXTCOLOR ) );
	sd_ui_shader.shaderSlot = shader_slot;
	sd_mesh.projType = Shader::PROJECTION_2D;

	Shader::Handle shader_ui = Shader::Build( sd_ui_shader );
	if ( shader_ui != Shader::SHADER_2D_UI )
	{
		LogErr( "Error loading UI shader." );
		return false;
	}
	Shader::Bind( shader_ui );
	Shader::SendInt( Shader::UNIFORM_TEXTURE0, Texture::TARGET0 );
#endif
	int GetCurrentShader() { return renderer->curr_GL_program; }
	int GetCurrentMesh() { return renderer->curr_GL_vao; }
	int GetCurrentTexture( Texture::Target t ) { return renderer->curr_GL_texture[t]; }
	Texture::Target GetCurrentTextureTarget() { return renderer->curr_GL_texture_target; }
	int GetCurrentCubemapTexture() { return renderer->curr_GL_cubemap_texture; }

	void StartTextRendering()
	{
		//Shader::Bind( renderer->shaders[] );
		//Mesh::Bind( renderer->text_vao );	// bind general text vao
	}

	void BindGBuffer()
	{
		FBO::Bind( 0 );	// gBuffer is always the 1st created fbo
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	}

	void UnbindGBuffer()
	{
		glBindFramebuffer( GL_FRAMEBUFFER, 0 ); // use the default FB again
	}

	void ClearBuffers()
	{

		// glClear(GL_ACCUM_BUFFER_BIT);
		// glClearColor(0.0f, 0.0f, 0.0f, 0.f);

		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	}

	void UpdateView( const mat4f &viewMatrix )
	{
		for ( size_t i = 0; i < renderer->shaders.size(); ++i )
		{
			// force bind each shader in succession
			glUseProgram( renderer->shaders[i].id );
			Shader::SendMat4( Shader::UNIFORM_VIEWMATRIX, viewMatrix );
		}
		// force unbind to reset state
		Shader::Bind( -1 );

		// Update Skybox shader
		Render::Shader::Bind( Shader::SHADER_SKYBOX );
		mat4f skyboxView( viewMatrix );
		skyboxView[3] = vec4f( 0, 0, 0, 1 );
		Shader::SendMat4( Shader::UNIFORM_VIEWMATRIX, skyboxView );
	}

	void UpdateProjectionMatrix2D( const mat4f &proj )
	{
		for ( u32 i = 0; i < renderer->shaders_proj2d.size(); ++i )
		{
		    Render::Shader::Bind(renderer->shaders_proj2d[i]);
		    Render::Shader::SendMat4(Render::Shader::UNIFORM_PROJMATRIX, proj);
		}
	}

	void UpdateProjectionMatrix3D( const mat4f &proj )
	{
		for ( u32 i = 0; i < renderer->shaders_proj3d.size(); ++i )
		{
			Render::Shader::Bind( renderer->shaders_proj3d[i] );
			Render::Shader::SendMat4( Render::Shader::UNIFORM_PROJMATRIX, proj );
		}
	}

	// @param addSlot : if true, actively search for an empty slot to store the unexisting resource
	static bool FindResource( std::vector<RenderResource> &resources, const std::string &name, int &free_index, bool addSlot = true )
	{
		free_index = -1;

		// search for resource. return it if it exists
		for ( u32 i = 0; i < resources.size(); ++i )
		{
			if ( addSlot && resources[i].handle == -1 && free_index == -1 )
			{
				free_index = i;
				break;
			}
			if ( resources[i].name == name )
			{
				// LogDebug("Mesh resource ", name, " found. Returning its handle.");
				free_index = resources[i].handle;
				return true;
			}
		}

		// if mesh resource wasnt found and no free slot either, create a new resource slot
		if ( addSlot && free_index == -1 )
		{
			free_index = (int) resources.size();
			resources.push_back( RenderResource() );
		}
		return false;
	}

	static void AddResource( std::vector<RenderResource> &resources, int index,
		const std::string &name, int handle )
	{
		LogDebug( "Adding ", name, " to render resources." );
		resources[index].name = name;
		resources[index].handle = handle;
	}


}

// Function definitions for all render types
#include "render_internal/framebuffer.inl"
#include "render_internal/font.inl"
#include "render_internal/texture.inl"
#include "render_internal/mesh.inl"
#include "render_internal/shader.inl"
