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

	namespace Shader {
		struct Data {
			Data() : id(0), proj_matrix_type(0) {}
			u32 id;                             //!< GL Shader Program ID
			bool proj_matrix_type;              //!< Type of projection matrix used in shader

			int uniform_locations[Shader::UNIFORM_N];   //!< Locations for the possible uniforms
			//!< Ordered as the Shader::Uniform enum

		};
	}

	struct Renderer {
		// Those index the shaders & meshes arrays. They are not equal to the real
		// GL id.
		// a value of -1 mean that nothing is currently bound.
		GLint           curr_GL_program;
		GLint           curr_GL_vao;
		GLint           curr_GL_texture[Texture::TexTarget_N];
		Texture::Target curr_GL_texture_target;

		/// View/Camera Matrix
		mat4f           view_matrix;

		// Those arrays contain the GL-loaded resources and contains no explicit
		// duplicates.
		std::vector<Shader::Data> shaders;
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
		for (int i = 0; i < Texture::TexTarget_N; ++i)
			renderer->curr_GL_texture[i] = -1;
		renderer->curr_GL_texture_target = Texture::TexTarget0;
		glActiveTexture(GL_TEXTURE0);   // default to 1st one

		renderer->mesh_resources.clear();
		renderer->font_resources.clear();
		renderer->texture_resources.clear();
		renderer->spritesheets_resources.clear();

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

		renderer->mesh_resources.reserve(10);
		renderer->texture_resources.reserve(5);
		renderer->font_resources.reserve(5);
		renderer->spritesheets_resources.reserve(10);

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

		sd_ui_shader.uniforms[0] = Shader::Desc::Uniform("ProjMatrix", Shader::UNIFORM_PROJMATRIX);
		sd_ui_shader.uniforms[1] = Shader::Desc::Uniform("ModelMatrix", Shader::UNIFORM_MODELMATRIX);
		sd_ui_shader.uniforms[2] = Shader::Desc::Uniform("tex0", Shader::UNIFORM_TEXTURE0);
		sd_ui_shader.uniforms[3] = Shader::Desc::Uniform("text_color", Shader::UNIFORM_TEXTCOLOR);


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
		sd_mesh.attribs[3] = Shader::Desc::Attrib("in_color", 3);
		sd_mesh.uniforms[0] = Shader::Desc::Uniform("ModelMatrix", Shader::UNIFORM_MODELMATRIX);
		sd_mesh.uniforms[1] = Shader::Desc::Uniform("ViewMatrix", Shader::UNIFORM_VIEWMATRIX);
		sd_mesh.uniforms[2] = Shader::Desc::Uniform("ProjMatrix", Shader::UNIFORM_PROJMATRIX);
		sd_mesh.uniforms[3] = Shader::Desc::Uniform("tex0", Shader::UNIFORM_TEXTURE0);
		sd_mesh.uniforms[4] = Shader::Desc::Uniform("eyePosition", Shader::UNIFORM_EYEPOS);


		Shader::Handle shader_mesh = Shader::Build(sd_mesh);
		if (shader_mesh != Shader::SHADER_3D_MESH) {
			LogErr("Error loading Mesh shader.");
			return false;
		}
		Shader::Bind(shader_mesh);
		Shader::SendInt(Shader::UNIFORM_TEXTURE0, Texture::TexTarget0);

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
				// found resource by this name, return its handle
				D(LogInfo("Mesh resource ", name, " found. Returning its handle.");)
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
		D(LogInfo("Adding ", name, " to render resources.");)
		resources[index].name = name;
		resources[index].handle = handle;
	}

	namespace Shader {
		static GLuint build_new_shader(const char *src, GLenum type) {
			GLuint shader = glCreateShader(type);

			glShaderSource(shader, 1, &src, NULL);
			glCompileShader(shader);

			GLint status;
			glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

			if (!status) {
				GLint len;
				glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);

				GLchar *log = (GLchar*)malloc(len);
				glGetShaderInfoLog(shader, len, NULL, log);

				LogErr("\nShader compilation error :\n"
					"------------------------------------------\n",
					log,
					"------------------------------------------\n");

				free(log);
				shader = 0;
			}
			return shader;
		}

		Handle Build(const Desc &desc) {
			std::string v_src, f_src;
			GLuint v_shader = 0, f_shader = 0;
			bool from_file = false;

			// find out if building from source or from file
			// if given both(which makes no sense, dont do that), load from file
			if (!desc.vertex_file.empty() && !desc.fragment_file.empty()) {
				if (!Resource::ReadFile(v_src, desc.vertex_file)) {
					LogErr("Failed to read Vertex Shader file ", desc.vertex_file, ".");
					return -1;
				}

				if (!Resource::ReadFile(f_src, desc.fragment_file)) {
					LogErr("Failed to read Vertex Shader file ", desc.fragment_file, ".");
					return -1;
				}
				from_file = true;
			}
			else if (!desc.vertex_src.empty() && !desc.fragment_src.empty()) {
				v_src = desc.vertex_src;
				f_src = desc.fragment_src;
			}
			else {
				LogErr("Call shader_build with desc->vertex_file & desc->fragment_file OR "
					"with desc->vertex_src & desc->fragment_src. None were given.");
				return -1;
			}

			Shader::Data shader;

			shader.id = glCreateProgram();

			v_shader = build_new_shader(v_src.c_str(), GL_VERTEX_SHADER);
			if (!v_shader) {
				if(from_file)
					LogErr("Failed to build '", desc.vertex_file.c_str(), "' Vertex shader.");
				else
					LogErr("Failed to build Vertex shader from source.");
				glDeleteProgram(shader.id);
				return -1;
			}

			f_shader = build_new_shader(f_src.c_str(), GL_FRAGMENT_SHADER);
			if (!f_shader) {
				if(from_file)
					LogErr("Failed to build '", desc.fragment_file.c_str(), "' Fragment shader.");
				else
					LogErr("Failed to build Fragment shader from source.");
				glDeleteShader(v_shader);
				glDeleteProgram(shader.id);
				return -1;
			}

			glAttachShader(shader.id, v_shader);
			glAttachShader(shader.id, f_shader);

			glDeleteShader(v_shader);
			glDeleteShader(f_shader);

			// Set the Attribute Locations
			for (u32 i = 0; desc.attribs[i].used && i < SHADER_MAX_ATTRIBUTES; ++i) {
				glBindAttribLocation(shader.id, desc.attribs[i].location,
									 desc.attribs[i].name.c_str());
			}


			glLinkProgram(shader.id);

			GLint status;
			glGetProgramiv(shader.id, GL_LINK_STATUS, &status);

			if (!status) {
				GLint len;
				glGetProgramiv(shader.id, GL_INFO_LOG_LENGTH, &len);

				GLchar *log = (GLchar*)malloc(len);
				glGetProgramInfoLog(shader.id, len, NULL, log);

				LogErr("Shader Program link error : \n"
					"-----------------------------------------------------\n",
					log,
					"-----------------------------------------------------");

				free(log);
				glDeleteProgram(shader.id);
				return -1;
			}

			// Find out Uniform Locations
			memset(shader.uniform_locations, 0, sizeof(int)*UNIFORM_N); // blank all
			for (u32 i = 0; desc.uniforms[i].used && i < SHADER_MAX_UNIFORMS; ++i) {
				int loc = glGetUniformLocation(shader.id, desc.uniforms[i].name.c_str());
				if (loc < 0) {
					LogErr("While parsing Shader", desc.vertex_file, ", Uniform variable '",
						desc.uniforms[i].name, "' gave no location!");
				}
				else {
					// get the uniform location descriptor from the name
					shader.uniform_locations[desc.uniforms[i].desc] = loc;
				}
			}

			// Shader successfully loaded. Increment global counter
			int s = (int)renderer->shaders.size();
			renderer->shaders.push_back(shader);

			return s;
		}

		void Destroy(Handle h) {
			if (Exists(h)) {
				glDeleteProgram(renderer->shaders[h].id);
				renderer->shaders[h].id = 0;
			}
		}

		void Bind(Handle h) {
			GLint program = (h >= 0 && h < (int)renderer->shaders.size()) ? h : -1;

			if (renderer->curr_GL_program != program) {
				renderer->curr_GL_program = program;
				glUseProgram(program >= 0 ? (int)renderer->shaders[program].id : -1);
			}
		}

		void SendVec2(Uniform target, vec2f value) {
			glUniform2fv(renderer->shaders[renderer->curr_GL_program].uniform_locations[target],
				1, (GLfloat const *)value);
		}

		void SendVec3(Uniform target, vec3f value) {
			glUniform3fv(renderer->shaders[renderer->curr_GL_program].uniform_locations[target],
				1, (GLfloat const *)value);
		}

		void SendVec4(Uniform target, vec4f value) {
			glUniform4fv(renderer->shaders[renderer->curr_GL_program].uniform_locations[target],
				1, (GLfloat const *)value);
		}

		//void shader_send_mat3(Uniform target, mat3f value) {
			//glUniformMatrix3fv(renderer->shaders[renderer->curr_GL_program].uniform_locations[target],
				//1, GL_FALSE, (GLfloat const *)value);
		//}

		void SendMat4(Uniform target, mat4f value) {
			glUniformMatrix4fv(renderer->shaders[renderer->curr_GL_program].uniform_locations[target],
				1, GL_FALSE, (GLfloat const *)value);
		}

		void SendInt(Uniform target, int value) {
			glUniform1i(renderer->shaders[renderer->curr_GL_program].uniform_locations[target],
				value);
		}

		void SendFloat(Uniform target, f32 value) {
			glUniform1f(renderer->shaders[renderer->curr_GL_program].uniform_locations[target],
				value);
		}

		void SendVec2(const std::string &var_name, vec2f value) {
			glUniform2fv(glGetUniformLocation(renderer->shaders[renderer->curr_GL_program].id,
				var_name.c_str()), 1, (GLfloat const *)value);
		}

		void SendVec3(const std::string &var_name, vec3f value) {
			glUniform3fv(glGetUniformLocation(renderer->shaders[renderer->curr_GL_program].id,
				var_name.c_str()), 1, (GLfloat const *)value);
		}

		void SendVec4(const std::string &var_name, vec4f value) {
			glUniform4fv(glGetUniformLocation(renderer->shaders[renderer->curr_GL_program].id,
				var_name.c_str()), 1, (GLfloat const *)value);
		}

		//void SendMat3(const std::string &var_name, mat3f value) {
			//glUniformMatrix3fv(glGetUniformLocation(renderer->shaders[renderer->curr_GL_program].id,
				//var_name), 1, GL_FALSE, (GLfloat const *)value);
		//}

		void SendMat4(const std::string &var_name, mat4f value) {
			glUniformMatrix4fv(glGetUniformLocation(renderer->shaders[renderer->curr_GL_program].id,
				var_name.c_str()), 1, GL_FALSE, (GLfloat const *)value);
		}

		void SendInt(const std::string &var_name, int value) {
			glUniform1i(glGetUniformLocation(renderer->shaders[renderer->curr_GL_program].id,
				var_name.c_str()), value);
		}

		void SendFloat(const std::string &var_name, f32 value) {
			glUniform1f(glGetUniformLocation(renderer->shaders[renderer->curr_GL_program].id,
				var_name.c_str()), value);
		}

		bool Exists(Handle h) {
			return h >= 0 && h< (int)renderer->shaders.size() && renderer->shaders[h].id > 0;
		}
	}



	namespace Texture {
		Handle Build(const Desc &desc) {
			// Check if this font resource already exist
			int free_index;
			bool found_resource = FindResource(renderer->texture_resources, desc.name, free_index);

			if (found_resource) {
				return free_index;
			}


			Texture::Data texture;

			if (desc.from_file) {
				if (!Load(texture, desc.name)) {
					LogErr("Error while loading '", desc.name, "' image.");
					return -1;
				}
			}
			else {
				glGenTextures(1, &texture.id);
				glBindTexture(GL_TEXTURE_2D, texture.id);
				texture.size = desc.size;
			}


			int tex_i = (int)renderer->textures.size();
			renderer->textures.push_back(texture);

			// Add texture to render resources
			AddResource(renderer->texture_resources, free_index, desc.name, tex_i);

			return tex_i;
		}

		void Destroy(Handle h) {
			if (h >= 0 && h < (int)renderer->textures.size()) {
				Texture::Data &tex = renderer->textures[h];
				glDeleteTextures(1, &tex.id);
				tex.id = 0;
			}
		}

		void Bind(Handle h, Target target) {
			GLint tex = Exists(h) ? h : -1;

			// Switch Texture Target if needed
			if (target != renderer->curr_GL_texture_target) {
				renderer->curr_GL_texture_target = target;
				glActiveTexture(GL_TEXTURE0 + target);
			}

			// Switch active Texture if needed
			if (renderer->curr_GL_texture[target] != tex) {
				renderer->curr_GL_texture[target] = tex;
				glBindTexture(GL_TEXTURE_2D, tex >= 0 ? (int)renderer->textures[tex].id : -1);
			}
		}

		bool Exists(Handle h) {
			return h >= 0 && h < (int)renderer->textures.size() && renderer->textures[h].id > 0;
		}

	}



	namespace Font {
		Handle Build(Desc &desc) {
			if (desc.name.empty()) {
				LogErr("Font descriptor must includea font filename.");
				return -1;
			}
			if (!Resource::CheckExtension(desc.name, "ttf")) {
				LogErr("Font file must be a Truetype .ttf file.");
				return -1;
			}

			// Check if this font resource already exist
			int free_index;
			std::stringstream resource_name;
			resource_name << desc.name << "_" << desc.size;
			bool found_resource = FindResource(renderer->font_resources, resource_name.str(), free_index);

			if (found_resource) {
				return free_index;
			}


			Font::Data font;
			font.size = vec2i(0, 0);
			font.handle = -1;

			if (!CreateAtlas(font, desc.name, desc.size)) {
				LogErr("Error while loading font '", desc.name, "'.");
				return -1;
			}

			LogInfo("Loaded font ", desc.name, ".");

			u32 font_i = (int)renderer->fonts.size();
			renderer->fonts.push_back(font);

			// Add created font to renderer resources
			AddResource(renderer->font_resources, free_index, resource_name.str(), font_i);

			return font_i;
		}

		void Destroy(Handle h) {
			if (Exists(h)) {
				Font::Data &font = renderer->fonts[h];
				FT_Done_Face(font.face);
				Texture::Destroy(font.handle);
				font.handle = -1;
			}
		}

		void Bind(Handle h, Texture::Target target) {
			if (Exists(h))
				Texture::Bind(renderer->fonts[h].handle, target);
		}

		bool Exists(Handle h) {
			return h >= 0 && h < (int)renderer->fonts.size() && renderer->fonts[h].handle >= 0;
		}
	}



	namespace Mesh {
		Handle Build(const Desc &desc) {
			f32 *vp = nullptr, *vn = nullptr, *vt = nullptr, *vc = nullptr;
			u32 *idx = nullptr;

			int free_index;
			bool found_resource = FindResource(renderer->mesh_resources, desc.name, free_index);

			if (found_resource) {
				return free_index;
			}


			Mesh::Data mesh;

			if(!desc.empty_mesh) {
				if (desc.indices_n > 0 && desc.indices && desc.vertices_n > 0 && desc.positions) {
					vp = (f32*)desc.positions;
					idx = (u32*)desc.indices;

					if (desc.normals)
						vn = (f32*)desc.normals;
					if (desc.texcoords)
						vt = (f32*)desc.texcoords;
					if (desc.colors)
						vc = (f32*)desc.colors;
					// Create empty mesh (i.e. only a vao, used to create the text vao for example)
				}
				else {
					// TODO : Here we allow mesh creation without data
					// It is used for TextMeshes VAO, so for now, no error
					LogErr("Tried to create a Mesh without giving indices or positions array.");
					return -1;
				}
			}


			mesh.vertices_n = desc.vertices_n;
			mesh.indices_n = desc.indices_n;

			glGenVertexArrays(1, &mesh.vao);
			// Disallow 0-Vao. If given VAO with index 0, ask for another one
			// this should never happen because VAO-0 is already constructed for the text
			if (!mesh.vao) glGenVertexArrays(1, &mesh.vao);
			glBindVertexArray(mesh.vao);

			if (vp) {
				mesh.attrib_flags = MESH_POSITIONS;
				glEnableVertexAttribArray(0);

				// fill position buffer
				glGenBuffers(1, &mesh.vbo[0]);
				glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo[0]);
				glBufferData(GL_ARRAY_BUFFER, mesh.vertices_n * sizeof(vec3f),
							 vp, GL_STATIC_DRAW);

				// add it to the vao
				glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (GLubyte*)NULL);
			}

			// Check for indices
			if(idx) {
				mesh.attrib_flags |= MESH_INDICES;
				glGenBuffers(1, &mesh.ibo);
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ibo);
				glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.indices_n * sizeof(u32),
							 idx, GL_STATIC_DRAW);
			}

			// check for normals
			if (vn) {
				mesh.attrib_flags |= MESH_NORMALS;
				glEnableVertexAttribArray(1);

				// fill normal buffer
				glGenBuffers(1, &mesh.vbo[1]);
				glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo[1]);
				glBufferData(GL_ARRAY_BUFFER, mesh.vertices_n * sizeof(vec3f),
							 vn, GL_STATIC_DRAW);

				// add it to vao
				glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (GLubyte*)NULL);
			}

			// check for texcoords
			if (vt) {
				mesh.attrib_flags |= MESH_TEXCOORDS;
				glEnableVertexAttribArray(2);

				// fill normal buffer
				glGenBuffers(1, &mesh.vbo[2]);
				glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo[2]);
				glBufferData(GL_ARRAY_BUFFER, mesh.vertices_n * sizeof(vec2f),
							 vt, GL_STATIC_DRAW);

				// add it to vao
				glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, (GLubyte*)NULL);
			}

			// check for colors
			if (vc) {
				mesh.attrib_flags |= MESH_COLORS;
				glEnableVertexAttribArray(3);

				// fill normal buffer
				glGenBuffers(1, &mesh.vbo[3]);
				glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo[3]);
				glBufferData(GL_ARRAY_BUFFER, mesh.vertices_n * sizeof(vec4f),
							 vc, GL_STATIC_DRAW);

				// add it to vao
				glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, 0, (GLubyte*)NULL);
			}

			glBindVertexArray(0);

			int mesh_i = (int)renderer->meshes.size();
			renderer->meshes.push_back(mesh);

			// Add created mesh to renderer resources
			AddResource(renderer->mesh_resources, free_index, desc.name, mesh_i);

			return mesh_i;
		}

		Handle BuildSphere() {
			// Test resource existence before recreating it
			{
				Handle h;
				if(Exists("Sphere1", h)) {
					return h;
				}
			}

			const f32 radius = 1.f;
			const u32 nLon = 32, nLat = 24;

			const u32 nVerts = (nLon+1) * nLat + 2;
			const u32 nTriangles = nVerts * 2;
			const u32 nIndices = nTriangles * 3;

			// Positions
			vec3f pos[nVerts];
			pos[0] = vec3f(0, 1, 0) * radius;
			for(u32 lat = 0; lat < nLat; ++lat) {
				f32 a1 = M_PI * (f32)(lat + 1) / (nLat + 1);
				f32 sin1 = std::sin(a1);
				f32 cos1 = std::cos(a1);

				for(u32 lon = 0; lon <= nLon; ++lon) {
					f32 a2 = M_TWO_PI * (f32)(lon == nLon ? 0 : lon) / nLon;
					f32 sin2 = std::sin(a2);
					f32 cos2 = std::cos(a2);

					pos[lon + lat * (nLon + 1) + 1] = vec3f(sin1 * cos2, cos1, sin1 * sin2) * radius;
				}
			}
			pos[nVerts - 1] = vec3f(0,1,0) * -radius;

			// Normals
			vec3f nrm[nVerts];
			for(u32 i = 0; i < nVerts; ++i) {
				nrm[i] = pos[i];
				nrm[i].Normalize();
			}

			// UVs
			vec2f uv[nVerts];
			for(u32 i = 0; i < nVerts; ++i) {
				uv[i] = vec2f(0,0);	// TODO
			}

			// Triangles/Indices
			u32 indices[nIndices];
			{
				// top
				u32 i = 0;
				for(u32 lon = 0; lon < nLon; ++lon) {
					indices[i++]  = lon + 2;
					indices[i++]  = lon + 1;
					indices[i++]  = 0;
				}

				// middle
				for(u32 lat = 0; lat < nLat - 1; ++lat) {
					for(u32 lon = 0; lon < nLon; ++lon) {
						u32 curr = lon + lat * (nLon + 1) + 1;
						u32 next = curr + nLon + 1;

						indices[i++] = curr;
						indices[i++] = curr + 1;
						indices[i++] = next + 1;

						indices[i++] = curr;
						indices[i++] = next + 1;
						indices[i++] = next;
					}
				}

				// bottom
				for(u32 lon = 0; lon < nLon; ++lon) {
					indices[i++] = nVerts - 1;
					indices[i++] = nVerts - (lon + 2) - 1;
					indices[i++] = nVerts - (lon + 1) - 1;
				}
			}

			Desc desc("Sphere1", false, nIndices, indices, nVerts, (f32*)pos, (f32*)nrm, (f32*)uv);
			return Build(desc);
		}

		void Destroy(Handle h) {
			if (Exists(h)) {
				Data &mesh = renderer->meshes[h];
				glDeleteBuffers(4, mesh.vbo);
				glDeleteBuffers(1, &mesh.ibo);
				glDeleteVertexArrays(1, &mesh.vao);
				mesh.attrib_flags = MESH_POSITIONS;
				mesh.vertices_n = 0;
				mesh.indices_n = 0;


				// Remove it as a loaded resource
				for (u32 i = 0; i < renderer->mesh_resources.size(); ++i)
					if ((int)renderer->mesh_resources[i].handle == h) {
						renderer->mesh_resources[i].handle = -1;
						renderer->mesh_resources[i].name = "";
					}


				mesh.animation_n = 0;
			}
		}

		void Bind(Handle h) {
			GLint vao = Exists(h) ? h : -1;

			if (renderer->curr_GL_vao != vao) {
				renderer->curr_GL_vao = vao;
				glBindVertexArray(vao >= 0 ? (int)renderer->meshes[vao].vao : -1);
			}
		}

		void Render(Handle h, const AnimState &state) {
			if (Exists(h)) {
				const Mesh::Data &md = renderer->meshes[h];

				Bind(h);
				glDrawElements(GL_TRIANGLES, md.indices_n, GL_UNSIGNED_INT, 0);
			}
				//if (state && state.type > ANIM_NONE) {
				//}
				//else {
				// identity bone matrix for meshes having no mesh animation
				//mat4 identity;
				//mat4_identity(identity);
				//shader_send_mat4(UNIFORM_BONEMATRIX0, identity);
				//}
		}


		bool Exists(Handle h) {
			return h >= 0 && h < (int)renderer->meshes.size() && renderer->meshes[h].vao > 0;
		}
		
		bool Exists(const std::string &resourceName, Handle &res) {
			return FindResource(renderer->mesh_resources, resourceName, res, false);
		}

		void SetAnimation(Handle h, AnimState &state, AnimType type) {
			if (Exists(h)) {
				state.curr_frame = 0;
				state.curr_time = 0.f;
				state.type = type;

				if (type > ANIM_NONE) {
					state.duration = renderer->meshes[h].animations[type - 1].duration;
					UpdateAnimation(h, state, 0.f);
				}
			}
		}

		void UpdateAnimation(Handle h, AnimState &state, float dt) {
			mat4f root_mat;
			root_mat.Identity();
			root_mat = root_mat.RotateX(-M_PI_OVER_TWO);	// rotate root by 90X for left-handedness

			if (Exists(h)) {
				const _Animation &anim = renderer->meshes[h].animations[state.type - 1];
				state.curr_time += dt;

				if (anim.used && state.curr_time > anim.frame_duration[state.curr_frame + 1]) {
					++state.curr_frame;
					if (state.curr_frame == anim.frame_n - 1) {
						state.curr_frame = 0;
						state.curr_time = 0.f;
					}
				}
			}
		}

	}



	namespace TextMesh {
		Handle Build(Desc &desc) {
			// Create a new textmesh with the
			Handle tmesh_handle = SetString(-1, desc.font_handle, desc.string);
			return tmesh_handle;
		}

		Handle SetString(Handle h, Font::Handle fh, const std::string &str) {
			if (!Font::Exists(fh)) {
				LogErr("Font handle ", fh, " is not a valid renderer resource.");
				return false;
			}

			bool updated = true;        // true if this is an updated of an existing text
			TextMesh::Data *dst;// = renderer->textmeshes[0];	// temporary reference

			if (h >= 0) {
				Assert(h < (int)renderer->textmeshes.size());
				dst = &renderer->textmeshes[h];
			}
			else {
				updated = false;
				renderer->textmeshes.push_back(TextMesh::Data());
				dst = &renderer->textmeshes.back();
				*dst = TextMesh::Data();
			}

			if (!CreateFromString(*dst, renderer->fonts[fh], str)) {
				LogErr("Error creating TextMesh from given string.");
				return -1;
			}

			return updated ? h : (Handle)(renderer->textmeshes.size()-1);
		}

		bool Exists(Handle h) {
			return h >= 0 && h < (int)renderer->textmeshes.size() && renderer->textmeshes[h].vbo > 0;
		}

		void Destroy(Handle h) {
			if (Exists(h)) {
				Data &textmesh = renderer->textmeshes[h];
				glDeleteBuffers(1, &textmesh.vbo);
				textmesh.vbo = 0;
			}
		}

		void Bind(Handle h) {
			if (Exists(h)) {
				Data &textmesh = renderer->textmeshes[h];

				glBindBuffer(GL_ARRAY_BUFFER, textmesh.vbo);
				glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (GLvoid*)NULL);
				glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0,
					(GLvoid*)(textmesh.vertices_n * 2 * sizeof(f32)));
				glEnableVertexAttribArray(0);   // enable positions
				glDisableVertexAttribArray(1);  // no normals
				glEnableVertexAttribArray(2);   // enable texcoords
			}
		}

		void Render(Handle h) {
			Bind(h);
			glDrawArrays(GL_TRIANGLES, 0, renderer->textmeshes[h].vertices_n);
		}
	}



	namespace SpriteSheet {
		Handle LoadFromFile(const std::string &filename) {
			Json ss_json;
			if (!ss_json.Open(filename)) {
				LogErr("Error loading spritesheet json file.");
				return -1;
			}

			SpriteSheet::Data ss;

			ss.file_name = filename;
			ss.resource_name = Json::ReadString(ss_json.root, "name", "");
			std::string tex_file = Json::ReadString(ss_json.root, "texture", "");

			// try to find the resource
			int free_index;
			bool found_resource = FindResource(renderer->spritesheets_resources,
				ss.resource_name, free_index);

			if (found_resource) {
				return free_index;
			}

			// try to load the texture resource or get it if it exists
			Texture::Desc tdesc(tex_file);
			ss.sheet_texture = Texture::Build(tdesc);

			if (ss.sheet_texture == -1) {
				LogErr("Error loading texture file during spritesheet creation.");
				return -1;
			}

			cJSON *sprite_arr = cJSON_GetObjectItem(ss_json.root, "sprites");
			if (sprite_arr) {
				int sprite_count = cJSON_GetArraySize(sprite_arr);
				for (int i = 0; i < sprite_count; ++i) {
					Sprite sprite;
					cJSON *sprite_root = cJSON_GetArrayItem(sprite_arr, i);
					if (cJSON_GetArraySize(sprite_root) != 5) {
						LogErr("A Sprite definition in a spritesheet must have 5 fields : x y w h name.");
						continue;
					}
					sprite.geometry.x = cJSON_GetArrayItem(sprite_root, 0)->valueint;
					sprite.geometry.y = cJSON_GetArrayItem(sprite_root, 1)->valueint;
					sprite.geometry.w = cJSON_GetArrayItem(sprite_root, 2)->valueint;
					sprite.geometry.h = cJSON_GetArrayItem(sprite_root, 3)->valueint;
					sprite.name = cJSON_GetArrayItem(sprite_root, 4)->valuestring;

					ss.sprites.push_back(sprite);
				}
			}

			Handle ss_i = renderer->spritesheets.size();
			renderer->spritesheets.push_back(ss);

			AddResource(renderer->spritesheets_resources, free_index, ss.resource_name, ss_i);

			return ss_i;
		}

		void Destroy(Handle h) {
			if (h >= 0 && h < (int)renderer->spritesheets.size()) {
				Texture::Destroy(renderer->spritesheets[h].sheet_texture);
				renderer->spritesheets[h].sprites.clear();
			}
		}
	}
}
