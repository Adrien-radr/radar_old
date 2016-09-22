#pragma once

//#include "../render.h"
#include "common/common.h"

struct FT_FaceRec_;
typedef FT_FaceRec_* FT_Face;

namespace Render {
	namespace Font {
		// Freetype Library. initialized and freed by the renderer
		bool InitFontLibrary();
		void DestroyFontLibrary();

		struct Desc {
			Desc(const std::string &fname = "", u32 fsize = 0) :
				name(fname), size(fsize) {
			}

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

		namespace _internal {
			struct Glyph {
				vec2i   advance,
					size,
					position;

				f32     x_offset;
			};

			struct Data {
				Data() : size(0, 0), handle(-1) {}

				Glyph			glyphs[128];
				FT_Face			face;

				vec2i			size;
				Texture::Handle handle;
			};


			bool CreateAtlas(Data &font, const std::string &file, u32 size);
		}
	}
}





