#include "geometry.h"
#include "common/SHEval.h"

#include <algorithm>

#pragma optimize("", off)

static bool even(int n) {
	return !(n & 1);
}

static int sign(f32 b) {
	return (b >= 0) - (b < 0);
}

//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

vec3f Plane::RayIntersection(const vec3f &rayOrg, const vec3f &rayDir) {
	const f32 distance = N.Dot(P - rayOrg) / N.Dot(rayDir);
	return rayOrg + rayDir * distance;
}

//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

f32 Polygon::SolidAngle(const vec3f &A, const vec3f &B, const vec3f &C) {
	// Arvo solid angle : alpha + beta + gamma - pi
	// Oosterom & Strackee 83 method
	const vec3f tmp = A.Cross(B);
	const f32 num = std::fabsf(tmp.Dot(A));
	const f32 r1 = std::sqrtf(A.Dot(A));
	const f32 r2 = std::sqrtf(B.Dot(B));
	const f32 r3 = std::sqrtf(C.Dot(C));

	const f32 denom = r1 * r2 * r3 + A.Dot(B) * r3 + A.Dot(C) * r2 + B.Dot(C) * r1;

	// tan(phi/2) = num/denom
	f32 halPhi = std::atan2f(num, denom);
	if (halPhi < 0.f) halPhi += M_PI;

	return 2.f * halPhi;
}

f32 Polygon::CosSumIntegral(f32 x, f32 y, f32 c, int nMin, int nMax) {
	const f32 sinx = std::sinf(x);
	const f32 siny = std::sinf(y);

	int i = even(nMax) ? 0 : 1;
	f32 F = even(nMax) ? y - x : siny - sinx;
	f32 S = 0.f;

	while (i <= nMax) {
		if (i >= nMin)
			S += std::pow(c, i) * F;

		const f32 T = std::pow(std::cosf(y), i + 1) * siny - std::pow(std::cosf(x), i + 1) * sinx;
		F = (T + (i + 1)*F) / (i + 2);
		i += 2;
	}

	return S;
}

f32 Polygon::LineIntegral(const vec3f & A, const vec3f & B, const vec3f & w, int nMin, int nMax) {
	const f32 eps = 1e-7f;
	if ((nMax < 0) || ((fabs(w.Dot(A)) < eps) && (fabs(w.Dot(B)) < eps))) {
		return 0.f;
	}

	vec3f s = A;
	s.Normalize();

	const f32 sDotB = s.Dot(B);

	vec3f t = B - s * sDotB;
	t.Normalize();

	const f32 a = w.Dot(s);
	const f32 b = w.Dot(t);
	const f32 c = std::sqrtf(a*a + b*b);

	const f32 cos_l = sDotB / B.Dot(B);
	const f32 l = std::acosf(std::max(-1.f, std::min(1.f, cos_l)));
	const f32 phi = sign(b) * std::acosf(a / c);

	return CosSumIntegral(-phi, l - phi, c, nMin, nMax);
}

f32 Polygon::BoundaryIntegral(const vec3f & w, const vec3f & v, int nMin, int nMax) {
	f32 b = 0.f;

	for (Edge &e : edges) {
		vec3f n = e.A.Cross(e.B);
		n.Normalize();

		b += n.Dot(v) * LineIntegral(e.A, e.B, w, nMin, nMax);
	}

	return b;
}

f32 Polygon::AxialMoment(const vec3f & w, int order) {
	Assert(edges.size() == 3); // only tris for the moment (Arvo impl)

	f32 a = -BoundaryIntegral(w, w, 0, order - 1);

	if (even(order)) {
		const vec3f A = edges[0].A;
		const vec3f B = edges[1].A;
		const vec3f C = edges[2].A;
		a += SolidAngle(A, B, C);
	}

	return a / (order + 1);
}

