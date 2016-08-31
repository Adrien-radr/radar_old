#pragma once

#include "common/common.h"

namespace Render {
	namespace BRDF {
		// Integrate a constant-value Edge/Poly cosine-weighted at origin (u1 & u2 unit vectors)
		f32 IntegrateEdge(const vec3f &u1, const vec3f &u2);
		f32 IntegratePoly(const vec3f *edges, const u32 edgeCount, bool twoSided);

		/// BRDFS
		/// roughness goes from 1e-3 (shiny) to 1.0 (diffuse)
		/// All BRDF functions return the BRDF evaluation PRIOR to the multiplication by NdotL. This should be done afterwards

		// Diffuse BRDF
		f32 Diffuse();

		// Disney Diffuse BRDF
		f32 DiffuseBurley(f32 NdotL, f32 NdotV, f32 LdotH, f32 roughness);

		// Phong BRDF
		f32 Phong(const vec3f &N, const vec3f &R, f32 roughness);

		// GGX BRDF
		vec3f FresnelSchlick(const vec3f &f0, f32 f90, f32 u);
		vec3f GGX(f32 NdotL, f32 NdotV, f32 NdotH, f32 LdotH, f32 roughness, const vec3f &F0);

		// LTC stuff
		vec2f LTC_Coords(f32 NdotV, f32 roughness, int LUTSize);
		mat3f LTC_Matrix(f32 *ltcMatrix, const vec2f &coord, int LUTSize);
		f32   LTC_Evaluate(const vec3f &N, const vec3f &V, const vec3f &P, const mat3f &Minv, const vec3f points[4], bool twoSided);
	}
}