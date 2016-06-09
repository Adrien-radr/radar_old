#pragma once

#include "../render.h"

#include <ft2build.h>
#include FT_FREETYPE_H

namespace Render {
	namespace Font {
		struct _Glyph {
			vec2i   advance,
					size,
					position;

			f32     x_offset;
		};

		struct Data {
			Data() : size(0,0), handle(-1) {}

			_Glyph			glyphs[128];
			FT_Face			face;

			vec2i			size;
			Texture::Handle handle;
		};

		// Instance of the Freetype Library. initialized and freed by the renderer
		extern FT_Library Library;

		bool CreateAtlas(Data &font, const std::string &file, u32 size);
	}
}





