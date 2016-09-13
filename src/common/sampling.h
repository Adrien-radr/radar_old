#pragma once

#include "common.h"

namespace Sampling {
	/// Different Sphere random sampling methods.
	/// dirs.y is UP

	/// Uniform Sphere Distribution using STL Mersenne Twister (Random::)
	void SampleSphereRandom(std::vector<vec3f> &dirs, u32 count);
	f32 SampleSphereRandomPDF();


	void SampleHemisphereRandom(std::vector<vec3f> &dirs, u32 count);
	f32 SampleHemisphereRandomPDF();

	/// BlueNoise
	void SampleSphereBluenoise(std::vector<vec3f> &dirs, u32 count);
}