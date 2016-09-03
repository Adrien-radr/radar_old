#include "sampling.h"
#include <algorithm>

void Sampling::SampleSphereRandom(std::vector<vec3f>& dirs, u32 count) {
	dirs.reserve(count);

	for (u32 i = 0; i < count; ++i) {
		const vec2f uv = Random::Vec2f();

		vec3f d;

		// cosTheta, sinTheta
		d.y = 2.f * uv.y - 1.f;

		const f32 y2 = d.y * d.y;
		const f32 sinTheta = std::sqrtf(std::max(0.f, 1.f - y2));

		// phi
		const f32 phi = 2.f * M_PI * uv.x;
		d.x = sinTheta * std::cosf(phi);
		d.z = sinTheta * std::sinf(phi);

		dirs.push_back(d);
	}
}

f32 Sampling::SampleSphereRandomPDF() {
	static f32 pdf = 1.f / (4.f * M_PI);
	return pdf;
}

void Sampling::SampleHemisphereRandom(std::vector<vec3f>& dirs, u32 count) {
	dirs.reserve(count);

	for (u32 i = 0; i < count; ++i) {
		const vec2f uv = Random::Vec2f();

		vec3f d;

		// cosTheta, sinTheta
		d.y = uv.y;

		const f32 y2 = d.y * d.y;
		const f32 sinTheta = std::sqrtf(std::max(0.f, 1.f - y2));

		// phi
		const f32 phi = 2.f * M_PI * uv.x;
		d.x = sinTheta * std::cosf(phi);
		d.z = sinTheta * std::sinf(phi);

		dirs.push_back(d);
	}
}

f32 Sampling::SampleHemisphereRandomPDF() {
	static f32 pdf = 1.f / (2.f * M_PI);
	return pdf;
}
