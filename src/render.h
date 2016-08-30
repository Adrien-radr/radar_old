#pragma once

#include "common/common.h"

// Renderer constants
#define SHADER_MAX_UNIFORMS 64
#define SHADER_MAX_ATTRIBUTES 6

namespace Render {
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
	void UpdateView(const mat4f &viewMatrix, const vec3f &eyePos);

	u32 GetFramebufferTextureID(u32 framebufferIdx, u32 textureIdx);

	//bool FindResource(const std::vector<RenderResource> &resources, const )

	namespace Shader {
		/// Available shaders for drawing, sorted by their projection matrix type
		enum Type {
            // Use 2D Ortographic Projection
            SHADER_2D_UI = 0,

            // Use 3D Perspective Projection
            SHADER_3D_MESH = 1,
			SHADER_GBUFFERPASS = 2,
			SHADER_SKYBOX = 3,
            //SHADER_3D_MESH_BONE = 4,
            //SHADER_3D_HEIGHTFIELD = 5,

            _SHADER_END,                                    // Do not use


            _SHADER_2D_PROJECTION_START = SHADER_2D_UI,     // Do not use
            _SHADER_2D_PROJECTION_END = SHADER_3D_MESH,     // Do not use
            _SHADER_3D_PROJECTION_START = SHADER_3D_MESH,   // Do not use
            _SHADER_3D_PROJECTION_END = _SHADER_END         // Do not use
		};

		/// Uniform Locations Descriptors
		/// When building a shader, we extract and store the uniform locations
		/// corresponding to a in-shader uniform name.
		/// When sending a uniform value to a shader from C code, use these
		/// uniform descriptor to specify which uniform the value is intended for.
		enum Uniform {
			UNIFORM_PROJMATRIX,         // for "ProjMatrix", mat4
			UNIFORM_VIEWMATRIX,         // for "ViewMatrix", mat4
			UNIFORM_MODELMATRIX,        // for "ModelMatrix", mat4

			UNIFORM_TEXTURE0,           // Fragment Uniform, for "tex0", int	Usually diffuse texture
			UNIFORM_TEXTURE1,           // Fragment Uniform, for "tex1", int	Usually specular texture
			UNIFORM_TEXTURE2,           // Fragment Uniform, for "tex2", int	Usually normal texture
			UNIFORM_TEXTURE3,           // Fragment Uniform, for "tex2", int	Usually AO texture

			UNIFORM_TEXTURE4,
			UNIFORM_TEXTURE5,

			UNIFORM_TEXTCOLOR,          // Fragment Uniform, for "text_color", vec4

			UNIFORM_EYEPOS,
			UNIFORM_NPOINTLIGHTS,
			UNIFORM_NAREALIGHTS,
			UNIFORM_GROUNDTRUTH,
			UNIFORM_GLOBALTIME,
			UNIFORM_OBJECTID,

			_UNIFORM_N                   // Do not use
		};

		// Uniform Block Locations Descriptors
		enum UniformBlock {
			UNIFORMBLOCK_MATERIAL,
			UNIFORMBLOCK_POINTLIGHTS,
			UNIFORMBLOCK_AREALIGHTS,

			_UNIFORMBLOCK_N				// Do not use 
		};

		/// Shader Descriptor for shader building.
		struct Desc {
			Desc() : vertex_file(""), fragment_file(""), vertex_src(""), fragment_src(""), shaderSlot(-1) {}

			/// Shaders are loaded either from file or from text source directly
			/// If loading from file, vertex_file & fragment_file must be set
			std::string vertex_file, fragment_file;
			/// If loading from source, vertex_src & fragment_src must be set
			std::string vertex_src, fragment_src;

			/// Attribute Locations in the shader.
			/// Fill this array with as much (<SHADER_MAX_ATTRIBUTES) attribs before
			/// calling shader_build[FromFile]
			/// @param used must be set to true to activate one attribute
			/// @param name must be set to the in-shader name of the attribute
			/// @param location must be set to the desired location for this attrib
			struct Attrib {
				Attrib() : used(false), name("") {}
				Attrib(const std::string &n, u32 l)
					: used(true), name(n), location(l) {}

				bool		used;
				std::string name;
				u32			location;
			} attribs[SHADER_MAX_ATTRIBUTES];

			/// Uniform Locations in the shader.
			/// Fill this array with as much (<SHADER_MAX_UNIFORMS) uniforms before
			/// calling shader_build[FromFile]
			/// @param used must be set to true to activate one uniform
			/// @param name must be given (name of the uniform in shader)
			/// @param desc is the wanted uniform descriptor
			///     ex: name = "ProjMatrix", and desc = UNIFORM_PROJMATRIX
			struct Uniform {
				Uniform() : name("") {}
				Uniform(const std::string &n, Shader::Uniform d)
					: name(n), desc(d) {}

