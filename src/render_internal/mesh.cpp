#include "mesh.h"
#include "font.h"
#include "common/SHEval.h"
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

		Handle BuildBox() {
			// Test resource existence before recreating it
			{
				Handle h;
				if(Exists("Box1", h)) {
					return h;
				}
			}

			vec3f pos[24] = {
				vec3f(-1, -1, -1),
				vec3f(-1, -1, 1),
				vec3f(-1, 1, 1),
				vec3f(-1, 1, -1),

				vec3f(1, -1, 1),
				vec3f(1, -1, -1),
				vec3f(1, 1, -1),
				vec3f(1, 1, 1),

				vec3f(-1, -1, 1),
				vec3f(-1, -1, -1),
				vec3f(1, -1, -1),
				vec3f(1, -1, 1),

				vec3f(-1, 1, -1),
				vec3f(-1, 1, 1),
				vec3f(1, 1, 1),
				vec3f(1, 1, -1),

				vec3f(1, 1, -1),
				vec3f(1, -1, -1),
				vec3f(-1, -1, -1),
				vec3f(-1, 1, -1),

				vec3f(-1, 1, 1),
				vec3f(-1, -1, 1),
				vec3f(1, -1, 1),
				vec3f(1, 1, 1)
			};

			vec3f nrm[24] = {
				vec3f(-1, 0, 0),
				vec3f(-1, 0, 0),
				vec3f(-1, 0, 0),
				vec3f(-1, 0, 0),

				vec3f(1, 0, 0),
				vec3f(1, 0, 0),
				vec3f(1, 0, 0),
				vec3f(1, 0, 0),

				vec3f(0, -1, 0),
				vec3f(0, -1, 0),
				vec3f(0, -1, 0),
				vec3f(0, -1, 0),

				vec3f(0, 1, 0),
				vec3f(0, 1, 0),
				vec3f(0, 1, 0),
				vec3f(0, 1, 0),

				vec3f(0, 0, -1),
				vec3f(0, 0, -1),
				vec3f(0, 0, -1),
				vec3f(0, 0, -1),

				vec3f(0, 0, 1),
				vec3f(0, 0, 1),
				vec3f(0, 0, 1),
				vec3f(0, 0, 1)
			};

			vec2f uv[24] = {
				vec2f(0, 1),
				vec2f(0, 0),
				vec2f(1, 0),
				vec2f(1, 1),

				vec2f(0, 1),
				vec2f(0, 0),
				vec2f(1, 0),
				vec2f(1, 1),

				vec2f(0, 1),
				vec2f(0, 0),
				vec2f(1, 0),
				vec2f(1, 1),

				vec2f(0, 1),
				vec2f(0, 0),
				vec2f(1, 0),
				vec2f(1, 1),

				vec2f(0, 1),
				vec2f(0, 0),
				vec2f(1, 0),
				vec2f(1, 1),

				vec2f(0, 1),
				vec2f(0, 0),
				vec2f(1, 0),
				vec2f(1, 1),
			};

			u32 indices[36];
			for(u32 i = 0; i < 6; ++i) {
				indices[i * 6 + 0] = i * 4 + 0;
				indices[i * 6 + 1] = i * 4 + 1;
				indices[i * 6 + 2] = i * 4 + 2;

				indices[i * 6 + 3] = i * 4 + 0;
				indices[i * 6 + 4] = i * 4 + 2;
				indices[i * 6 + 5] = i * 4 + 3;
			}

			Desc desc("Box1", false, 36, indices, 24, (f32*)pos, (f32*)nrm, (f32*)uv);
			return Build(desc);
		}

		Handle BuildSHVisualization(const float *shCoeffs, const u32 bandN, const std::string &meshName, bool shNormalization, const u32 numPhi, const u32 numTheta) {
			const u32 faceCount = numPhi * 2 + (numTheta-3) * numPhi * 2;
			const u32 indexCount = faceCount * 3;
			const u32 vertexCount = 2 + numPhi * (numTheta - 2);
			const u32 shCoeffsN = bandN * bandN;

			std::vector<u32> indices(indexCount);
			std::vector<vec3f> pos(vertexCount);
			std::vector<vec3f> nrm(vertexCount);

			u32 fi, vi, rvi = 0, ci;

			// Triangles
			{
				u32 rfi = 0;

				// top cap
				for (fi = 0; fi < numPhi; ++fi) {
					indices[fi * 3 + 0] = 0;
					indices[fi * 3 + 1] = fi + 1;
					indices[fi * 3 + 2] = ((fi + 1) % numPhi) + 1;
					++rfi;
				}

				// spans
				for (vi = 1; vi < numTheta - 2; ++vi) {
					for (fi = 0; fi < numPhi; ++fi) {
						indices[rfi * 3 + 0] = (1 + (vi - 1) * numPhi + fi);
						indices[rfi * 3 + 1] = (1 + (vi)     * numPhi + fi);
						indices[rfi * 3 + 2] = (1 + (vi)     * numPhi + ((fi + 1) % numPhi));
						++rfi;
						indices[rfi * 3 + 0] = (1 + (vi - 1) * numPhi + fi);
						indices[rfi * 3 + 1] = (1 + (vi)     * numPhi + ((fi + 1) % numPhi));
						indices[rfi * 3 + 2] = (1 + (vi - 1) * numPhi + ((fi + 1) % numPhi));
						++rfi;
					}
				}

				// bottom cap
				for (fi = 0; fi < numPhi; ++fi) {
					indices[rfi * 3 + 0] = (1 + numPhi * (numTheta - 3) + fi);
					indices[rfi * 3 + 1] = (1 + numPhi * (numTheta - 2));
					indices[rfi * 3 + 2] = (1 + numPhi * (numTheta - 3) + ((fi + 1) % numPhi));
					++rfi;
				}
			}

			// Scale function coeffs
			std::vector<float> scaledCoeffs(shCoeffsN);
			float normFactor = shNormalization ? 0.3334f/shCoeffs[0] : 1.f;
			for (ci = 0; ci < shCoeffsN; ++ci) {
				scaledCoeffs[ci] = shCoeffs[ci] * normFactor;
			}

			std::vector<float> shVals(shCoeffsN);

			for (vi = 0; vi < numTheta; ++vi) {
				float theta = vi * M_PI / ((float) numTheta - 1.f);
				float cT = std::cos(theta);
				float sT = std::sin(theta);

				if (vi && (vi < numTheta - 1)) { // spans
					for (fi = 0; fi < numPhi; ++fi) {
						float phi = fi * 2.f * M_PI / (float) numPhi;
						float cP = std::cos(phi);
						float sP = std::sin(phi);

						vec3f vpos(sT * cP, sT * sP, cT);
						vec3f vnrm = vpos;
						vnrm.Normalize();

						// Get sh coeffs for this direction
						std::fill_n(shVals.begin(), shCoeffsN, 0.f);
						SHEval(bandN, vpos.x, vpos.z, vpos.y, &shVals[0]);

						float fVal = 0.f;
						for (ci = 0; ci < shCoeffsN; ++ci) {
							fVal += shVals[ci] * scaledCoeffs[ci];
						}

						if (fVal < 0.f) {
							fVal *= -1.f;
						}

						vpos *= fVal;

						pos[rvi] = vpos;
						nrm[rvi] = vnrm;
						++rvi;
					}
				}
				else { // Cap 2 points
					if (vi) theta -= 1e-4f;
					else theta = 1e-4f;

					cT = std::cos(theta);
					sT = std::sin(theta);
					float cP, sP;
					cP = sP = 0.707106769f; // sqrt(0.5)

					vec3f vpos(sT * cP, sT * sP, cT);
					vec3f vnrm = vpos;
					vnrm.Normalize();

					// Get sh coeffs for this direction
					std::fill_n(shVals.begin(), shCoeffsN, 0.f);
					SHEval(bandN, vpos.x, vpos.z, vpos.y, &shVals[0]);

					float fVal = 0.f;
					for (ci = 0; ci < shCoeffsN; ++ci) {
						fVal += shVals[ci] * scaledCoeffs[ci];
					}

					if (fVal < 0.f) {
						fVal *= -1.f;
					}

					vpos *= fVal;

					pos[rvi] = vpos;
					nrm[rvi] = vnrm;
					++rvi;
				}
			}

			Desc desc(meshName, false, indexCount, (u32*)(&indices[0]), vertexCount, (f32*)(&pos[0]), (f32*)(&nrm[0]));
			Handle h = Build(desc);

			return h;
		}
	}
}


