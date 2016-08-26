#include "SHIntegration.h"

AreaLight::Handle alh = -1;
AreaLight::Handle alh2;
AreaLight::Handle alh3;
vec3f alPos(40., 3, -20);
vec3f alPos2(20., 7.5, 20);


SHInt sh1;

bool MakeLights(Scene *scene) {
	PointLight::Desc light;
	AreaLight::Desc al;
	PointLight::Handle light_h;

	// Lights
	light.position = vec3f(9.5, 5, 10);
	light.Ld = vec3f(1.5, 1, 0);
	light.radius = 1000.f;

	light_h = scene->Add(light);
	if(light_h < 0) goto err; 

	light.position = vec3f(90, 15, -3);
	light.Ld = vec3f(1.2, 1.2, 3);
	light.radius = 1000.f;

	light_h = scene->Add(light);
	if(light_h < 0) goto err; 

	light.position = vec3f(-90, 15, -3);
	light.Ld = vec3f(1.2, 1.2, 3);
	light.radius = 1000.f;

	light_h = scene->Add(light);
	if(light_h < 0) goto err;
/*
	light.position = vec3f(40.5, 8, -10);
	light.Ld = vec3f(2,2,2);
	light.radius = 1000.f;

	light_h = scene->Add(light);
	if(light_h < 0) goto err;
*/
	light.position = vec3f(-5, 10, -10);
	light.Ld = vec3f(1.5, 0.8, 1.2);
	light.radius = 1000.f;

	light_h = scene->Add(light);
	if(light_h < 0) goto err;

	light.position = vec3f(20, 30, 0);
	light.Ld = vec3f(3, 3, 3);
	light.radius = 1000.f;

	light_h = scene->Add(light);
	if(light_h < 0) goto err;

	al.position = vec3f(60, 0, 0);
	al.width = vec2f(10.f, 2.f);
	al.rotation = vec3f(0, 0, 0);
	al.Ld = vec3f(1,0.8,0);

	alh = scene->Add(al);
	if(alh < 0) goto area_err;


	al.position = alPos;
	al.width = vec2f(8.f, 6.f);
	al.rotation = vec3f(0, 0, M_PI/4);
	al.Ld = vec3f(2.0,1.5,1);

	alh2 = scene->Add(al);
	if(alh2 < 0) goto area_err;

	al.position = alPos2;
	al.width = vec2f(60.f, 18.f);
	al.rotation = vec3f(0, M_PI, 0);
	al.Ld = vec3f(1.5,1,2);

	alh3 = scene->Add(al);
	if(alh3 < 0) goto area_err;

	return true;
	err:
	{
		LogErr("Couldn't add light to scene.");
		return false;
	}

	area_err: {
		LogErr("Couldn't add area light to scene.");
		return false;
	}
}