				std::string		name;
				Shader::Uniform	desc;
			};

			struct UniformBlock {
				UniformBlock(const std::string &n, Shader::UniformBlock d)
					: name(n), desc(d) {}
				std::string 			name;
				Shader::UniformBlock 	desc;
			};

			std::vector<Uniform> uniforms;
			std::vector<UniformBlock> uniformblocks;

			std::vector<u32> linkedLibraries;

			int shaderSlot;
		};

		/// Shader Handle.
		/// Shaders are stored and worked on internally.
		/// Outside of render.c, Shaders are referred by those handles
		typedef int Handle;

		/// Build a GL Shader Program from one Vertex Shader and one Fragment Shader, from files
		//bool shader_buildFromFile(Shader *shader, char const *v_src, char const *f_src);

		/// Build a GL Shader Program from a Shader Description.
		/// Read the ShaderDesc comments above to understand what to give to this function
		/// Returns the handle of the created shader. Or -1 if an error occured.
		Handle Build(const Desc &desc);

		/// Build a vertex or fragment shader from source depending on type
		u32 /*GLuint*/ BuildShader(const char *src, int /*GLenum*/ type);

		/// Deallocate GL data
		void Destroy(Handle h);

		/// Bind the given Shader Program as Current GL Program
		/// if @param shader is -1, all GL shader programs will be unbound
		void Bind(Handle h);

		/// Returns true if the given shader exists in renderer
		bool Exists(Handle h);

		// Function Collection to send uniform variables to currently bound GL shader
		void SendVec2(Uniform target, vec2f value);
		void SendVec3(Uniform target, vec3f value);
		void SendVec4(Uniform target, vec4f value);
		//void SendMat3(Uniform target, mat3 value);
		void SendMat4(Uniform target, mat4f value);
		void SendInt(Uniform target, int value);
		void SendFloat(Uniform target, f32 value);

		// Same but by designating them as strings (less performant)
		void SendVec2(const std::string &target, vec2f value);
		void SendVec3(const std::string &target, vec3f value);
		void SendVec4(const std::string &target, vec4f value);
		//void SendMat3(const std::string &target, mat3f value);
		void SendMat4(const std::string &target, mat4f value);
		void SendInt(const std::string &target, int value);
		void SendFloat(const std::string &target, f32 value);
	}

	namespace UBO {
		enum StorageType {
			ST_STATIC,
			ST_DYNAMIC
		};

		struct Desc {
			Desc(f32 *data, u32 data_size, StorageType t) : sType(t), data(data), size(data_size) {}

			StorageType sType;
			f32 *data;			//!< pointer on the data to register in the UBO
			u32 size;			//!< size in bytes of this data
		};

		typedef int Handle;

		Handle Build(const Desc &desc);
		void Update(Handle h, const Desc &desc);
		void Destroy(Handle h);
		void Bind(Shader::UniformBlock loc, Handle h);
		bool Exists(Handle h);
	}

	namespace Texture {
		enum Type {
			FromFile,	// Normal loading from disk file. Checks if the file is already loaded as resource each time
			Empty,		// Create empty texture (e.g. used by FBOs)
			Cubemap		// Loads 6 cubemap faces from the 6 given disk files (which each is a FromFile resource)
		};

		/// Descriptor for Texture creation
		/// If texture_file is given, load texture from given file (supported: PNG)
		/// Else, return an empty texture of the given size, registered in the renderer,
		/// for use elsewhere
		struct Desc {
			Desc(const std::string &tname = "") : type(FromFile) {
				name[0] = tname;
			}
			std::string name[6];	//!< Texture name
			vec2i size;         	//!< Used only if texture_file is not given
			Type type;				//!< Determines how the texture is loaded
		};

		enum Target {
			TARGET0 = 0,		// Usually Diffuse Texture
			TARGET1 = 1,		// Usually Specular Texture
			TARGET2 = 2,		// Usually Normal Texture
			TARGET3 = 3,		// Usually Occlusion(AO) Texture

			// Additional misc slots
			TARGET4 = 4,
			TARGET5 = 5,

			_TARGET_N
		};

		enum TextureFormat {
			FMT_UNKNOWN,

			R8U,
			RG8U,
			RGB8U,
			RGBA8U,

			R16F,
			RG16F,
			RGB16F,
			RGBA16F,

