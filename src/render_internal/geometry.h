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
	void InitUnit(const vec3f &p0, const vec3f &p1, const vec3f &p2, const vec3f &intPos) {
		q0 = p0 - intPos;
		q1 = p1 - intPos;
		q2 = p2 - intPos;
		q0.Normalize();
		q1.Normalize();
		q2.Normalize();

		const vec3f d1 = q1 - q0;
		const vec3f d2 = q2 - q0;

		const vec3f nrm = d1.Cross(d2);
		const f32 nrmLen = std::sqrtf(nrm.Dot(nrm));
		area = solidAngle = nrmLen * 0.5f;

		// compute inset triangle's unit normal
		const f32 areaThresh = 1e-5f;
		bool badPlane = -1.0f * q0.Dot(nrm) <= 0.0f;

		if (badPlane || area < areaThresh) {
			unitNormal = -(q0 + q1 + q2);
			unitNormal.Normalize();
		}
		else {
			unitNormal = nrm / nrmLen;
		}
	}

	/// Same as InitUnit, except the resulting triangle is kept in world space. 
	/// triNormal is the normal of the plane collinear with the triangle
	void InitWS(const vec3f &triNormal, const vec3f &p0, const vec3f &p1, const vec3f &p2, const vec3f &intPos) {
		unitNormal = triNormal;

		q0 = p0 - intPos;
		q1 = p1 - intPos;
		q2 = p2 - intPos;

		ComputeArea();

		const vec3f bary = (q0 + q1 + q2) * 0.3333333333333f;
		const f32 rayLenSqr = bary.Dot(bary);
		const f32 rayLen = std::sqrtf(rayLenSqr);
		solidAngle = bary.Dot(unitNormal) * (-area / (rayLenSqr * rayLen));
	}

	/// Returns the geometry term cos(theta) / r^3
	f32 GetSample(vec3f &rayUnitDir, const f32 s, const f32 t) const {
		const vec3f rayDir = SamplePoint(s, t);
		//const vec3f rayDir = q0 * (1.f - s - t) + q1 * s + q2 * t;
		const f32 rayLenSqr = rayDir.Dot(rayDir);
		const f32 rayLen = std::sqrtf(rayLenSqr);
		rayUnitDir = rayDir / rayLen;

		return -rayDir.Dot(unitNormal) / (rayLenSqr * rayLen);
	}

	/// Returns the distance of the triangle to the integration Point. Only works robustly for Unit triangles
	f32 distToOrigin() const {
		return -1.0f * unitNormal.Dot(q0);
	}

	/// Subdivide the triangle into 4 sub triangles (stored in the given vector.
	// subdivided is always 4-length
	u32 Subdivide4(Triangle subdivided[4]) const {
		vec3f q01 = (q0 + q1);
		vec3f q02 = (q0 + q2);
		vec3f q12 = (q1 + q2);
		q01.Normalize();
		q02.Normalize();
		q12.Normalize();

		subdivided[0].InitUnit(q0, q01, q02, vec3f(0.f));
		subdivided[1].InitUnit(q01, q1, q12, vec3f(0.f));
		subdivided[2].InitUnit(q02, q12, q2, vec3f(0.f));
		subdivided[3].InitUnit(q12, q02, q01, vec3f(0.f));

		return 4;
	}

	vec3f SamplePoint(f32 u1, f32 u2) const {
		const f32 su1 = std::sqrtf(u1);
		const vec2f bary(1.f - su1, u2 * su1);

		return q0 * bary.x + q1 * bary.y + q2 * (1.f - bary.x - bary.y);
	}

private:
	void ComputeArea() {
		const vec3f v1 = q1 - q0, v2 = q2 - q0;
		const vec3f n1 = v1.Cross(v2);

		area = std::fabsf(n1.Dot(unitNormal)) * 0.5f;
	}
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
	f32   hx, hy;	// half-width and -height of the rectangle

	vec3f p0, p1, p2, p3;	// world-space corners of the rectangle

	/// Returns a random point on the rectangle
	vec3f SamplePoint(f32 u1, f32 u2) const;
	
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