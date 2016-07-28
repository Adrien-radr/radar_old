#include "src/device.h"
#include "src/render.h"

#define SWIDTH 1024
#define SHEIGHT 768

Render::Mesh::Handle test_mesh;
Object::Handle crysisGuy = -1;
AreaLight::Handle alh = -1;
AreaLight::Handle alh2;
AreaLight::Handle alh3;
vec3f alPos(40., 3, -20);
vec3f alPos2(20., 7.5, 20);

Render::Texture::Handle th1;
Render::Texture::Handle th2;

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
	/*
	Render::Texture::Desc tdesc("data/ltc_mat.dds");
	th1 = Render::Texture::Build(tdesc);
	if(!th1) {
		return false;
	}
	Render::Texture::Desc tdesc2("data/ltc_amp.dds");
	th2 = Render::Texture::Build(tdesc2);
	if(!th2) {
		return false;
	}*/
	
	// Render::Texture::Bind(th1, Render::Texture::TARGET3);
	// Render::Texture::Bind(th2, Render::Texture::TARGET4);


	Skybox::Desc skyd;
	skyd.filenames[0] = "data/skybox/sky1/right.png";
	skyd.filenames[1] = "data/skybox/sky1/left.png";
	skyd.filenames[2] = "data/skybox/sky1/down.png";
	skyd.filenames[3] = "data/skybox/sky1/up.png";
	skyd.filenames[4] = "data/skybox/sky1/back.png";
	skyd.filenames[5] = "data/skybox/sky1/front.png";
	Skybox::Handle sky = scene->Add(skyd);
	if(sky < 0) {
		LogErr("Error loading skybox.");
		return false;
	}
	scene->SetSkybox(sky);


	
	Render::Mesh::Desc mdesc("TestMesh", false, 6, idx, 4, pos, normals, texcoords, nullptr, nullptr, colors);

	test_mesh = Render::Mesh::Build(mdesc);
	if(test_mesh < 0) {
		LogErr("Error creating test mesh");
		return false;
	}

	ModelResource::Handle crysisModel = scene->LoadModelResource("data/nanosuit/nanosuit.obj");
	if(crysisModel < 0) {
		LogErr("Error loading nanosuit");
		return false;
	}

	ModelResource::Handle metal0Model = scene->LoadModelResource("data/colt/colt.obj");
	if(metal0Model < 0) {
		LogErr("Error loading metal0");
		return false;
	}

	ModelResource::Handle m14Model = scene->LoadModelResource("data/ak/AK_BS_Exp.obj");
	if(metal0Model < 0) {
		LogErr("Error loading m14");
		return false;
	}

#if 0
	ModelResource::Handle ironmanModel = scene->LoadModelResource("data/ironman/ironman.obj");
	if(ironmanModel < 0) {
		LogErr("Error loading ironman");
		return false;
	}
	ModelResource::Handle sponzaModel = scene->LoadModelResource("data/sponza/sponza.obj");
	if(sponzaModel < 0) {
		LogErr("Error loading sponza.");
		return false;
	}

	Object::Handle sponza = scene->InstanciateModel(sponzaModel);
	if(sponza < 0) {
		LogErr("Error creating sponza scene");
		return false;
	}
	Object::Desc *sponzaObj = scene->GetObject(sponza);
	sponzaObj->Scale(0.08f);
	sponzaObj->Translate(vec3f(0,-0.9,0));
	