			R32F,
			RG32F,
			RGB32F,
			RGBA32F,

			DEPTH16F,
			DEPTH32F,

			DXT1,
			DXT3,
			DXT5,

			_FORMAT_N
		};

		/// Texture Handle
		/// Textures are stored and worked on internally by the renderer.
		/// Outside of render.c, Textures are referred by those handles
		typedef int Handle;

		/// Build a Texture from the given Texture Descriptor. See the above TextureDesc.
		/// If asked to load from a texture file(PNG,..), see first if already loaded.
		/// If not, load it, store it, and return it.
		/// @return : the Texture Handle if successful. -1 otherwise
		Handle Build(const Desc &desc);

		/// Deallocate GL data for the given Texture
		void Destroy(Handle h);

		/// Bind given Texture as current target's texture
		/// If -1 is given, unbind all textures for the given target
		void Bind(Handle h, Target t);
		void BindCubemap(Handle h, Target t);

		/// Returns true if the given texture exists in renderer
		bool Exists(Handle h);

		/// Returns the OpenGL texture ID of the given texture
		u32 GetGLID(Handle h);

		/// Retrieve data from the given texture
		/// WARN : dst must be allocated to the right size
		/// Returns false if the operation failed
		bool GetData(Handle h, TextureFormat fmt, f32 *dst);

		// Default 1x1 textures
		extern Handle DEFAULT_DIFFUSE;	// white for diffuse
		extern Handle DEFAULT_NORMAL;	// (127, 255, 127) for normal
	}


	namespace FBO {
		enum GBufferAttachment {
			OBJECTID,
			DEPTH,
			NORMAL,
			WORLDPOS,
			_ATTACHMENT_N
		};

		struct Desc {
			std::vector<Texture::TextureFormat> textures;	//!< Ordered texture attachments
			vec2i size;
		};

		struct Data {
			u32 framebuffer;							//!< FB handle
			u32 depthbuffer;							//!< Associated Depth Render Buffer
			std::vector<Texture::Handle> attachments;	//!< Associated textures, in attachment order
		};	

		typedef int Handle;

		Handle Build(Desc d);
		void Destroy(Handle h);
		void Bind(Handle h);
		bool Exists(Handle h);
		// void BindTexture(Handle h);

		/// Returns the OpenGL Texture ID of the given attachment of the main GBuffer (FBO0)
		u32 GetGBufferAttachment(GBufferAttachment idx);

		/// Returns the string literal name of the given attachment
		const char *GetGBufferAttachmentName(GBufferAttachment idx);

		/// General GBuffer query. Query only 1 texel value of the idx attachment.
		vec4f ReadGBuffer(GBufferAttachment idx, int x, int y);

		/// GBuffer picking : returns the object ID & Vertex ID under the given position (x,y)
		vec2i ReadVertexID(int x, int y);
	};



	namespace Font {
		struct Desc {
			Desc(const std::string &fname = "", u32 fsize = 0) :
				name(fname), size(fsize) {}

			std::string name; //!< TTF font file
			u32			size; //!< Wanted font size
		};

		/// Font Handle
		/// Fonts are stored and worked on internally by the renderer.
		/// Outside of render.c, Fonts are referred by those handles
		typedef int Handle;

		/// Build a Font Atlas from a TTF font file, at the wanted size.
		Handle Build(Desc &desc);

		// Free all font data allocated by font_build
		void Destroy(Handle h);

		/// Returns true if the given font exists in the renderer
		bool Exists(Handle h);

		/// Binds the font's atlas texture as current texture for given target
		void Bind(Handle h, Texture::Target target);

	}

	namespace Mesh {
		enum Attribute {
			MESH_POSITIONS = 1,
			MESH_NORMALS = 2,
			MESH_TEXCOORDS = 4,
			MESH_TANGENT = 8,
			MESH_BINORMAL = 16,
			MESH_COLORS = 32,
			MESH_INDICES = 64
		};

		enum AnimType {
			ANIM_NONE,      // static mesh, no bone animation
			ANIM_IDLE,

			ANIM_N // Do not Use
		};

		struct AnimState {
			int curr_frame;
			f32 curr_time;
			f32 duration;

			AnimType type;
		};


