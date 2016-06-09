#pragma once

/// IMPORTANT !!!!!!!!!!!!1
/// This file is intended to be included in render.c and optionnally
/// in render_internal/*.c files, not elsewhere !

#include "../render.h"
#include "GL/glew.h"

namespace Render {
	namespace Texture {
		/// Intermediate format for texture loading
		/// Do not use unless you know why/how
		struct _tex {
			_tex() : width(0), height(0), id(0), texels(NULL) {}

			u32         width,
						height;

			GLenum      fmt;
			GLint       int_fmt;
			GLuint      id;

			GLubyte    *texels;
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