#endif
	crysisGuy = scene->InstanciateModel(crysisModel);
	if(crysisGuy < 0) {
		LogErr("Error creating crysis Guy.");
		return false;
	}
	Object::Desc *crysisGuyObj = scene->GetObject(crysisGuy);
	crysisGuyObj->Translate(vec3f(0,-0.7,1));


	// Object::Handle ironman = scene->InstanciateModel(ironmanModel);
	// if(ironman < 0) {
		// LogErr("Error creating ironman Guy.");
		// return false;
	// }
	// Object::Desc *ironmanObj = scene->GetObject(ironman);
	// ironmanObj->Translate(vec3f(0,-0.7,1));



	Object::Handle metal_h = scene->InstanciateModel(metal0Model);
	if(metal_h < 0) {
		LogErr("Error creating metal0.");
		return false;
	}
	Object::Desc *coltObj = scene->GetObject(metal_h);
	coltObj->Translate(vec3f(40.5, 3, -16));


	Object::Handle m14_h = scene->InstanciateModel(m14Model);
	if(m14_h < 0) {
		LogErr("Error creating m14.");
		return false;
	}
	Object::Desc *m14_o = scene->GetObject(m14_h);
	m14_o->Translate(vec3f(40.5, 1.0, -16));
	// m14_o->Scale(vec3f(2));
	

	Render::Mesh::Handle sphere = Render::Mesh::BuildSphere();
	if(sphere < 0) {
		LogErr("Error creating sphere mesh");
		return false;
	}

	Object::Desc odesc(Render::Shader::SHADER_3D_MESH);	
	{
		odesc.ClearSubmeshes();

		Material::Desc mat_desc(col3f(0.1,0.1,0.1), col3f(1,1,1), col3f(1,1,1), 1);//, "data/sponza/textures/sponza_floor_a_diff.png");
		mat_desc.normalTexPath = "data/sponza/textures/sponza_floor_a_nrm.png";
		mat_desc.specularTexPath = "data/sponza/textures/sponza_floor_a_spec.png";
		mat_desc.ltcMatrixPath = "data/ltc_mat.dds";
		mat_desc.ltcAmplitudePath = "data/ltc_amp.dds";
		Material::Handle mat = scene->Add(mat_desc);
		if(mat < 0) {
			LogErr("Error adding material");
			return false;
		}

		odesc.AddSubmesh(test_mesh, mat);
		odesc.Identity();
		odesc.Translate(vec3f(0,-1.5f,0));

		Object::Handle obj = scene->Add(odesc);
		if(obj < 0) {
			LogErr("Error registering plane");
			return false;
		}
	}

	const int sphere_n = 10;
	const int sphere_j = 8;
	for(int j = 0; j < sphere_j; ++j) {
		for(int i = 0; i < sphere_n; ++i) {
			f32 fi = pow((i+1) / (f32)sphere_n, 0.4);
			f32 fj = j / (f32)sphere_j;

			odesc.ClearSubmeshes();

			odesc.Identity();
			odesc.Translate(vec3f(-2 + i * 3.f, 0.f, -8 + j * 3.f));
			odesc.Rotate(vec3f(0, M_PI_OVER_TWO * i, 0));

			Material::Desc mat_desc(col3f(0.0225 + fj * 0.16, 0.0735, 0.19125 - fj * 0.16),
									col3f(0.0828 + fj * 1.05, 0.17048, 1.9038 - fj * 1.05),
									col3f(0.08601+ fj * 0.16, 0.13762, 0.25678- fj * 0.16),
									0.001f + 0.984f * fi);
			mat_desc.normalTexPath = "data/wave_nm.png";
			mat_desc.specularTexPath = "data/metal_spec.png";
			mat_desc.ltcMatrixPath = "data/ltc_mat.dds";
			mat_desc.ltcAmplitudePath = "data/ltc_amp.dds";

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

	Render::Mesh::Handle box = Render::Mesh::BuildBox();
	if(box < 0) {
		LogErr("Error creating box mesh");
		return false;
	}
	odesc.Identity();
	odesc.ClearSubmeshes();
	odesc.AddSubmesh(box, Material::DEFAULT_MATERIAL);
	Object::Handle boxo = scene->Add(odesc);
	if(boxo < 0) {
		LogErr("Error creating box.");
		return false;
	}

	return true;
}

void updateFunc(Scene *scene, float dt) {
	// const Device &device = GetDevice();
	// vec2i mouse_coord = vec2i(device.GetMouseX(), device.GetMouseY());
	static f32 t = 0;

#if 0
	Object::Desc *model = scene->GetObject(crysisGuy);
	model->Translate(vec3f(1 * dt,0,0));
	model->Rotate(vec3f(0,M_PI * 0.5 * dt,0));
#endif

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

	t += dt;
}

void renderFunc(Scene *scene) {
}


int main() {
	Log::Init();

	Device &device = GetDevice();
	if (!device.Init(initFunc, updateFunc, renderFunc)) {
		printf("Error initializing Device. Aborting.\n");
		device.Destroy();
		return 1;
	}

	device.Run();

	device.Destroy();
	Log::Close();
    return 0;
}
