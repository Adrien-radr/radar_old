#pragma once

#include "src/device.h"
#include "src/scene.h"

class SHInt {
public:
	bool Init(Scene *scene, u32 band);

	void AddAreaLight(AreaLight::Handle ah) {
		areaLights.push_back(ah);
	}

	void UpdateCoords(const vec3f &position, const vec3f &normal);
	void UpdateData(const std::vector<float> &coeffs);

private:
	vec3f IntegrateTrisWS(const AreaLight::UniformBufferData &al, std::vector<f32> &shvals);
	void IntegrateAreaLightsWS();
	void IntegrateAreaLightsUnit();
private:
	u32 nBand;
	u32 nCoeff;
	std::vector<float> shCoeffs;

	Object::Handle shObj;
	Material::Handle material;

	vec3f integrationPos;
	vec3f integrationNrm;

	Scene *gameScene;
	std::vector<AreaLight::Handle> areaLights;
};