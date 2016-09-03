#include "SHIntegration.h"
#include "src/common/SHEval.h"
#include "src/common/sampling.h"

#include <algorithm>

#pragma optimize("",off)
static const float g_SubdivThreshold = 0.866f; // subdivide triangles having edges larger than 60 degrees

bool SHInt::Init(Scene *scene, u32 band) {
	nBand = band;
	nCoeff = band * band;
	gameScene = scene;

	integrationPos = vec3f(0, 0, 0);
	integrationNrm = vec3f(0, 1, 0);

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

	ltcMat = new float[32 * 32 * 4]; // 32x32 RGBA32F
	ltcAmp = new float[32 * 32 * 2]; // 32x32 RG32F

	// Get ltc data from GL textures
	Material::Data *matData = gameScene->GetMaterial(material);

	Render::Texture::GetData(matData->ltcMatrix, Render::Texture::RGBA32F, ltcMat);
	Render::Texture::GetData(matData->ltcAmplitude, Render::Texture::RG32F, ltcAmp);

	shNormalization = false;
	GGXexponent = 0.5f;
	integrationMethod = TriSamplingUnit;
	numSamples = 1024;
	usedBRDF = BRDF_Diffuse;

	return true;
}

SHInt::~SHInt() {
	delete[] ltcMat;
	delete[] ltcAmp;
}

void SHInt::Recompute() {
	integrationView = gameScene->GetCamera().position - integrationPos;
	integrationView.Normalize();

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

	Render::Mesh::Handle shVis = Render::Mesh::BuildSHVisualization(&shCoeffs[0], nBand, "SHVis1", shNormalization);
	if (shVis < 0) {
		LogErr("Error creating sh vis mesh");
		return;
	}

	od->AddSubmesh(shVis, material);
}

f32 SHInt::IntegrateTrisLTC(const AreaLight::UniformBufferData &al, std::vector<f32> &shvals) {
	std::vector<float> shtmp(nCoeff);

	vec3f points[4];
	AreaLight::GetVertices(al, points);

	// Integrate the polygon for diffuse & ggx brdf
	f32 contrib = 0.f;

	if (usedBRDF & (AreaLightBRDF::BRDF_Diffuse | AreaLightBRDF::BRDF_Both)) {
		mat3f MinvDiff; // identity for diffuse
		contrib += Render::BRDF::LTC_Evaluate(integrationNrm, integrationView, integrationPos, MinvDiff, points, false);
	}
	if (usedBRDF & (AreaLightBRDF::BRDF_GGX | AreaLightBRDF::BRDF_Both)) {
		// indexing variables
		const f32 NdotV = std::max(0.f, integrationNrm.Dot(integrationView));
		const f32 roughness = std::max(1e-3f, 1.f - GGXexponent);
		const f32 Ks = 1.f;

		const vec2f ltcCoords = Render::BRDF::LTC_Coords(NdotV, roughness, 32);
		const mat3f MinvSpec = Render::BRDF::LTC_Matrix(ltcMat, ltcCoords, 32);
		const vec2f schlick = BilinearLookup<vec2f>(ltcAmp, ltcCoords, vec2i(32, 32));

		f32 spec = Render::BRDF::LTC_Evaluate(integrationNrm, integrationView, integrationPos, MinvSpec, points, false);
		spec *= Ks * schlick.x + (1.f - Ks) * schlick.y;

		contrib += spec;
	}

	// project AL's center with integration weight onto SH
	vec3f alBary = al.position - integrationPos;
	alBary.Normalize();

	//for (u32 i = 0; i < 4; ++i) {
		//vec3f rayDir = points[i] - integrationPos;
		//rayDir.Normalize();

	vec3f rayDir = alBary;
	SHEval(nBand, rayDir.x, rayDir.z, rayDir.y, &shtmp[0]);

	for (u32 j = 0; j < nCoeff; ++j) {
		shvals[j] += shtmp[j] * contrib;
	}
	//}

	return 1.f / (2.f * M_PI);
}