f32 Polygon::DoubleAxisMoment(const vec3f & w, const vec3f & v, int order) {
	if(order == 0)
		return AxialMoment(w, order);

	f32 a = AxialMoment(w, order - 1);
	f32 b = BoundaryIntegral(w, v, order, order);

	return (order * a * w.Dot(v) - b) / (order + 2);
}

//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

void Triangle::InitUnit(const vec3f &p0, const vec3f &p1, const vec3f &p2, const vec3f &intPos) {
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
	} else {
		unitNormal = nrm / nrmLen;
	}
}

/// Same as InitUnit, except the resulting triangle is kept in world space. 
/// triNormal is the normal of the plane collinear with the triangle
void Triangle::InitWS(const vec3f &triNormal, const vec3f &p0, const vec3f &p1, const vec3f &p2, const vec3f &intPos) {
	unitNormal = triNormal;

	q0 = p0 - intPos;
	q1 = p1 - intPos;
	q2 = p2 - intPos;

	ComputeArea();

	const vec3f bary = (q0 + q1 + q2) * 0.3333333333333f;
	const f32 rayLenSqr = bary.Dot(bary);
	const f32 rayLen = std::sqrtf(rayLenSqr);
	solidAngle = -bary.Dot(unitNormal) * (area / (rayLenSqr * rayLen));
}

u32 Triangle::Subdivide4(Triangle subdivided[4]) const {
	vec3f q01 = (q0 + q1);
	vec3f q02 = (q0 + q2);
	vec3f q12 = (q1 + q2);

	subdivided[0].InitUnit(q0, q01, q02, vec3f(0.f));
	subdivided[1].InitUnit(q01, q1, q12, vec3f(0.f));
	subdivided[2].InitUnit(q02, q12, q2, vec3f(0.f));
	subdivided[3].InitUnit(q01, q12, q02, vec3f(0.f));

	return 4;
}

void Triangle::ComputeArea() {
	const vec3f v1 = q1 - q0, v2 = q2 - q0;
	const vec3f n1 = v1.Cross(v2);

	area = std::fabsf(n1.Dot(unitNormal)) * 0.5f;
}

vec3f Triangle::SamplePoint(f32 u1, f32 u2) const {
	const f32 su1 = std::sqrtf(u1);
	const vec2f bary(1.f - su1, u2 * su1);

	return q0 * bary.x + q1 * bary.y + q2 * (1.f - bary.x - bary.y);
}

f32 Triangle::SampleDir(vec3f &rayDir, const f32 s, const f32 t) const {
	rayDir = SamplePoint(s, t);
	const f32 rayLenSqr = rayDir.Dot(rayDir);
	const f32 rayLen = std::sqrtf(rayLenSqr);
	rayDir /= rayLen;

	const f32 costheta = -unitNormal.Dot(rayDir);

	return costheta / rayLenSqr;
}

//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

vec3f Rectangle::SamplePoint(f32 u1, f32 u2) const{
	const vec3f bl = position - ex * hx - ey * hy;
	const f32 w = hx * 2.f;
	const f32 h = hy * 2.f;

	return bl + ex * w * u1 + ey * h * u2;
}

f32 Rectangle::SampleDir(vec3f & rayDir, const vec3f & pos, const f32 u1, const f32 u2) const {
	const vec3f pk = SamplePoint(u1, u2);
	rayDir = pk - pos;
	const f32 rayLenSq = rayDir.Dot(rayDir);
	const f32 rayLen = std::sqrtf(rayLenSq);
	rayDir /= rayLen;

	const f32 costheta = -ez.Dot(rayDir);

	return costheta / rayLenSq;
}

