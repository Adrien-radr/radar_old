#include "random.h"
#include <random>
#include <array>
#include <algorithm>

namespace Random {
	std::mt19937 twister;
	std::uniform_real_distribution<float> rd(0, 1);

	// Feed the twister with enough entropy (state size of 19968 bits == 624 ints)
	void InitRandom() {
		std::random_device r;
		std::array<int, 624> seed_data;
		std::generate(seed_data.begin(), seed_data.end(), std::ref(r));

		std::seed_seq seed(std::begin(seed_data), std::end(seed_data));
		twister = std::mt19937(seed);
	}

	float Float() {
		return rd(twister);
	}

	vec2f Vec2f() {
		return vec2f(Float(), Float());
	}

	vec3f Vec3f() {
		return vec3f(Float(), Float(), Float());
	}

	vec4f Vec4f() {
		return vec4f(Float(), Float(), Float(), Float());
	}

	int Int(int a, int b) {
		std::uniform_int_distribution<int> di(a, b);

		return di(twister);
	}

	unsigned int UInt(unsigned int a, unsigned int b) {
		std::uniform_int_distribution<unsigned int> di(a, b);

		return di(twister);
	}

	vec2i Vec2i(int a, int b) {
		return vec2i(Int(a, b), Int(a, b));
	}
}