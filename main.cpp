#include "src/device.h"
#include "src/render.h"
#include "src/model.h"

#define SWIDTH 1024
#define SHEIGHT 768

Render::Mesh::Handle test_mesh;

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

	// Model bunny;
	// if(!bunny.LoadFromFile("data/bunny.ply")) {
	// 	LogErr("Error loading bunny");
	// 	return false;
	// }

	Render::Mesh::Desc mdesc("TestMesh", false, 6, idx, 4, pos, normals, texcoords, colors);

	test_mesh = Render::Mesh::Build(mdesc);
	if(test_mesh < 0) {
		LogErr("Error creating test mesh");
		return false;
	}

	Render::Mesh::Handle sphere = Render::Mesh::BuildSphere();
	if(sphere < 0) {
		LogErr("Error creating sphere mesh");
		return false;
	}

	Render::Texture::Desc tdesc;

	tdesc.name = "data/dummy.png";
	Render::Texture::Handle dummy_texture = Render::Texture::Build(tdesc);
	if(dummy_texture < 0) {
		LogErr("Error creating dummy texture");
		return false;
	}

	tdesc.name = "data/asphalt.png";
	Render::Texture::Handle asphalt_texture = Render::Texture::Build(tdesc);
	if(asphalt_texture < 0) {
		LogErr("Error creating asphalt_texture");
		return false;
	}


	Object::Desc odesc(test_mesh, Render::Mesh::ANIM_NONE, Render::Shader::SHADER_3D_MESH, dummy_texture, Material::DEFAULT_MATERIAL);

	{
		Material::Desc mat_desc(col3f(0.1,0.1,0.1), col3f(.5,.5,.5), col3f(1,1,1), 0.4);
		Material::Handle mat = scene->Add(mat_desc);
		if(mat < 0) {
			LogErr("Error adding material");
			return false;
		}

		odesc.texture = asphalt_texture;
		odesc.material = mat;
		odesc.model_matrix.Identity();
		odesc.Translate(vec3f(0,-1.5f,0));

		Object::Handle obj = scene->Add(odesc);
		if(obj < 0) {
			LogErr("Error registering plane");
			return false;
		}
	}

	odesc.mesh = sphere;
	odesc.texture = dummy_texture;

	const int sphere_n = 10;
	for(int j = 0; j < 1; ++j) {
		for(int i = 0; i < sphere_n; ++i) {
			odesc.model_matrix.Identity();
			odesc.Translate(vec3f(2 - i * 3.f, 0.f, 2 -j * 3.f));

			Material::Desc mat_desc(col3f(0.0225, 0.0735, 0.19125),
									col3f(0.0828, 0.17048, 1.9038),
									col3f(0.08601, 0.13762, 0.25678),
									0.001f + (0.974f/sphere_n) * (i+1));

			Material::Handle mat = scene->Add(mat_desc);
			if(mat < 0) {
				LogErr("Error adding material");
				return false;
			}

			odesc.material = mat;

			Object::Handle sphere_object = scene->Add(odesc);
			if(sphere_object < 0) {
				LogErr("Error registering sphere ", i, ", ", j, ".");
				return false;
			}
		}
	}

// Render::SpriteSheet::Handle ssh = Render::SpriteSheet::LoadFromFile("data/ships_spritesheet.json");

	return true;
}

void updateFunc(Scene *scene, float dt) {
	// const Device &device = GetDevice();
	// vec2i mouse_coord = vec2i(device.GetMouseX(), device.GetMouseY());

}

void renderFunc(Scene *scene) {
	// TODO : add modelmatrix
	// mat4f m;
	// m = mat4f::Translation(vec3f(device.window_center.x, device.window_center.y, 0.f));
	// Render::Shader::SendMat4(Render::Shader::UNIFORM_MODELMATRIX, m);

	// Render::Mesh::Render(test_mesh, Render::Mesh::AnimState());
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
