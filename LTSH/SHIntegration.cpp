#include "SHIntegration.h"
#include "src/common/SHEval.h"

#pragma optimize("",off)

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
	vec3f p0, p1, p2;
	vec3f q0, q1, q2;
	vec3f unitNormal;
	f32 area;
	f32 solidAngle;

	void InitUnit(const AreaLight::UniformBufferData &al, const vec3f &p0, const vec3f &p1, const vec3f &p2, const vec3f &intPos) {
		this->p0 = p0;
		this->p1 = p1;
		this->p2 = p2;
		q0 = p0 - intPos;
		q1 = p1 - intPos;
		q2 = p2 - intPos;
		q0.Normalize();
		q1.Normalize();
		q2.Normalize();

		const vec3f d1 = q1 - q0;
		const vec3f d2 = q2 - q0;

		const vec3f nrm = d1.Cross(d2);
		const f32 nrmLen = sqrtf(nrm.Dot(nrm));
		area = solidAngle = nrmLen / 2.f;

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

		this->p0 = p0;
		this->p1 = p1;
		this->p2 = p2;
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
	f32 GetSample(vec3f &rayUnitDir, const f32 s, const f32 t) {
		const vec3f rayDir = q0 * (1.f - s - t) + q1 * s + q2 * t;
		const f32 rayLenSqr = rayDir.Dot(rayDir);
		const f32 rayLen = std::sqrtf(rayLenSqr);
		rayUnitDir = rayDir / rayLen;

		return -rayDir.Dot(unitNormal) / (rayLenSqr * rayLen);
	}

private:
	void ComputeArea() {
		const vec3f v1 = p1 - p0, v2 = p2 - p0;
		const vec3f n1 = v1.Cross(v2);

		area = std::fabsf(n1.Dot(unitNormal)) * 0.5f;
	}

};



f32 SHInt::IntegrateTris(const AreaLight::UniformBufferData &al, std::vector<f32> &shvals) {
	static u32 triIdx[2][3] = { {0, 1, 2}, {0, 2, 3} };

	std::vector<float> shtmp(nCoeff);

	const u32 sampleCount = 1024;

	vec3f points[4];
	AreaLight::GetVertices(al, points);

	vec3f sum(0.f);
	f32 areaLightNorm = 0.f;
	const u32 numTri = 2; // 2 tris per arealight

	// Accum
	for (u32 tri = 0; tri < numTri; ++tri) {
		Triangle triangle;

		if (wsSampling)
			triangle.InitWS(al, points[triIdx[tri][0]], points[triIdx[tri][1]], points[triIdx[tri][2]], integrationPos);
		else
			triangle.InitUnit(al, points[triIdx[tri][0]], points[triIdx[tri][1]], points[triIdx[tri][2]], integrationPos);

		f32 triWeight = Luminance(al.Ld) * triangle.solidAngle;
		f32 triPdf = triangle.area / triWeight;

		if (triPdf > 0.f) {
			// Sample triangle
			for (u32 i = 0; i < sampleCount; ++i) {
				vec3f rayDir;
				vec2f randVec = Random::Vec2f();

				f32 weight = triangle.GetSample(rayDir, randVec.x, randVec.y) * triPdf;


				if (weight > 0.f && rayDir.Dot(integrationNrm) > 0.f) {
					areaLightNorm += 1.f/triWeight;// 1.f / triPdf;

					std::fill_n(shtmp.begin(), nCoeff, 0.f);
					SHEval(nBand, rayDir.x, rayDir.z, rayDir.y, &shtmp[0]);

					for (u32 j = 0; j < nCoeff; ++j) {
						shvals[j] += shtmp[j] * weight;
					}

					sum += vec3f(1) * weight;
				}
			}
		}
	}

	// Reduce
	//for (u32 j = 0; j < nCoeff; ++j) {
		//shvals[j] *= areaLightNorm / (float)(numTri * sampleCount);
	//}

	//sum *= areaLightNorm / (float)(numTri * sampleCount);
	
	return areaLightNorm / (float)(numTri * sampleCount);
}

void SHInt::IntegrateAreaLights() {
	std::vector<float> shvals(nCoeff);
	std::vector<float> shtmp(nCoeff);
	std::fill_n(shvals.begin(), nCoeff, 0.f);

	f32 wtSum = 0.f;

	for (u32 i = 0; i < areaLights.size(); ++i) {
		const AreaLight::UniformBufferData *al = gameScene->GetAreaLightUBO(areaLights[i]);

		if (al) {
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