bool initFunc(Scene *scene) {
	if(!MakeLights(scene))
		return false;

	f32 hWidth = 200;
	f32 texRepetition = hWidth/5;
	f32 pos[] = {
		-hWidth, 0, -hWidth,
		-hWidth, 0, hWidth,
		hWidth, 0, hWidth,
		hWidth, 0, -hWidth
	};
	f32 colors[] = {
		1, 0, 0, 1,
		0, 1, 0, 1,
		0, 0, 1, 1,
		1, 1, 1, 1
	};
	f32 texcoords[] = {
		0, 0,
		0, texRepetition,
		texRepetition, texRepetition,
		texRepetition, 0
	};

	f32 normals[] = {
		0, 1, 0,
		0, 1, 0,
		0, 1, 0,
		0, 1, 0
	};

	u32 idx[] = {
		0, 1, 2, 0, 2, 3
	};

	Skybox::Desc skyd;
	skyd.filenames[0] = "../../data/skybox/sky1/right.png";
	skyd.filenames[1] = "../../data/skybox/sky1/left.png";
	skyd.filenames[2] = "../../data/skybox/sky1/down.png";
	skyd.filenames[3] = "../../data/skybox/sky1/up.png";
	skyd.filenames[4] = "../../data/skybox/sky1/back.png";
	skyd.filenames[5] = "../../data/skybox/sky1/front.png";
	Skybox::Handle sky = scene->Add(skyd);
	if(sky < 0) {
		LogErr("Error loading skybox.");
		return false;
	}
	scene->SetSkybox(sky);


	
	Render::Mesh::Desc mdesc("TestMesh", false, 6, idx, 4, pos, normals, texcoords, nullptr, nullptr, colors);
	Render::Mesh::Handle plane_mesh = Render::Mesh::Build(mdesc);
	if(plane_mesh < 0) {
		LogErr("Error creating test mesh");
		return false;
	}

	Render::Mesh::Handle sphere = Render::Mesh::BuildSphere();
	if (sphere < 0) {
		LogErr("Error creating sphere mesh");
		return false;
	}


	u32 nband = 6;
	sh1.Init(scene, nband);
	sh1.AddAreaLight(alh);
	sh1.AddAreaLight(alh2);
	sh1.AddAreaLight(alh3);



	Object::Desc odesc(Render::Shader::SHADER_3D_MESH);	
	{
		odesc.ClearSubmeshes();

		Material::Desc mat_desc(col3f(0.1,0.1,0.1), col3f(1,1,1), col3f(1,1,1), 0.65);
		mat_desc.diffuseTexPath = "../../data/concrete.png";
		mat_desc.normalTexPath = "../../data/concrete_nm.png";
		mat_desc.ltcMatrixPath = "../../data/ltc_mat.dds";
		mat_desc.ltcAmplitudePath = "../../data/ltc_amp.dds";
		Material::Handle mat = scene->Add(mat_desc);
		if(mat < 0) {
			LogErr("Error adding material");
			return false;
		}

		odesc.AddSubmesh(plane_mesh, mat);
		odesc.Identity();
		odesc.Translate(vec3f(0,-1.5f,0));

		Object::Handle obj = scene->Add(odesc);
		if(obj < 0) {
			LogErr("Error registering plane");
			return false;
		}
	}

	const vec3f arrayPos(20, 0, 0);

	const int sphere_n = 10;
	const int sphere_j = 8;
	for(int j = 0; j < sphere_j; ++j) {
		for(int i = 0; i < sphere_n; ++i) {
			f32 fi = pow((i+1) / (f32)sphere_n, 0.4f);
			f32 fj = j / (f32)sphere_j;

			odesc.ClearSubmeshes();

			odesc.Identity();
			odesc.Translate(arrayPos + vec3f(-2 + i * 3.f, 0.f, -8 + j * 3.f));
			odesc.Rotate(vec3f(0, M_PI_OVER_TWO * i, 0));

			Material::Desc mat_desc(col3f(0.0225 + fj * 0.16, 0.0735, 0.19125 - fj * 0.16),
									col3f(0.0828 + fj * 1.05, 0.17048, 1.9038 - fj * 1.05),
									col3f(0.08601+ fj * 0.16, 0.13762, 0.25678- fj * 0.16),
									0.001f + 0.984f * fi);
			mat_desc.normalTexPath = "../../data/wave_nm.png";
			//mat_desc.specularTexPath = "data/metal_spec.png";
			mat_desc.ltcMatrixPath = "../../data/ltc_mat.dds";
			mat_desc.ltcAmplitudePath = "../../data/ltc_amp.dds";

			Material::Handle mat = scene->Add(mat_desc);
			if(mat < 0) {
				LogErr("Error adding material");
				return false;
			}

			odesc.AddSubmesh(sphere, mat);

			Object::Handle sphere_object = scene->Add(odesc);
			if(sphere_object < 0) {
				LogErr("Error registering sphere ", i, ", ", j, ".");
				return false;
			}
		}
	}

	return true;
}

void updateFunc(Scene *scene, float dt) {
	const Device &device = GetDevice();
	vec2i mouseCoords = vec2i(device.GetMouseX(), device.GetMouseY());
	static f32 t = 0, oneSec = 0.f;
	

	if (device.IsMouseHit(MouseButton::MB_Left)) {
		vec4f pos = Render::FBO::ReadGBuffer(Render::FBO::GBufferAttachment::WORLDPOS, mouseCoords.x, mouseCoords.y);
		vec4f nrm = Render::FBO::ReadGBuffer(Render::FBO::GBufferAttachment::NORMAL, mouseCoords.x, mouseCoords.y);
		
		sh1.UpdateCoords(vec3f(pos.x, pos.y, pos.z), vec3f(nrm.x, nrm.y, nrm.z));
	}

	AreaLight::Desc *light = scene->GetLight(alh);
	if(light) {
		// light->rotation.y += dt * M_PI * 0.5f;
	}
	AreaLight::Desc *light2 = scene->GetLight(alh2);
	if(light2) {
		// light2->position.y = alPos.y + 2 * sinf(1.5*t);
		// light2->rotation.z += dt * M_PI * 0.5f;
	}
	AreaLight::Desc *light3 = scene->GetLight(alh3);
	if(light3) {
		// light3->position.x = alPos2.x + 10 * sinf(1*t);
	}

	if (oneSec > 1.f) {
		oneSec = 0.f;
	}

	t += dt;
	oneSec += dt;
}

void renderFunc(Scene *scene) {
}


int main() {
	Log::Init();

	Device &device = GetDevice();
	if (!device.Init(initFunc, updateFunc, renderFunc)) {
		printf("Error initializing Device. Aborting.\n");
		device.Destroy();
		system("PAUSE");
		return 1;
	}

	device.Run();

	device.Destroy();
	Log::Close();
    return 0;
}
