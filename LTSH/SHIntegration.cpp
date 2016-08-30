#include "SHIntegration.h"
#include "src/common/SHEval.h"
#include <algorithm>

#pragma optimize("",off)
static const float g_SubdivThreshold = 0.866f; // subdivide triangles having edges larger than 60 degrees

/////////////////////////////////////////////////////////////////////////////////////////////////
vec3f fresnelSchlick(vec3f f0, float f90, float u) {
	return f0 + (vec3f(f90) - f0) * std::pow(1.f - u, 5.f);
}

vec3f GGX(float NdotL, float NdotV, float NdotH, float LdotH, float roughness, vec3f F0) {
	float alpha2 = roughness * roughness;

	// F 
	vec3f F = fresnelSchlick(F0, 1.f, LdotH);

	// D (Trowbridge-Reitz). Divide by PI at the end
	float denom = NdotH * NdotH * (alpha2 - 1.f) + 1.f;
	float D = alpha2 / (M_PI * denom * denom);

	// G (Smith GGX - Height-correlated)
	float lambda_GGXV = NdotL * std::sqrtf((-NdotV * alpha2 + NdotV) * NdotV + alpha2);
	float lambda_GGXL = NdotV * std::sqrtf((-NdotL * alpha2 + NdotL) * NdotL + alpha2);
	// float G = G_schlick_GGX(k, NdotL) * G_schlick_GGX(k, NdotV);

	// optimized G(NdotL) * G(NdotV) / (4 NdotV NdotL)
	// float G = 1.0 / (4.0 * (NdotL * (1 - k) + k) * (NdotV * (1 - k) + k));
	float G = 0.5f / (lambda_GGXL + lambda_GGXV);

	return F * G * D;
}

// LTC STUFF
vec2f LTCCoords(float NdotV, float roughness) {
	float theta = acos(NdotV);
	vec2f coords = vec2f(roughness, theta / (0.5f*M_PI));

	// corrected lookup (x - 1 + 0.5)
	const float LUT_SIZE = 32.f;
	coords = coords * (LUT_SIZE - 1.f) / LUT_SIZE + 0.5f / LUT_SIZE;

	return coords;
}

mat3f LTCMatrix(f32 *ltcMatrix, vec2f coord) {
	// bilinear interpolation of the ltcmatrix 

	//tmp : nearest
	const vec2i coordI((int)(32.f * coord.x), (int)(32.f * coord.y));
	const vec4f *ltcMat4f = (vec4f*)ltcMatrix;

	const u32 index = std::min(32 * 32 - 1, coordI.y * 32 + coordI.x);
	const vec4f v = ltcMat4f[index];
	//v = v.rgba;
	// bgra
	// rgba

	// inverse of 
	// a 0 b
	// 0 c 0
	// d 0 1
	const mat3f Minv = mat3f(
		vec3f(1,     0, v.y),       // 1st column
		vec3f(0,   v.z,   0),
		vec3f(v.w,   0, v.x)
	);

	return Minv;
}

f32 IntegrateEdge(const vec3f &P1, const vec3f &P2) {
	f32 cosTheta = P1.Dot(P2);
	cosTheta = std::max(-0.9999f, std::min(0.9999f, cosTheta));

	f32 theta = std::acosf(cosTheta);
	f32 r = (P1.Cross(P2)).z * theta / std::sinf(theta);
	return r;
}

f32 LTCEvaluate(const vec3f &N, const vec3f &V, const vec3f &P, const mat3f &Minv, const vec3f points[4], bool twoSided) {
	// N orthonormal frame
	vec3f T, B;
	T = V - N * V.Dot(N);
	T.Normalize();
	B = N.Cross(T);

	mat3f LMinv = Minv * mat3f(T, B, N).Transpose();

	// polygon in TBN frame 
	vec3f L[5];
	L[0] = LMinv * (points[0] - P);
	L[1] = LMinv * (points[1] - P);
	L[2] = LMinv * (points[2] - P);
	L[3] = LMinv * (points[3] - P);
	L[4] = L[3];

	//int n;
	//ClipQuadToHorizon(L, n);

	//if (n == 0)
		//return vec3(0, 0, 0);

	// polygon projected onto sphere
	L[0].Normalize();
	L[1].Normalize();
	L[2].Normalize();
	L[3].Normalize();
	L[4].Normalize();

	// integration
	float sum = 0.0;

	sum += IntegrateEdge(L[0], L[1]);
	sum += IntegrateEdge(L[1], L[2]);
	sum += IntegrateEdge(L[2], L[3]);
	sum += IntegrateEdge(L[3], L[0]);

	sum = twoSided ? std::abs(sum) : std::max(0.f, -sum);

	return sum;
}
/////////////////////////////////////////////////////////////////////////////////////////////////

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

	wsSampling = false;
	shNormalization = false;
	GGXexponent = 0.5f;
	integrationMethod = TriSampling;
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

