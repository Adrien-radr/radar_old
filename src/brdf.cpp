#include "BRDF.h"
#include <algorithm>

namespace Render {
	namespace BRDF {
		f32 IntegrateEdge(const vec3f &u1, const vec3f &u2) {
			f32 cosTheta = u1.Dot(u2);
			cosTheta = std::max(-0.9999f, std::min(0.9999f, cosTheta));

			f32 theta = std::acosf(cosTheta);
			f32 r = (u1.Cross(u2)).z * theta / std::sinf(theta);
			return r;
		}

		f32 IntegratePoly(const vec3f *edges, const u32 edgeCount, bool twoSided) {
			f32 sum = 0.0;

			for (u32 i = 0; i < edgeCount - 1; ++i) {
				sum += IntegrateEdge(edges[i], edges[i + 1]);
			}
			sum += IntegrateEdge(edges[edgeCount - 1], edges[0]);

			sum = twoSided ? std::abs(sum) : std::max(0.f, -sum);

			return sum;
		}

		f32 Diffuse() {
			static f32 invpi = 1.f / (f32)M_PI;
			return invpi;
		}

		f32 DiffuseBurley(f32 NdotL, f32 NdotV, f32 LdotH, f32 roughness) {
			f32 energyBias = 0.5f * roughness;
			f32 energyFactor = 1.f + roughness / 1.51f;

			f32 fd90 = energyBias + 2.f * LdotH * LdotH * roughness;
			vec3f f0 = vec3f(1.f);
			f32 lightScatter = FresnelSchlick(f0, fd90, NdotL).x;
			f32 viewScatter = FresnelSchlick(f0, fd90, NdotV).x;

			return lightScatter * viewScatter * energyFactor * Diffuse();
		}

		f32 Phong(const vec3f &V, const vec3f &R, f32 roughness) {
			return std::pow(std::max(0.f, V.Dot(R)), (1.f / (roughness*roughness)));
		}

		vec3f FresnelSchlick(const vec3f &f0, f32 f90, f32 u) {
			return f0 + (vec3f(f90) - f0) * std::pow(1.f - u, 5.f);
		}

		vec3f GGX(f32 NdotL, f32 NdotV, f32 NdotH, f32 LdotH, f32 roughness, const vec3f &F0) {
			f32 alpha2 = roughness * roughness;

			// F 
			vec3f F = FresnelSchlick(F0, 1.f, LdotH);

			// D (Trowbridge-Reitz). Divide by PI at the end
			f32 denom = NdotH * NdotH * (alpha2 - 1.f) + 1.f;
			f32 D = alpha2 / (M_PI * denom * denom);

			// G (Smith GGX - Height-correlated)
			f32 lambda_GGXV = NdotL * std::sqrtf((-NdotV * alpha2 + NdotV) * NdotV + alpha2);
			f32 lambda_GGXL = NdotV * std::sqrtf((-NdotL * alpha2 + NdotL) * NdotL + alpha2);
			// f32 G = G_schlick_GGX(k, NdotL) * G_schlick_GGX(k, NdotV);

			// optimized G(NdotL) * G(NdotV) / (4 NdotV NdotL)
			// f32 G = 1.0 / (4.0 * (NdotL * (1 - k) + k) * (NdotV * (1 - k) + k));
			f32 G = 0.5f / (lambda_GGXL + lambda_GGXV);

			return F * G * D;
		}

		vec2f LTC_Coords(f32 NdotV, f32 roughness, int LUTSize) {
			f32 theta = acos(NdotV);
			vec2f coords = vec2f(roughness, theta / (0.5f * M_PI));

			// corrected lookup (x - 1 + 0.5)
			coords = coords * ((f32)LUTSize - 1.f) / (f32)LUTSize + 0.5f / (f32)LUTSize;

			return coords;
		}

		mat3f LTC_Matrix(f32 *ltcMatrix, const vec2f &coord, int LUTSize) {
			// bilinear interpolation of the ltcmatrix 
			const vec4f v = BilinearLookup<vec4f>(ltcMatrix, coord, vec2i(LUTSize, LUTSize));

			// inverse of 
			// a 0 b
			// 0 c 0
			// d 0 1
			const mat3f Minv = mat3f(
				vec3f(1, 0, v.y),       // 1st column
				vec3f(0, v.z, 0),
				vec3f(v.w, 0, v.x)
			);

			return Minv;
		}

		f32 LTC_Evaluate(const vec3f &N, const vec3f &V, const vec3f &P, const mat3f &Minv, const vec3f points[4], bool twoSided) {
			// N orthonormal frame
			vec3f T, B;
			T = V - N * V.Dot(N);
			T.Normalize();
			B = N.Cross(T);

			mat3f LMinv = Minv * mat3f(T, B, N).Transpose();

			// polygon in TBN frame 
			vec3f L[5];
			L[0] = LMinv * (points[0] - P);
			L[1] = LMinv * (points[1] - P);
			L[2] = LMinv * (points[2] - P);
			L[3] = LMinv * (points[3] - P);
			L[4] = L[3];

			//int n;
			//ClipQuadToHorizon(L, n);

			//if (n == 0)
			//return vec3(0, 0, 0);

			// polygon projected onto sphere
			L[0].Normalize();
			L[1].Normalize();
			L[2].Normalize();
			L[3].Normalize();
			L[4].Normalize();

			return IntegratePoly(L, 4, twoSided);
		}
	}
}