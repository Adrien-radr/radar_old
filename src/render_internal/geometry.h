#pragma once
#include "common/common.h"

struct Edge {
	vec3f A, B;
};

struct Polygon {
	std::vector<Edge> edges;

	/// Oosterom & Strackee 83' triangle solidangle
	/// A, B & C in unit space (spherical triangle)
	f32 SolidAngle(const vec3f &A, const vec3f &B, const vec3f &C);

	f32 CosSumIntegral(f32 x, f32 y, f32 c, int nMin, int nMax);
	f32 LineIntegral(const vec3f &A, const vec3f &B, const vec3f &w, int nMin, int nMax);
	f32 BoundaryIntegral(const vec3f &w, const vec3f &v, int nMin, int nMax);
	f32 AxialMoment(const vec3f &w, int order);
	f32 DoubleAxisMoment(const vec3f &w, const vec3f &v, int order);
};



/// Represents a spherical or WS triangle of three coordinates q0, q1, q2.
/// depending on the Init function used
/// solid angle is approximated. For a real triangle solid angle, use Polygon::SolidAngle
struct Triangle {
	vec3f q0, q1, q2;
	vec3f unitNormal;
	f32 area;
	f32 solidAngle;

	/// intPos is the world space origin of the spherical projection of world space coordinate vertices p0, p1, p2
	void InitUnit(const vec3f &p0, const vec3f &p1, const vec3f &p2, const vec3f &intPos);

	/// Same as InitUnit, except the resulting triangle is kept in world space. 
	/// triNormal is the normal of the plane collinear with the triangle
	void InitWS(const vec3f &triNormal, const vec3f &p0, const vec3f &p1, const vec3f &p2, const vec3f &intPos);

	/// Returns the distance of the triangle to the integration Point. Only works robustly for Unit triangles
	f32 distToOrigin() const {
		return -1.0f * unitNormal.Dot(q0);
	}

	/// Subdivide the triangle into 4 sub triangles (stored in the given vector).
	u32 Subdivide4(Triangle subdivided[4]) const;

	/// Returns a point on the triangle sampled according to (u1, u2)
	vec3f SamplePoint(f32 u1, f32 u2) const;

	/// Returns a random direction toward the triangle from the given position
	/// Returns the geometry term costheta / r^2 for this sample
	f32 SampleDir(vec3f &rayDir, const f32 s, const f32 t) const;

private:
	void ComputeArea();
};

struct Rectangle {
	/// --------------------- ^
	/// |         ^ ey      | | hy
	/// |         |         | |
	/// |         p-->ex    | v
	/// |                   |
	/// |                   |
	/// ---------------------
	///			  <---------> hx

	vec3f position;	// world-space position of light center
	vec3f ex, ey;	// direction vectors tangent to the light plane
	vec3f ez;		// normal to the rectangle, (cross(ex, ey))
	f32   hx, hy;	// half-width and -height of the rectangle

	vec3f p0, p1, p2, p3;	// world-space corners of the rectangle

	/// Returns a point on the rectangle sampled according to (u1, u2)
	vec3f SamplePoint(f32 u1, f32 u2) const;

	/// Returns a random direction toward the rectangle from the given position
	/// Returns the geometry term costheta / r^2 for this sample
	f32 SampleDir(vec3f &rayUnitDir, const vec3f &pos, const f32 s, const f32 t) const;
	
	/// Returns the solid angle subtended by the rectangle when projected to the given integration position
	f32 SolidAngle(const vec3f &integrationPos) const;

	/// Analytic Approximation to the Rect integral,
	/// cosine-weighted at surface point Pos with normal Nrm,
	/// from Frostbite
	/// This auto performs horizon-cliping
	f32 IntegrateStructuredSampling(const vec3f &integrationPos, const vec3f &integrationNrm) const;

	/// Numerical Integration with the technique from Pixar [Pekelis & Hery 2014]
	f32 IntegrateAngularStratification(const vec3f &integrationPos, const vec3f &integrationNrm, u32 sampleCount, std::vector<f32> &shvals, int nBand) const;

	/// Numerical integration for Ground Truth, True random world space rectangle sampling
	f32 IntegrateRandom(const vec3f &integrationPos, const vec3f &integrationNrm, u32 sampleCount, std::vector<f32> &shvals, int nBand) const;
};