#include "font.h"
#include "GL/glew.h"
#include <algorithm>

namespace Render {
	namespace Font{
		FT_Library Library;

		bool CreateAtlas(Data &font, const std::string &file, u32 size) {
			if (FT_New_Face(Library, file.c_str(), 0, &font.face)) {
				LogErr("Font file '", file, "' could not be opened.");
				return false;
			}
			if (FT_Set_Pixel_Sizes(font.face, 0, size)) {
				LogErr("Font '", file.c_str(), "' is not available at requested size ", size, ".");
				return false;
			}

			FT_GlyphSlot g = font.face->glyph;

			// see how large the atlas should be
			for (u32 i = 32; i < 128; ++i) {
				if (FT_Load_Char(font.face, i, FT_LOAD_RENDER)) {
					LogErr("Could not load char ", i, ": ", (char)i, ".");
					return false;
				}

				font.size.x += g->bitmap.width;
				font.size.y = std::max(font.size.y, (int)g->bitmap.rows);
			}

			Texture::Desc td;
			td.size = font.size;
			std::stringstream texture_name;
			texture_name << file << "_" << size << "_atlas";
			td.name[0] = texture_name.str();
			td.type = Texture::Empty;
			font.handle = Texture::Build(td);
			if (font.handle < 0) {
				LogErr("Error creating empty texture for font.");
				return false;
			}

			GLint curr_alignment;
			glGetIntegerv(GL_UNPACK_ALIGNMENT, &curr_alignment);
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, font.size.x, font.size.y,
				0, GL_RED, GL_UNSIGNED_BYTE, 0);

			// iterate over the font chars and write them in atlas
			int x = 0;
			for (u32 i = 32; i < 128; ++i) {
				if (FT_Load_Char(font.face, i, FT_LOAD_RENDER))
					continue;

				glTexSubImage2D(GL_TEXTURE_2D, 0, x, 0, g->bitmap.width,
					g->bitmap.rows,
					GL_RED,
					GL_UNSIGNED_BYTE,
					g->bitmap.buffer);

				font.glyphs[i].advance[0] = g->advance.x >> 6;
				font.glyphs[i].advance[1] = g->advance.y >> 6;

				font.glyphs[i].size[0] = g->bitmap.width;
				font.glyphs[i].size[1] = g->bitmap.rows;

				font.glyphs[i].position[0] = g->bitmap_left;
				font.glyphs[i].position[1] = g->bitmap_top;

				font.glyphs[i].x_offset = (f32)x / (f32)font.size.x;

				x += g->bitmap.width;
			}

			glPixelStorei(GL_UNPACK_ALIGNMENT, curr_alignment);

			return true;
		}

	}
}