f32 Rectangle::SolidAngle(const vec3f & integrationPos) const {
	const vec3f q0 = p0 - integrationPos;
	const vec3f q1 = p1 - integrationPos;
	const vec3f q2 = p2 - integrationPos;
	const vec3f q3 = p3 - integrationPos;

	vec3f n0 = q0.Cross(q1);
	vec3f n1 = q1.Cross(q2);
	vec3f n2 = q2.Cross(q3);
	vec3f n3 = q3.Cross(q0);
	n0.Normalize();
	n1.Normalize();
	n2.Normalize();
	n3.Normalize();

	const f32 alpha = std::acosf(-n0.Dot(n1));
	const f32 beta = std::acosf(-n1.Dot(n2));
	const f32 gamma = std::acosf(-n2.Dot(n3));
	const f32 zeta = std::acosf(-n3.Dot(n0));

	return alpha + beta + gamma + zeta - 2 * M_PI;
}

f32 Rectangle::IntegrateStructuredSampling(const vec3f & integrationPos, const vec3f & integrationNrm) const {
	// Solving E(n) = Int_lightArea [ Lin <n.l> dl ] == lightArea * Lin * Average[<n.l>]
	// With Average[<n.l>] approximated with the average of the 4 corners and center of the rect

	// unit space solid angle (== unit space area)
	f32 solidAngle = SolidAngle(integrationPos);

	// unit space vectors to the 5 sample points

	vec3f q0 = p0 - integrationPos;
	vec3f q1 = p1 - integrationPos;
	vec3f q2 = p2 - integrationPos;
	vec3f q3 = p3 - integrationPos;
	vec3f q4 = position - integrationPos;

	q0.Normalize();
	q1.Normalize();
	q2.Normalize();
	q3.Normalize();
	q4.Normalize();

	// area * Average[<n.l>] (Lin is 1.0)
	return solidAngle * 0.2f * (
		std::max(0.f, q0.Dot(integrationNrm)) +
		std::max(0.f, q1.Dot(integrationNrm)) +
		std::max(0.f, q2.Dot(integrationNrm)) +
		std::max(0.f, q3.Dot(integrationNrm)) +
		std::max(0.f, q4.Dot(integrationNrm))
		);
}

f32 Rectangle::IntegrateMRP(const vec3f & integrationPos, const vec3f & integrationNrm) const {
	const vec3f d0p = -ez;
	const vec3f d1p = integrationNrm;

	const f32 nDotpN = std::max(0.f, integrationNrm.Dot(ez));

	vec3f d0 = d0p + integrationNrm * nDotpN;
	vec3f d1 = d1p - ez * nDotpN;
	d0.Normalize();
	d1.Normalize();

	vec3f dh = d0 + d1;
	dh.Normalize();

	Plane rectPlane = { position, ez };
	const vec3f pH = rectPlane.RayIntersection(integrationPos, dh);
	// todo : clamp pH in the rectangle defined by the light

	const f32 solidAngle = SolidAngle(integrationPos);

	vec3f rayDir = pH - integrationPos;
	rayDir.Normalize();

	return solidAngle * std::max(0.f, integrationNrm.Dot(rayDir));
}

