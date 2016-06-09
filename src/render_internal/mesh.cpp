#include "mesh.h"
#include "font.h"

// GL functions
#include "GL/glew.h"

namespace Render {
	namespace TextMesh {
		static u32 sizes_n = 6;
		static u32 letter_count[] = { 16, 32, 128, 512, 1024, 2048 };

		bool CreateFromString(Data &mesh, const Font::Data &font, const std::string &str) {
			u32 str_len = (u32)str.length();

			if (str_len > mesh.curr_letter_n) {
				// find upper bound for data size
				u32 lc = 0;
				for (u32 i = 0; i < sizes_n; ++i) {
					if (str_len < letter_count[i]) {
						lc = letter_count[i];
						break;
					}
				}

				if (lc <= 0) {
					LogErr("Given string is too large. Max characters is ", letter_count[sizes_n - 1]);
					return false;
				}

				// check if need to expand vertex array
				if (lc > mesh.curr_letter_n) {
					mesh.data.resize(4 * 6 * lc);

					// create mesh if first time
					if (0 == mesh.vbo) {
						glGenBuffers(1, &mesh.vbo);
					}
					glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
					glBufferData(GL_ARRAY_BUFFER, 4 * 6 * lc*sizeof(f32),
						NULL, GL_DYNAMIC_DRAW);
				}

				mesh.curr_letter_n = lc;
			}

			mesh.vertices_n = str_len * 6;

			int n_pos = 0, n_tex = mesh.vertices_n * 2;
			int fw = font.size.x, fh = font.size.y;

			f32 x_left = 0;
			f32 x = 0, y = 0;

			const Font::_Glyph *glyphs = font.glyphs;

			for (u32 i = 0; i < str_len; ++i) {
				int ci = (int)str[i];

				if (0 == str[i]) break;  // just in case str_len is wrong

				// TODO : Parameter for max length before line wrap
				if (/*x > 300 ||*/ '\n' == str[i]) {
					y += (fh + 4);
					x = x_left;
					n_pos += 12;
					n_tex += 12;
					continue;
				}
				else if (32 == str[i]) {
					x += (fh / 3.f);
					//            n_pos += 12;
					//            n_tex += 12;

					for (u32 i = 0; i < 12; ++i) {
						mesh.data[n_pos++] = 0.f;
						mesh.data[n_tex++] = 0.f;
					}

					continue;
				}

				f32 x2 = x + glyphs[ci].position[0];
				f32 y2 = y + (fh - glyphs[ci].position[1]);
				f32 w = (f32)glyphs[ci].size[0];
				f32 h = (f32)glyphs[ci].size[1];

				if (!w || !h) continue;

				x += glyphs[ci].advance[0];
				y += glyphs[ci].advance[1];

				mesh.data[n_pos++] = x2;     mesh.data[n_pos++] = y2;
				mesh.data[n_pos++] = x2;     mesh.data[n_pos++] = y2 + h;
				mesh.data[n_pos++] = x2 + w; mesh.data[n_pos++] = y2 + h;

				mesh.data[n_pos++] = x2;     mesh.data[n_pos++] = y2;
				mesh.data[n_pos++] = x2 + w; mesh.data[n_pos++] = y2 + h;
				mesh.data[n_pos++] = x2 + w; mesh.data[n_pos++] = y2;

				//log_info("<%g,%g>, <%g,%g>", glyphs[ci].x_offset, 
				//0.f, 
				//glyphs[ci].x_offset+glyphs[ci].size[0]/(f32)fw, 
				//glyphs[ci].size[1]/(f32)fh);
				mesh.data[n_tex++] = glyphs[ci].x_offset;
				mesh.data[n_tex++] = 0;
				mesh.data[n_tex++] = glyphs[ci].x_offset;
				mesh.data[n_tex++] = (f32)glyphs[ci].size[1] / (f32)fh;
				mesh.data[n_tex++] = glyphs[ci].x_offset + (f32)glyphs[ci].size[0] / (f32)fw;
				mesh.data[n_tex++] = (f32)glyphs[ci].size[1] / (f32)fh;

				mesh.data[n_tex++] = glyphs[ci].x_offset;
				mesh.data[n_tex++] = 0;
				mesh.data[n_tex++] = glyphs[ci].x_offset + (f32)glyphs[ci].size[0] / (f32)fw;
				mesh.data[n_tex++] = (f32)glyphs[ci].size[1] / (f32)fh;
				mesh.data[n_tex++] = glyphs[ci].x_offset + (f32)glyphs[ci].size[0] / (f32)fw;
				mesh.data[n_tex++] = 0;
			}

			// update vbo
			glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
			glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)0, 4 * mesh.vertices_n*sizeof(f32),
				&mesh.data[0]);

			return true;
		}
	}
}


