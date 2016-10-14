#pragma once

#include "common.h"
namespace Random
{
	void InitRandom();

	float Float();
	vec2f Vec2f();
	vec3f Vec3f();
	vec4f Vec4f();

	template<typename T>
	T Value()
	{
		T::unimplemented_function; // trick to get errors if this is called at compile-time
	}

	template<>
	__forceinline f32 Value<f32>()
	{
		return Float();
	}

	template<>
	__forceinline vec2f Value<vec2f>()
	{
		return Vec2f();
	}

	template<>
	__forceinline vec3f Value<vec3f>()
	{
		return Vec3f();
	}

	template<>
	__forceinline vec4f Value<vec4f>()
	{
		return Vec4f();
	}

	/// Random int between [a, b)
	int Int( int a, int b );

	/// Random uint between [a, b)
	unsigned int UInt( unsigned int a, unsigned int b );

	template<typename T>
	class Pool
	{
	public:
		Pool() : pool( nullptr ), sampleCount( 0 ), sampleIdx( 0 ) {}

		~Pool()
		{
			if ( pool )
				delete[] pool;
		}

		void Init( u32 nSamples, T( mapFunc )( T ) = []( T v ) { return v; } )
		{
			if ( pool )
				delete[] pool;

			sampleCount = nSamples;
			sampleIdx = 0;
			pool = new T[sampleCount];

			// Fill the pool with the mapped random values
			// starts with index 1 since GetNext() starts with this one
			for ( u32 i = 1; i < sampleCount; ++i )
			{
				pool[i] = mapFunc( Random::Value<T>() );
			}
			pool[0] = mapFunc( Random::Value<T>() );
		}

		T Next()
		{
			Assert( pool );
			sampleIdx = ( sampleIdx + 1 ) % sampleCount;

			return pool[sampleIdx];
		}

	private:
		T *pool;
		u32 sampleCount;
		u32 sampleIdx;
	};

	/// Global Random floating point value pool, queriable from anywhere
	/// For faster query than the individual functions
	extern Random::Pool<f32> GlobalPool;
}