f32 Rectangle::IntegrateAngularStratification(const vec3f & integrationPos, const vec3f & integrationNrm, u32 sampleCount, std::vector<f32> &shvals, int nBand) const {
	const u32 sampleCountX = (u32) std::sqrt((f32) sampleCount);
	const u32 sampleCountY = sampleCountX;

	// bottom left point
	const vec3f a0 = position - ex * hx - ey * hy;

	const vec3f W1 = position - ex * hx;
	const vec3f W2 = position + ex * hx;
	const vec3f H1 = position - ey * hy;
	const vec3f H2 = position + ey * hy;

	const f32 lw1_2 = W1.Dot(W1);
	const f32 lw2_2 = W2.Dot(W2);
	const f32 lh1_2 = H1.Dot(H1);
	const f32 lh2_2 = H2.Dot(H2);

	const f32 rwidth = 2.f * hx;
	const f32 rwidth_2 = rwidth * rwidth;
	const f32 rheight = 2.f * hy;
	const f32 rheight_2 = rheight * rheight;

	const f32 lw1 = std::sqrtf(lw1_2);
	const f32 lw2 = std::sqrtf(lw2_2);
	const f32 lh1 = std::sqrtf(lh1_2);
	const f32 lh2 = std::sqrtf(lh2_2);

	// normal pointing away from rectangle (in same direction as exiting light)
	vec3f unitNormal = ex.Cross(ey);
	unitNormal.Normalize();

	const f32 cosx = -W1.Dot(ex) / lw1;
	const f32 sinx = std::sqrtf(1.f - cosx * cosx);
	const f32 cosy = -H1.Dot(ey) / lh1;
	const f32 siny = std::sqrtf(1.f - cosy * cosy);

	const f32 dx = 1.f / sampleCountX;
	const f32 dy = 1.f / sampleCountY;

	const f32 theta = std::acosf((lw1_2 + lw2_2 - rwidth_2) * 0.5f / (lw1 * lw2));
	const f32 gamma = std::acosf((lh1_2 + lh2_2 - rheight_2) * 0.5f / (lh1 * lh2));
	const f32 theta_n = theta * dx;
	const f32 gamma_n = gamma * dy;

	const f32 tanW = std::tanf(theta_n);
	const f32 tanH = std::tanf(gamma_n);

	const int nCoeff = nBand * nBand;
	std::vector<f32> shtmp(nCoeff);

	// Marching over the equi angular rectangles
	f32 x1 = 0.f;
	f32 tanx1 = 0.f;
	for (u32 x = 0; x < sampleCountX; ++x) {
		const f32 tanx2 = (tanx1 + tanW) / (1.f - tanx1 * tanW);
		const f32 x2 = lw1 * tanx2 / (sinx + tanx2 * cosx);
		const f32 lx = x2 - x1;

		f32 y1 = 0.f;
		f32 tany1 = 0.f;
		for (u32 y = 0; y < sampleCountY; ++y) {
			const f32 tany2 = (tany1 + tanH) / (1.f - tany1 * tanH);
			const f32 y2 = lh1 * tany2 / (siny + tany2 * cosy);
			const f32 ly = y2 - y1;

			const f32 u1 = (x1 + Random::Float() * lx) / rwidth;
			const f32 u2 = (y1 + Random::Float() * ly) / rheight;

			vec3f rayDir;
			const f32 invPdf = SampleDir(rayDir, integrationPos, u1, u2) * lx * ly * sampleCount;

			if (invPdf > 0.f) {
				SHEval(nBand, rayDir.x, rayDir.z, rayDir.y, &shtmp[0]);

				for (int i = 0; i < nCoeff; ++i) {
					shvals[i] += shtmp[i] * invPdf; // constant luminance of 1 for now
				}
			}

			y1 = y2;
			tany1 = tany2;
		}

		x1 = x2;
		tanx1 = tanx2;
	}

	return 1.f / (f32) sampleCount;
}

f32 Rectangle::IntegrateRandom(const vec3f & integrationPos, const vec3f & integrationNrm, u32 sampleCount, std::vector<f32>& shvals, int nBand) const {
	// Rectangle area
	const f32 area = 4.f * hx * hy;

	// normal pointing away from rectangle (in same direction as exiting light)
	vec3f unitNormal = ex.Cross(ey);
	unitNormal.Normalize();

	const int nCoeff = nBand * nBand;
	std::vector<f32> shtmp(nCoeff);

	// costheta * A / r^3
	for (u32 i = 0; i < sampleCount; ++i) {
		const vec2f randV = Random::Vec2f();

		vec3f rayDir;
		const f32 invPdf = SampleDir(rayDir, integrationPos, randV.x, randV.y) * area;

		if (invPdf > 0.f) {
			SHEval(nBand, rayDir.x, rayDir.z, rayDir.y, &shtmp[0]);

			for (int j = 0; j < nCoeff; ++j) {
				shvals[j] += shtmp[j] * invPdf; // constant luminance of 1 for now
			}
		}
	}

	return 1.f / (f32) sampleCount;
}
