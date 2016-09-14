#pragma once

#include "src/device.h"
#include "src/scene.h"
#include "eigen/Eigen/Core"

enum AreaLightIntegrationMethod {
	UniformRandom,
	AngularStratification,
	SphericalRectangles,
	TriSamplingUnit,
	TriSamplingWS,
	TangentPlane,
	ArvoMoments,
	LTCAnalytic
};

enum AreaLightBRDF {
	BRDF_Diffuse = 1,
	BRDF_GGX = 2,
	BRDF_Both = 4
};

class SHInt {
public:
	SHInt() : integrationPos(0), integrationNrm(0, 1, 0), integrationView(0, -1, 0),
		ltcMat(nullptr), ltcAmp(nullptr) {}
	~SHInt();
	bool Init(Scene *scene, u32 band);

	void AddAreaLight(AreaLight::Handle ah) {
		areaLights.push_back(ah);
	}

	void Recompute();
	void UpdateCoords(const vec3f &position, const vec3f &normal);
	void TestConvergence(const std::string &outputFileName, u32 numPasses, u32 numSamplesEqual, f32 numSecondsEqual);

	void SetGGXExponent(f32 exp) { GGXexponent = exp; }
	void UseSHNormalization(bool b) { shNormalization = b; }
	void SetIntegrationMethod(AreaLightIntegrationMethod m) { integrationMethod = m; }
	void SetSampleCount(u32 sc) { numSamples = sc; }
	void SetBRDF(AreaLightBRDF brdf) { usedBRDF = brdf; }

private:
	f32 IntegrateLight(const AreaLight::UniformBufferData &al, std::vector<f32> &shvals);
	f32 IntegrateLightSmart(const AreaLight::UniformBufferData &al, const Rectangle &rect, const SphericalRectangle &sphrect, 
		const Polygon &P, const PlanarRectangle &prect, std::vector<f32> &shvals);
	f32 IntegrateTrisSampling(const AreaLight::UniformBufferData &al, std::vector<f32> &shvals, bool wsSampling);
	f32 IntegrateTrisLTC(const AreaLight::UniformBufferData &al, std::vector<f32> &shvals);
	f32 IntegrateArvoMoments(const Polygon &P, std::vector<f32> &shvals);
	void IntegrateAreaLights();


	void UpdateData(const std::vector<float> &coeffs);

private:
	u32 nBand;
	u32 nCoeff;
	std::vector<float> shCoeffs;

	Object::Handle shObj;
	Material::Handle material;

	vec3f integrationPos;
	vec3f integrationNrm;
	vec3f integrationView;

	bool shNormalization;
	f32  GGXexponent;
	AreaLightIntegrationMethod integrationMethod;
	AreaLightBRDF usedBRDF;
	u32 numSamples;

	// Precomputed stuff for analytical methods
	f32 *ltcMat;
	f32 *ltcAmp;

	Eigen::Matrix<typename float, Eigen::Dynamic, Eigen::Dynamic> APMap;
	std::vector<vec3f> sampleDirs;

	Random::Pool<vec2f> triPool; // (1 - sqrt(s), sqrt(s) * t) for triangle sampling

	Scene *gameScene;
	std::vector<AreaLight::Handle> areaLights;
};