f32 SHInt::IntegrateTrisLTC(const AreaLight::UniformBufferData &al, std::vector<f32> &shvals) {
	std::vector<float> shtmp(nCoeff);

	vec3f points[4];
	AreaLight::GetVertices(al, points);



	// Integrate the polygon for diffuse & ggx brdf
	f32 contrib = 0.f;

	if (usedBRDF & (AreaLightBRDF::BRDF_Diffuse | AreaLightBRDF::BRDF_Both)) {
		mat3f MinvDiff; // identity for diffuse
		contrib += LTCEvaluate(integrationNrm, integrationView, integrationPos, MinvDiff, points, false);
	}
	if (usedBRDF & (AreaLightBRDF::BRDF_GGX | AreaLightBRDF::BRDF_Both)) {
		// indexing variables
		const f32 NdotV = std::max(0.f, integrationNrm.Dot(integrationView));
		const f32 roughness = std::max(1e-3f, 1.f - GGXexponent);
		const f32 Ks = 1.f;

		const vec2f ltcCoords = LTCCoords(NdotV, roughness);
		const vec2i coordI((int)(32.f * ltcCoords.x), (int)(32.f * ltcCoords.y));

		// nearest ltcMat
		const mat3f MinvSpec = LTCMatrix(ltcMat, ltcCoords);

		// nearest ltcAmp
		const vec2f *ltcAmp2f = (vec2f*)ltcAmp;
		const u32 index = std::min(32 * 32 - 1, coordI.y * 32 + coordI.x);
		const vec2f schlick = ltcAmp[index];

		f32 spec = LTCEvaluate(integrationNrm, integrationView, integrationPos, MinvSpec, points, false);
		spec *= Ks * schlick.x + (1.f - Ks) * schlick.y;

		contrib += spec;
	}

	// project AL's center with integration weight onto SH
	vec3f alBary = al.position - integrationPos;
	alBary.Normalize();
	std::fill_n(shtmp.begin(), nCoeff, 0.f);

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

f32 SHInt::IntegrateTrisSampling(const AreaLight::UniformBufferData &al, std::vector<f32> &shvals) {
	static const u32 triIdx[2][3] = { {0, 1, 2}, {0, 2, 3} };
	static const u32 numTri = 2; // 2 tris per arealight

	const u32 sampleCount = numSamples / numTri;

	std::vector<float> shtmp(nCoeff);

	vec3f points[4];
	AreaLight::GetVertices(al, points);

	const f32 NdotV = std::max(0.f, integrationNrm.Dot(integrationView));
	const f32 roughness = std::max(1e-3f, 1.f - GGXexponent);

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
					f32 NdotL = rayDir.Dot(integrationNrm);

					// Fill SH with impulse from that direction
					if (weight > 0.f && NdotL > 0.f) {
						std::fill_n(shtmp.begin(), nCoeff, 0.f);
						SHEval(nBand, rayDir.x, rayDir.z, rayDir.y, &shtmp[0]);

						f32 contrib = 0.f;

						if (usedBRDF & (AreaLightBRDF::BRDF_Diffuse | AreaLightBRDF::BRDF_Both)) {
							contrib += 1.f / M_PI;
						}
						if (usedBRDF & (AreaLightBRDF::BRDF_GGX | AreaLightBRDF::BRDF_Both)) {
							vec3f H = integrationView + rayDir;
							H.Normalize();

							const f32 NdotH = std::max(0.f, integrationNrm.Dot(H));
							const f32 LdotH = std::max(0.f, rayDir.Dot(H));
							contrib += GGX(NdotL, NdotV, NdotH, LdotH, roughness, vec3f(1, 1, 1)).x;
						}

						for (u32 j = 0; j < nCoeff; ++j) {
							shvals[j] += shtmp[j] * weight * NdotL * contrib;
						}
					}
				}
			}
		}
	}
	
	return 1.f / (float)(numTri * sampleCount);
}

f32 SHInt::IntegrateTris(const AreaLight::UniformBufferData &al, std::vector<f32> &shvals) {
	f32 ret;

	switch (integrationMethod) {
	case TriSampling:
		ret = IntegrateTrisSampling(al, shvals);
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
	LogInfo(usedBRDF);
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