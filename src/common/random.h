#pragma once

#include "common.h"
namespace Random {
	void InitRandom();

	float Float();
	vec2f Vec2f();
	vec3f Vec3f();
	vec4f Vec4f();

	/// Random int between [a, b)
	int Int(int a, int b);

	/// Random uint between [a, b)
	unsigned int UInt(unsigned int a, unsigned int b);

	vec2i Vec2i();
}