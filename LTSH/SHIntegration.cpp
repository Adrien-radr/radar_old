#include "SHIntegration.h"
#include "src/common/SHEval.h"

#pragma optimize("",off)
static const float g_SubdivThreshold = 0.866f; // subdivide triangles having edges larger than 60 degrees

bool SHInt::Init(Scene *scene, u32 band) {
	nBand = band;
	nCoeff = band * band;
	gameScene = scene;

	integrationPos = vec3f(0, 0, 0);
	integrationNrm = vec3f(0, 1, 0);
	wsSampling = true;

	shCoeffs.resize(nCoeff);

	Material::Desc mat_desc(col3f(0.6f, 0.25f, 0), col3f(1, 0.8f, 0), col3f(1, 0, 0), 0.4f);
	mat_desc.ltcMatrixPath = "../../data/ltc_mat.dds";
	mat_desc.ltcAmplitudePath = "../../data/ltc_amp.dds";
	mat_desc.dynamic = true;
	mat_desc.gbufferDraw = false;

	material = scene->Add(mat_desc);
	if (material < 0) {
		LogErr("Error adding SH Vis material");
		return false;
	}

	Object::Desc od(Render::Shader::SHADER_3D_MESH);
	shObj = scene->Add(od);
	if (shObj < 0) {
		LogErr("Error adding SH Vis object");
		return false;
	}

	return true;
}

void SHInt::Recompute() {
	IntegrateAreaLights();
}

void SHInt::UpdateCoords(const vec3f &position, const vec3f &normal) {
	// place at position, shifted in the normal's direction a bit
	vec3f pos = position + normal * 0.1f;

	integrationPos = position;
	integrationNrm = normal;

	Object::Desc *od = gameScene->GetObject(shObj);
	od->Identity();
	od->Translate(pos);

	// Call Integration over new position
	Recompute();
}

void SHInt::UpdateData(const std::vector<float> &coeffs) {
	std::fill_n(shCoeffs.begin(), nCoeff, 0.f);
	std::copy_n(coeffs.begin(), nCoeff, shCoeffs.begin());

	Object::Desc *od = gameScene->GetObject(shObj);

	od->DestroyData();
	od->ClearSubmeshes();

	Render::Mesh::Handle shVis = Render::Mesh::BuildSHVisualization(&shCoeffs[0], nBand, "SHVis1");
	if (shVis < 0) {
		LogErr("Error creating sh vis mesh");
		return;
	}

	od->AddSubmesh(shVis, material);
}

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
	
	void InitWS(const AreaLight::UniformBufferData &al, const vec3f &p0, const vec3f &p1, const vec3f &p2, const vec3f &intPos) {
		unitNormal = vec3f(al.plane.x, al.plane.y, al.plane.z);

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

f32 SHInt::IntegrateTris(const AreaLight::UniformBufferData &al, std::vector<f32> &shvals) {
	static const u32 triIdx[2][3] = { {0, 1, 2}, {0, 2, 3} };
	static const u32 numTri = 2; // 2 tris per arealight

	const u32 sampleCount = 1024 / numTri;

	std::vector<float> shtmp(nCoeff);

	vec3f points[4];
	AreaLight::GetVertices(al, points);

	//vec3f sum(0.f);

	Triangle subdivided[4];
	u32 numSubdiv;

	// Accum
	for (u32 tri = 0; tri < numTri; ++tri) {
		Triangle triangle;

		if (wsSampling) {
			triangle.InitWS(al, points[triIdx[tri][0]], points[triIdx[tri][1]], points[triIdx[tri][2]], integrationPos);
			subdivided[0] = triangle;
			numSubdiv = 1;
		} 
		else {
			triangle.InitUnit(points[triIdx[tri][0]], points[triIdx[tri][1]], points[triIdx[tri][2]], integrationPos);

			// Subdivide projected triangle if too large for robust integration
			if (triangle.distToOrigin() > 1e-5f/* g_SubdivThreshold*/) {
				numSubdiv = triangle.Subdivide(subdivided);
			}
			else {
				subdivided[0] = triangle;
				numSubdiv = 1;
			}
		}

		// Loop over the subdivided triangles (or the single unsubdivided triangle)
		for (u32 subtri = 0; subtri < numSubdiv; ++subtri) {
			Triangle &sub = subdivided[subtri];
			const f32 triWeight = sub.area * Luminance(al.Ld);


			if (triWeight > 0.0f) {
				const u32 sc = sampleCount / numSubdiv;

				// Sample triangle
				for (u32 i = 0; i < sc; ++i) {
					vec3f rayDir;
					vec2f randVec = Random::Vec2f();

					f32 weight = sub.GetSample(rayDir, randVec.x, randVec.y) * triWeight;

					// Fill SH with impulse from that direction
					if (weight > 0.f && rayDir.Dot(integrationNrm) > 0.f) {
						std::fill_n(shtmp.begin(), nCoeff, 0.f);
						SHEval(nBand, rayDir.x, rayDir.z, rayDir.y, &shtmp[0]);

						for (u32 j = 0; j < nCoeff; ++j) {
							shvals[j] += shtmp[j] * weight;
						}

						//sum += vec3f(1.f) * weight;
					}
				}
			}
		}
	}
	
	return 1.f / (float)(numTri * sampleCount);
}

void SHInt::IntegrateAreaLights() {
	std::vector<float> shvals(nCoeff);
	std::vector<float> shtmp(nCoeff);
	std::fill_n(shvals.begin(), nCoeff, 0.f);

	f32 wtSum = 0.f;

	for (u32 i = 0; i < areaLights.size(); ++i) {
		const AreaLight::UniformBufferData *al = gameScene->GetAreaLightUBO(areaLights[i]);

		if (al && !AreaLight::Cull(*al, integrationPos, integrationNrm)) {
			std::fill_n(shtmp.begin(), nCoeff, 0.f);

			f32 wt = IntegrateTris(*al, shtmp);
			wtSum += wt;

			// update SH
			for (u32 j = 0; j < nCoeff; ++j) {
				shvals[j] += shtmp[j];
			}
		}
	}

	// Reduce
	for (u32 j = 0; j < nCoeff; ++j) {
		shvals[j] *= wtSum / (f32) areaLights.size();
	}

	// Update SH Coeffs
	UpdateData(shvals);
}