#pragma once

/// IMPORTANT !!!!!!!!!!!!1
/// This file is intended to be included in render.c and optionnally
/// in render_internal/*.c files, not elsewhere !

#include "../render.h"
#include "GL/glew.h"

namespace Render {
	namespace Texture {
		enum TextureFormat {
			FMT_UNKNOWN,

			R8U,
			RG8U,
			RGB8U,
			RGBA8U,

			R32F,
			RG32F,
			RGB32F,
			RGBA32F,

			DXT1,
			DXT3,
			DXT5,

			_FORMAT_MAX
		};

		struct FormatDesc {
			// FormatDesc() : 
				// blockSize(0), bpp(0), component(0), formatGL(GL_NONE), formatInternalGL(0), type(GL_NONE) {}
			u32 	blockSize;
			u32 	bpp;
			u32 	component;
			GLenum	formatGL;
			GLint 	formatInternalGL;
			GLenum  type;
		};

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
	}

	struct Sprite {
		std::string name;
		Rect geometry;
	};

	namespace SpriteSheet {
		struct Data {
			Data() {}

			std::string resource_name;
			std::string file_name;
			Texture::Handle sheet_texture;

			std::vector<Sprite> sprites;
		};
	}
}
