#pragma once
#include "common/common.h"

struct Edge {
	vec3f A, B;
};

struct Polygon {
	std::vector<Edge> edges;

	Polygon() {}
	Polygon(const std::vector<vec3f> &pts);

	/// Oosterom & Strackee 83' triangle solidangle
	/// A, B & C in unit space (spherical triangle)
	f32 SolidAngle() const;

	f32 CosSumIntegralArvo(f32 x, f32 y, f32 c, int nMin, int nMax) const;
	f32 LineIntegralArvo(const vec3f &A, const vec3f &B, const vec3f &w, int nMin, int nMax) const;
	f32 BoundaryIntegralArvo(const vec3f &w, const vec3f &v, int nMin, int nMax) const;
	f32 AxialMomentArvo(const vec3f &w, int order);
	f32 DoubleAxisMomentArvo(const vec3f &w, const vec3f &v, int order);

	// R is a n+2 zero vector that will be filled with all moments up to n
	void CosSumIntegral(f32 x, f32 y, f32 c, int n, std::vector<f32> &R) const;
	void LineIntegral(const vec3f &A, const vec3f &B, const vec3f &w, int n, std::vector<f32> &R) const;
	void BoundaryIntegral(const vec3f &w, const vec3f &v, int n, std::vector<f32> &R) const;
	void AxialMoment(const vec3f &w, int order, std::vector<f32> &R) const;
	std::vector<f32> AxialMoments(const std::vector<vec3f> &directions) const;
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

	/// Same but by providing the 2D barycentric point
	vec3f SamplePointBary(f32 baryX, f32 baryY) const;

	/// Returns a random direction toward the triangle from the given position
	/// Returns the geometry term costheta / r^2 for this sample
	f32 SampleDir(vec3f &rayDir, const f32 s, const f32 t) const;

	/// Same but by providing the 3D point on the triangle
	f32 SampleDir(vec3f &rayDir, const vec3f &pt) const;

private:
	void ComputeArea();
};

struct Rectangle {
	/// p3---------------------p2 ^
	///   |         ^ ey      |   | hy
	///   |         |         |   |
	///   |         p-->ex    |   v
	///   |                   |
	///   |                   |
	/// p0---------------------p1
	///			  <---------> hx

	vec3f position;	// world-space position of light center
	vec3f ex, ey;	// direction vectors tangent to the light plane
	vec3f ez;		// normal to the rectangle, (-cross(ex, ey))
	f32   hx, hy;	// half-width and -height of the rectangle

	vec3f p0, p1, p2, p3;	// world-space corners of the rectangle

	Rectangle() {}

	/// Create a Rectangle from 4 points
	Rectangle(const std::vector<vec3f> &verts);

	/// Creates a Rectangle from a Polygon. This only works if intended.
	Rectangle(const Polygon &P);

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

	/// Analytic Approximation to the Rect integral,
	/// from Drobot's Most Representative Point
	f32 IntegrateMRP(const vec3f &integrationPos, const vec3f &integrationNrm) const;

	/// Numerical Integration with the technique from Pixar [Pekelis & Hery 2014]
	/// This is the preferred method between good results and speed.
	f32 IntegrateAngularStratification(const vec3f &integrationPos, const vec3f &integrationNrm, u32 sampleCount, std::vector<f32> &shvals, int nBand) const;

	/// Numerical integration for Ground Truth, True random world space rectangle sampling
	f32 IntegrateRandom(const vec3f &integrationPos, const vec3f &integrationNrm, u32 sampleCount, std::vector<f32> &shvals, int nBand) const;
};

/// Rectangle projected to a plane
struct PlanarRectangle {
	vec3f p0, p1, p2, p3;
	vec3f ex, ey, ez;
	f32 w, h;
	f32 area;

	PlanarRectangle(const Rectangle &rect, const vec3f &integrationPoint);

	vec3f SamplePoint(f32 u1, f32 u2) const;
	f32 SampleDir(vec3f & rayDir, const f32 u1, const f32 u2) const;

	f32 IntegrateRandom(u32 sampleCount, std::vector<f32> &shvals, int nBand) const;
};

// For use with SphericalRectangle sampling [Urena13]
struct SphericalRectangle {
	vec3f o, x, y, z; // local reference system 'R'
	f32   z0, z0sq;
	f32   x0, y0, y0sq; // rectangle coords in 'R'
	f32   x1, y1, y1sq;
	f32   b0, b1, b0sq, k; // misc precomputed constants
	f32   S; // solid angle

	void Init(const Rectangle &rect, const vec3f &org);
	vec3f Sample(f32 u1, f32 u2) const;

	/// Numerical Integration using the technique from Urena et al., 2013
	/// This is more robust at grazing angles, but a bit slower than AS.
	f32 Integrate(const vec3f &integrationNrm, u32 sampleCount, std::vector<f32> &shvals, int nBand) const;

};

/// Defines a plan from an origin point and a normal
struct Plane {
	vec3f P;
	vec3f N;

	/// Returns the intersection point of the given ray on the plane
	vec3f RayIntersection(const vec3f &rayOrg, const vec3f &rayDir);

	/// Returns the closest point inside the given rectangle for the given point.
	/// If the input point is outside of of the 2D frame of the plan, it is projected on it
	vec3f ClampPointInRect(const Rectangle &rect, const vec3f &point);
};

