#pragma once
#include "common/common.h"

struct Triangle {
	vec3f q0, q1, q2;
	vec3f unitNormal;
	f32 area;
	f32 solidAngle;

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
		const vec3f rayDir = q0 * (1.f - s - t) + q1 * s + q2 * t;
		const f32 rayLenSqr = rayDir.Dot(rayDir);
		const f32 rayLen = std::sqrtf(rayLenSqr);
		rayUnitDir = rayDir / rayLen;

		return -rayDir.Dot(unitNormal) / (rayLenSqr * rayLen);
	}

	f32 distToOrigin() const {
		return -1.0f * unitNormal.Dot(q0);
	}

	// subdivided is always 4-length
	u32 Subdivide(Triangle *subdivided) const {
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

private:
	void ComputeArea() {
		const vec3f v1 = q1 - q0, v2 = q2 - q0;
		const vec3f n1 = v1.Cross(v2);

		area = std::fabsf(n1.Dot(unitNormal)) * 0.5f;
	}
};