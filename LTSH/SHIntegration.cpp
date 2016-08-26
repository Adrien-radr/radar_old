#include "SHIntegration.h"

bool SHInt::Init(Scene *scene, u32 band) {
	nBand = band;
	nCoeff = band * band;
	gameScene = scene;

	shCoeffs.resize(nCoeff);

	Material::Desc mat_desc(col3f(0.33f, 0.2f, 0), col3f(1, 0.8f, 0), col3f(1, 0, 0), 0.4f);
	mat_desc.ltcMatrixPath = "../../data/ltc_mat.dds";
	mat_desc.ltcAmplitudePath = "../../data/ltc_amp.dds";
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

void SHInt::UpdateCoords(const vec3f &position, const vec3f &normal) {
	// place at position, shifted in the normal's direction a bit
	vec3f pos = position + normal * 1.1f;

	Object::Desc *od = gameScene->GetObject(shObj);
	od->Identity();
	od->Translate(pos);
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