f32 SHInt::IntegrateTrisSampling(const AreaLight::UniformBufferData &al, std::vector<f32> &shvals, bool wsSampling) {
	static const u32 triIdx[2][3] = { {0, 1, 2}, {0, 2, 3} };
	static const u32 numTri = 2; // 2 tris per arealight

	const u32 sampleCount = numSamples / numTri;

	std::vector<float> shtmp(nCoeff);

	vec3f points[4];
	AreaLight::GetVertices(al, points);
	const vec3f pN(al.plane.x, al.plane.y, al.plane.z);

	const f32 NdotV = std::max(0.f, integrationNrm.Dot(integrationView));
	const f32 roughness = std::max(1e-3f, 1.f - GGXexponent);

	Triangle subdivided[4];
	u32 numSubdiv;

	// Accum
	for (u32 tri = 0; tri < numTri; ++tri) {
		if (wsSampling) {
			subdivided[0].InitWS(pN, points[triIdx[tri][0]], points[triIdx[tri][1]], points[triIdx[tri][2]], integrationPos);

			numSubdiv = 1;
		} 
		else {
			Triangle triangle;
			triangle.InitUnit(points[triIdx[tri][0]], points[triIdx[tri][1]], points[triIdx[tri][2]], integrationPos);

			// Subdivide projected triangle if too large for robust integration
			if (triangle.distToOrigin() > g_SubdivThreshold) {
				subdivided[0] = triangle;
				numSubdiv = 1;
			}
			else {
				numSubdiv = triangle.Subdivide4(subdivided);
			}
		}

		// Loop over the subdivided triangles (or the single unsubdivided triangle)
		for (u32 subtri = 0; subtri < numSubdiv; ++subtri) {
			const Triangle &sub = subdivided[subtri];
			const u32 sc = sampleCount / numSubdiv;

			// Sample triangle
			for (u32 i = 0; i < sc; ++i) {
				vec2f randVec = Random::Vec2f();

				vec3f rayDir;
				f32 invPdf = sub.SampleDir(rayDir, randVec.x, randVec.y) * sub.area / sc;
				//f32 NdotL = rayDir.Dot(integrationNrm);

				// Fill SH with impulse from that direction
				if (invPdf > 0.f) {// && NdotL > 0.f) {
					SHEval(nBand, rayDir.x, rayDir.z, rayDir.y, &shtmp[0]);

					/*
					f32 contrib = 0.f;

					if (usedBRDF & (AreaLightBRDF::BRDF_Diffuse | AreaLightBRDF::BRDF_Both)) {
						contrib += Render::BRDF::Diffuse();
					}
					if (usedBRDF & (AreaLightBRDF::BRDF_GGX | AreaLightBRDF::BRDF_Both)) {
						vec3f H = integrationView + rayDir;
						H.Normalize();

						const f32 NdotH = std::max(0.f, integrationNrm.Dot(H));
						const f32 LdotH = std::max(0.f, rayDir.Dot(H));
						contrib += Render::BRDF::GGX(NdotL, NdotV, NdotH, LdotH, roughness, vec3f(1, 1, 1)).x;
					}
					*/

					for (u32 j = 0; j < nCoeff; ++j) {
						shvals[j] += shtmp[j] * invPdf;// *NdotL * contrib;
					}
				}
			}
		}
	}
	
	return 1.f;
}

f32 SHInt::IntegrateLight(const AreaLight::UniformBufferData &al, std::vector<f32> &shvals) {
	f32 ret;

	Rectangle arect = AreaLight::GetRectangle(al);

	switch (integrationMethod) {
	case UniformRandom:
		ret = arect.IntegrateRandom(integrationPos, integrationNrm, numSamples, shvals, nBand);
		break;
	case AngularStratification:
		ret = arect.IntegrateAngularStratification(integrationPos, integrationNrm, numSamples, shvals, nBand);
		break;
	case TriSamplingUnit:
		ret = IntegrateTrisSampling(al, shvals, false);
		break;
	case TriSamplingWS:
		ret = IntegrateTrisSampling(al, shvals, true);
		break;
	case LTCAnalytic:
		ret = IntegrateTrisLTC(al, shvals);
		break;
	default:
		ret = 0.f;
		LogErr("Unknown Tri integration method.");
		break;
	}

	return ret;
}

void SHInt::IntegrateAreaLights() {
	std::vector<float> shvals(nCoeff);
	std::vector<float> shtmp(nCoeff);
	std::fill_n(shvals.begin(), nCoeff, 0.f);

	f32 wtSum = 0.f;

	const AreaLight::UniformBufferData *al = gameScene->GetAreaLightUBO(areaLights[0]);

	if (al && !AreaLight::Cull(*al, integrationPos, integrationNrm)) {
		vec3f points[4];
		AreaLight::GetVertices(*al, points);

		// Transform as spherical polygon
		for (u32 i = 0; i < 4; ++i) {
			points[i] -= integrationPos;
			points[i].Normalize();
		}

		Polygon p1;
		p1.edges.push_back(Edge{ points[0], points[1] });
		p1.edges.push_back(Edge{ points[1], points[2] });
		p1.edges.push_back(Edge{ points[2], points[0] });
		Polygon p2;
		p2.edges.push_back(Edge{ points[0], points[2] });
		p2.edges.push_back(Edge{ points[2], points[3] });
		p2.edges.push_back(Edge{ points[3], points[0] });

		const u32 dirCount = 2 * nBand + 1;
		const f32 norm = 1.f / (f32) dirCount;
		const u32 order = 2 * nBand + 1;

#if 1
		for (u32 i = 0; i < areaLights.size(); ++i) {
			const AreaLight::UniformBufferData *al = gameScene->GetAreaLightUBO(areaLights[i]);

			if (al && !AreaLight::Cull(*al, integrationPos, integrationNrm)) {
				std::fill_n(shtmp.begin(), nCoeff, 0.f);

				f32 wt = IntegrateLight(*al, shtmp);
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

#elif
		std::vector<vec3f> dirs;
		Sampling::SampleSphereRandom(dirs, dirCount);

		for (u32 i = 0; i < dirCount; ++i) {
			std::fill_n(shtmp.begin(), nCoeff, 0.f);
			SHEval(nBand, dirs[i].x, dirs[i].z, dirs[i].y, &shtmp[0]);

			f32 wt = 0.f;
			wt += p1.AxialMoment(dirs[i], order);
			wt += p2.AxialMoment(dirs[i], order);

			for (u32 j = 0; j < nCoeff; ++j) {
				shvals[j] += shtmp[j] * norm * wt;
			}
		}


#elif // testing random direction
		std::vector<vec3f> dirs;
		Sampling::SampleSphereRandom(dirs, dirCount);

		for (u32 i = 0; i < dirCount; ++i) {
			std::fill_n(shtmp.begin(), nCoeff, 0.f);
			SHEval(nBand, dirs[i].x, dirs[i].z, dirs[i].y, &shtmp[0]);

			for (u32 j = 0; j < nCoeff; ++j) {
				shvals[j] += shtmp[j] * norm * Sampling::SampleSphereRandomPDF();
			}
		}

#elif
#endif
	}
	// Update SH Coeffs
	UpdateData(shvals);
}