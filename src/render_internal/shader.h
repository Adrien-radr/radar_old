#pragma once
#include "common/common.h"

// Renderer constants
#define SHADER_MAX_UNIFORMS 64
#define SHADER_MAX_ATTRIBUTES 7

namespace Render
{
	namespace Shader
	{
		/*
		/// Available shaders for drawing, sorted by their projection matrix type
		enum Type
		{
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
		*/
		enum StaticShaders
		{
			SHADER_SKYBOX = 0,
		};

		enum ProjectionType
		{
			PROJECTION_NONE,
			PROJECTION_2D,
			PROJECTION_3D
		};

		/// Uniform Locations Descriptors
		/// When building a shader, we extract and store the uniform locations
		/// corresponding to a in-shader uniform name.
		/// When sending a uniform value to a shader from C code, use these
		/// uniform descriptor to specify which uniform the value is intended for.
		enum Uniform
		{
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
		enum UniformBlock
		{
			UNIFORMBLOCK_MATERIAL,
			UNIFORMBLOCK_LIGHTTYPE0,	// usually Point Light
			UNIFORMBLOCK_LIGHTTYPE1,
			UNIFORMBLOCK_LIGHTTYPE2,

			_UNIFORMBLOCK_N				// Do not use 
		};

		/// Shader Descriptor for shader building.
		struct Desc
		{
			Desc() : vertex_file( "" ), fragment_file( "" ), vertex_src( "" ), fragment_src( "" ), 
					 projType( PROJECTION_NONE ), shaderSlot( -1 ), attrib_idx(0) {}

			void AddAttrib(const std::string &name, int loc);
			void AddUniform(const std::string &name, Shader::Uniform u);

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
			struct Attrib
			{
				Attrib() : used( false ), name( "" ) {}
				Attrib( const std::string &n, u32 l )
					: used( true ), name( n ), location( l )
				{}

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
			struct Uniform
			{
				Uniform() : name( "" ) {}
				Uniform( const std::string &n, Shader::Uniform d )
					: name( n ), desc( d )
				{}

				std::string		name;
				Shader::Uniform	desc;
			};

			struct UniformBlock
			{
				UniformBlock( const std::string &n, Shader::UniformBlock d )
					: name( n ), desc( d )
				{}
				std::string 			name;
				Shader::UniformBlock 	desc;
			};

			std::vector<Uniform> uniforms;
			std::vector<UniformBlock> uniformblocks;

			std::vector<u32> linkedLibraries;

			ProjectionType projType;

			int shaderSlot;

		private:
			u32 attrib_idx;
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
		Handle Build( const Desc &desc );

		/// Build a vertex or fragment shader from source depending on type
		u32 /*GLuint*/ BuildShader( const char *src, int /*GLenum*/ type );

		/// Deallocate GL data
		void Destroy( Handle h );

		/// Bind the given Shader Program as Current GL Program
		/// if @param shader is -1, all GL shader programs will be unbound
		void Bind( Handle h );

		/// Returns true if the given shader exists in renderer
		bool Exists( Handle h );

		// Function Collection to send uniform variables to currently bound GL shader
		void SendVec2( Uniform target, vec2f value );
		void SendVec3( Uniform target, vec3f value );
		void SendVec4( Uniform target, vec4f value );
		//void SendMat3(Uniform target, mat3 value);
		void SendMat4( Uniform target, mat4f value );
		void SendInt( Uniform target, int value );
		void SendFloat( Uniform target, f32 value );

		// Same but by designating them as strings (less performant)
		void SendVec2( const std::string &target, vec2f value );
		void SendVec3( const std::string &target, vec3f value );
		void SendVec4( const std::string &target, vec4f value );
		//void SendMat3(const std::string &target, mat3f value);
		void SendMat4( const std::string &target, mat4f value );
		void SendInt( const std::string &target, int value );
		void SendFloat( const std::string &target, f32 value );

		namespace _internal
		{
			struct Data
			{
				Data() : id( 0 ) {}
				u32 id;                             //!< GL Shader Program ID
				// bool proj_matrix_type;              //!< Type of projection matrix used in shader

				// Arrays of uniforms{blocks}, ordered as the Shader::Uniform{Block} enum
				GLint  uniform_locations[Shader::_UNIFORM_N];   //!< Locations for the possible uniforms
				GLuint uniformblock_locations[Shader::_UNIFORMBLOCK_N];
			};
		}
	}

	namespace UBO
	{
		enum StorageType
		{
			ST_STATIC,
			ST_DYNAMIC
		};

		struct Desc
		{
			Desc( f32 *data, u32 data_size, StorageType t ) : sType( t ), data( data ), size( data_size ) {}

			StorageType sType;
			f32 *data;			//!< pointer on the data to register in the UBO
			u32 size;			//!< size in bytes of this data
		};

		typedef int Handle;

		Handle Build( const Desc &desc );
		void Update( Handle h, const Desc &desc );
		void Destroy( Handle h );
		void Bind( Shader::UniformBlock loc, Handle h );
		bool Exists( Handle h );

		namespace _internal
		{
			struct Data
			{
				Data() : id( 0 ) {}
				u32 id;
			};
		}
	}
}