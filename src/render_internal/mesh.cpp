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
	namespace Mesh {
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
			// const u32 nTriangles = nVerts * 2;
			const u32 nIndices = (nLat-1)*nLon*6 + nLon*2*3;//nTriangles * 3;

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
			uv[0] = vec2f(0,1);
			uv[nVerts-1] = vec2f(0.f);
			for(u32 lat = 0; lat < nLat; ++lat) {
				for(u32 lon = 0; lon <= nLon; ++lon) {
					uv[lon + lat * (nLon + 1) + 1] = vec2f(lon/(f32)nLon, 1.f - (lat + 1) / (f32)(nLat + 1));
				} 
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


	}
}


