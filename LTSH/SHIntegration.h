#pragma once

#include "src/device.h"
#include "src/scene.h"

class SHInt {
public:
	bool Init(Scene *scene, u32 band);

	void AddAreaLight(AreaLight::Handle ah) {
		areaLights.push_back(ah);
	}

	void Recompute();
	void UpdateCoords(const vec3f &position, const vec3f &normal);
	void UseWorldSpaceSampling(bool b) { wsSampling = b; }

private:
	f32 IntegrateTris(const AreaLight::UniformBufferData &al, std::vector<f32> &shvals);
	//vec3f IntegrateTrisUnit(const AreaLight::UniformBufferData &al, std::vector<f32> &shvals);
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
	bool wsSampling;

	Scene *gameScene;
	std::vector<AreaLight::Handle> areaLights;
};