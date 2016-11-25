#include "sampling.h"
#include <algorithm>

void Sampling::SampleSphereRandom( std::vector<vec3f>& dirs, u32 count )
{
	dirs.reserve( count );

	for ( u32 i = 0; i < count; ++i )
	{
		const vec2f uv = Random::Vec2f();

		vec3f d;

		// cosTheta, sinTheta
		d.y = 2.f * uv.y - 1.f;

		const f32 y2 = d.y * d.y;
		const f32 sinTheta = std::sqrt( std::max( 0.f, 1.f - y2 ) );

		// phi
		const f32 phi = 2.f * M_PI * uv.x;
		d.x = sinTheta * std::cos( phi );
		d.z = sinTheta * std::sin( phi );

		dirs.push_back( d );
	}
}

f32 Sampling::SampleSphereRandomPDF()
{
	static f32 pdf = 1.f / ( 4.f * M_PI );
	return pdf;
}

void Sampling::SampleHemisphereRandom( std::vector<vec3f>& dirs, u32 count )
{
	dirs.reserve( count );

	for ( u32 i = 0; i < count; ++i )
	{
		const vec2f uv = Random::Vec2f();

		vec3f d;

		// cosTheta, sinTheta
		d.y = uv.y;

		const f32 y2 = d.y * d.y;
		const f32 sinTheta = std::sqrt( std::max( 0.f, 1.f - y2 ) );

		// phi
		const f32 phi = 2.f * M_PI * uv.x;
		d.x = sinTheta * std::cos( phi );
		d.z = sinTheta * std::sin( phi );

		dirs.push_back( d );
	}
}

f32 Sampling::SampleHemisphereRandomPDF()
{
	static f32 pdf = 1.f / ( 2.f * M_PI );
	return pdf;
}

f32 MinDotDistance( const std::vector<vec3f>& dirs, const vec3f& w )
{

	// The set of testing direction is empty.
	if ( dirs.size() == 0 )
	{
		return 0.0f;
	}

	// Got a least one direction in `dirs`
	float dist = std::numeric_limits<float>::max();
	for ( auto& d : dirs )
	{
		const float dot = Dot( d, w );
		dist = std::min( dist, dot );
	}
	return dist;
}

void Sampling::SampleSphereBluenoise( std::vector<vec3f>& dirs, u32 count )
{
	const u32 maxTry = 1000;
	dirs.reserve( count );

	float max_dist = 0.5;
	u32 n = 0;
	while ( n < count )
	{

		// Vector to add
		vec3f w;

		u32 nb_try = 0;
		while ( nb_try < maxTry )
		{
			vec2f randV = Random::Vec2f();

			// Generate a random direction by uniformly sampling the sphere.
			float z = 2.0f * randV.x - 1.0f;
			float y = sqrt( 1.0f - z*z );
			float p = 2.0f * M_PI * randV.y;
			w = vec3f( y*cos( p ), z, y * sin( p ) );
			w = Normalize( w );

			// Testing if the distance satisfy blue noise properties.
			float dot_dist = MinDotDistance( dirs, w );
			if ( dot_dist < max_dist )
			{
				break;
			}
			else
			{
				++nb_try;
			}
		}

		if ( nb_try == maxTry )
		{
			max_dist = sqrt( max_dist );
		}
		else
		{
			dirs.push_back( w );
			++n;
		}
	}
}