		/// Mesh Description
		/// param vertices_n : number of vertices the mesh has.    
		/// @param positions : array of vertex positions.          
		/// @param normals : array of vertex normals.
		/// @param texcoords : array of vertex texture UV coords
		/// @param colors : array of vertex colors.
		struct Desc {
			Desc(const std::string &resource_name, bool empty_mesh, u32 icount, u32 *idx_arr,
				u32 vcount, f32 *pos_arr, f32 *normal_arr = nullptr, f32 *texcoord_arr = nullptr,
				f32 *tangent_arr = nullptr, f32 *bitangent_arr = nullptr, f32 *col_arr = nullptr) :
				name(resource_name), empty_mesh(empty_mesh), vertices_n(vcount), indices_n(icount),
				indices(idx_arr), positions(pos_arr), normals(normal_arr), texcoords(texcoord_arr),
				tangents(tangent_arr), bitangents(bitangent_arr), colors(col_arr) {}


			std::string name;	//!< name of the mesh for resource managment
			bool empty_mesh;

			u32 vertices_n;		//!< number of vertices the mesh has
			u32 indices_n;		//!< number of indices

			u32 *indices;

			f32 *positions;     //!< format vec3
			f32 *normals;       //!< format vec3
			f32 *texcoords;     //!< format vec2
			f32 *tangents;		//!< format vec3
			f32 *bitangents;	//!< format vec3
			f32 *colors;        //!< format vec4
		};

		/// Mesh Handle.
		/// Meshes are stored and worked on internally by the renderer.
		/// Outside of render.c, Meshes are referred by those handles
		typedef int Handle;

		/// Build a Mesh from the given Mesh Description. See above to understand what is needed.
		/// @return : the Mesh Handle if creation successful. -1 if error occured.
		Handle Build(const Desc &desc);

		/// Build a radius 1 sphere
		Handle BuildSphere();

		/// Build a radius 1 box
		Handle BuildBox();

		/// Build a SH visualization mesh
		/// @param shNormalization : put to true if the sh visualization should be normalized by DC
		Handle BuildSHVisualization(const float *shCoeffs, const u32 bandN, const std::string &meshName, bool shNormalization, const u32 numPhi = 48, const u32 numTheta = 96);

		/// Deallocate GL data for the given mesh handle
		void Destroy(Handle h);

		/// Bind given Mesh as current VAO for drawing
		/// If -1 is given, unbind all VAOs
		void Bind(Handle h);

		/// Returns true if the given mesh exists in renderer
		bool Exists(Handle h);
		bool Exists(const std::string &resourceName, Handle &h); // returns the resource in h if it exists as a resource

		/// Renders the given mesh, binding it if not currently bound as GL Current VAO.
		/// The given animation state is used to transmit bone-matrix data to the shader
		/// before drawing the mesh. If NULL, an identity bonematrix is used
		void Render(Handle h);

		/// Sets the current played animation of state. Reset it at the beginning of 1st frame
		// void SetAnimation(Handle h, AnimState &state, AnimType type);

		/// Update the given animation state so that it continues playing its current animation
		/// with the added delta time, thus updating its bone matrices
		/// @param state : the animation to update
		/// @param mesh_i : the mesh resource handle from which the animation is taken
		/// @param dt : delta time to advance animation
		// void UpdateAnimation(Handle h, AnimState &state, float dt);
	}

	namespace TextMesh {
		struct Desc {
			Desc(Font::Handle font, const std::string &str, const vec4f &strcolor) :
				font_handle(font), string(str), color(strcolor) {}

			Font::Handle font_handle;
			std::string string;
			vec4f color;
		};

		/// TextMesh Handle.
		/// TextMeshes are stored and worked on internally by the renderer.
		/// Outside of render.c, TextMeshes are referred by those handles
		typedef int Handle;

		// Creates the TextMesh with the given descriptor
		Handle Build(Desc &desc);

		/// Deallocate GL data for the given textmesh handle
		void Destroy(Handle h);

		/// Binds the given textmesh's VBO as current
		void Bind(Handle h);

		/// Render the given textmesh. Binds it if not currently bound
		void Render(Handle h);

		/// Returns true if the given mesh exists in renderer
		bool Exists(Handle h);

		/// Change the string displayed by a TextMesh
		/// @param h : handle of the text mesh to be modified. -1 for a new handle
		/// @param mesh : Handle of the used font
		/// @param str : new string to be displayed
		/// @return : h if it is >= 0, or a new handle if h is -1
		Handle SetString(Handle h, Font::Handle fh, const std::string &str);
	}

	namespace SpriteSheet {
		/// Spritesheet Handle.
		/// Spritesheets are stored and worked on internally by the renderer.
		/// Outside of render.c, Spritesheets are referred by those handles
		typedef int Handle;

		/// Loads a spritesheet and all its sprites from a JSON spritesheet
		/// file. Returns a handle to the spritesheet
		Handle LoadFromFile(const std::string &filename);
		void Destroy(Handle h);
	}
}
