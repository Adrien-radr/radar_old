#pragma once

#include "GL/glew.h"

namespace Render {
	namespace Texture {
		enum Type {
			FromFile,	// Normal loading from disk file. Checks if the file is already loaded as resource each time
			Empty,		// Create empty texture (e.g. used by FBOs)
			Cubemap		// Loads 6 cubemap faces from the 6 given disk files (which each is a FromFile resource)
		};

		struct FormatDesc {
			// FormatDesc() : 
				// blockSize(0), bpp(0), component(0), formatGL(GL_NONE), formatInternalGL(0), type(GL_NONE) {}
			u32 	blockSize;
			u32 	bpp;
			u32 	component;
			GLint 	formatInternalGL;
			GLenum	formatGL;
			GLenum  type;
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

		FormatDesc GetTextureFormat(TextureFormat fmt);


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

		namespace _internal {
			/// Intermediate format for texture loading
			/// Do not use unless you know why/how
			struct _tex {
				_tex() : width(0), height(0), id(0), format(FMT_UNKNOWN), mipmapCount(0), texels(NULL) {}

				u32         	width,
					height;

				GLuint      	id;

				TextureFormat 	format;
				FormatDesc		desc;
				GLuint			mipmapCount;

				GLubyte    		*texels;
			};

			/// Format for a loaded GL texture
			struct Data {
				Data() : id(0) {}

				u32     id;             //!< GL Texture ID
				vec2i   size;           //!< Texture resolution in texels
			};

			/// Loads a PNG image with libpng.
			/// This function allocates a _tex structure. Caller must free it
			/// once done with it
			/// @param filename : Filename of PNG file on disk
			/// @return : The allocated and filled texture
			_tex *LoadPNG(const std::string &filename);

			/// Same for DDS image file
			_tex *LoadDDS(const std::string &filename);

			/// Loads an image file. Determine the format internally
			/// @param texture : target. Image will be loaded in here
			/// @param filename : image file on disk
			/// @return : true if successful. false if error occured.
			bool Load(Data &texture, const std::string &filename);

			/// Loads an image as a given cubemap face.
			/// A Cubemap Texture should be bound before calling that.
			/// This should only be called from Texture::Build
			bool LoadCubemapFace(const std::string &filename, u32 face);
		}
	}

	struct Sprite {
		std::string name;
		Rect geometry;
	};


	namespace SpriteSheet {
		/// Spritesheet Handle.
		/// Spritesheets are stored and worked on internally by the renderer.
		/// Outside of render.c, Spritesheets are referred by those handles
		typedef int Handle;

		/// Loads a spritesheet and all its sprites from a JSON spritesheet
		/// file. Returns a handle to the spritesheet
		Handle LoadFromFile(const std::string &filename);
		void Destroy(Handle h);

		namespace _internal {
			struct Data {
				std::string resource_name;
				std::string file_name;
				Texture::Handle sheet_texture;

				std::vector<Sprite> sprites;
			};
		}
	}
}
