#include "src/device.h"
#include "src/render.h"

#define SWIDTH 1024
#define SHEIGHT 768

Render::Mesh::Handle test_mesh;
Object::Handle crysisGuy = -1;


bool MakeLights(Scene *scene) {
	// Lights
	Light::Desc light;
	light.position = vec3f(-9.5, 5, -10);
	light.Ld = vec3f(1.5, 1, 0);
	light.radius = 1000.f;

	Light::Handle light_h = scene->Add(light);
	if(light_h < 0) goto err; 

	light.position = vec3f(-90, 15, 3);
	light.Ld = vec3f(1.2, 1.2, 3);
	light.radius = 1000.f;

	light_h = scene->Add(light);
	if(light_h < 0) goto err;


	return true;
	err:
	{
		LogErr("Couldn't add light to scene.");
		return false;
	}
}

bool initFunc(Scene *scene) {
	f32 hWidth = 100;
	f32 texRepetition = hWidth/10;
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

	ModelResource::Handle sponzaModel = scene->LoadModelResource("data/sponza/sponza.obj");
	if(sponzaModel < 0) {
		LogErr("Error loading sponza.");
		return false;
	}

	Object::Handle sponza = scene->AddFromModel(sponzaModel);
	if(sponza < 0) {
		LogErr("Error creating sponza scene");
		return false;
	}
	Object::Desc *sponzaObj = scene->GetObject(sponza);
	sponzaObj->Scale(0.08f);
	sponzaObj->Translate(vec3f(0,-0.9,0));

	crysisGuy = scene->AddFromModel(crysisModel);
	if(crysisGuy < 0) {
		LogErr("Error creating crysis Guy.");
		return false;
	}
	Object::Desc *crysisGuyObj = scene->GetObject(crysisGuy);
	crysisGuyObj->Translate(vec3f(0,-0.7,-1));
	
	if(!MakeLights(scene))
		return false;

	Render::Mesh::Handle sphere = Render::Mesh::BuildSphere();
	if(sphere < 0) {
		LogErr("Error creating sphere mesh");
		return false;
	}

	Object::Desc odesc(Render::Shader::SHADER_3D_MESH);	
	{
		odesc.ClearSubmeshes();

		Material::Desc mat_desc(col3f(0.1,0.1,0.1), col3f(.5,.5,.5), col3f(1,1,1), 0.4, "data/concrete.png");
		mat_desc.normalTexPath = "data/concrete_nm.png";
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
	for(int j = 0; j < 1; ++j) {
		for(int i = 0; i < sphere_n; ++i) {
			odesc.ClearSubmeshes();

			odesc.Identity();
			odesc.Translate(vec3f(2 - i * 3.f, 0.f, 2 -j * 3.f));
			odesc.Rotate(vec3f(0, M_PI_OVER_TWO * i, 0));

			Material::Desc mat_desc(col3f(0.0225, 0.0735, 0.19125),
									col3f(0.0828, 0.17048, 1.9038),
									col3f(0.08601, 0.13762, 0.25678),
									0.001f + (0.974f/sphere_n) * (i+1));
			mat_desc.normalTexPath = "data/wave_nm.png";

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
	// const Device &device = GetDevice();
	// vec2i mouse_coord = vec2i(device.GetMouseX(), device.GetMouseY());

	Object::Desc *model = scene->GetObject(crysisGuy);
	model->Translate(vec3f(-1 * dt,0,0));
	model->Rotate(vec3f(0,M_PI * 0.5 * dt,0));
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
