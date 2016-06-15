#include "src/device.h"
#include "src/render.h"

#define SWIDTH 1024
#define SHEIGHT 768

Render::Mesh::Handle test_mesh;
Render::Texture::Handle scout_texture;

bool initFunc(Scene *scene) {
	f32 pos[] = {
		-30, 0, -30,
		-30, 0, 30,
		30, 0, 30,
		30, 0, -30
	};
	f32 colors[] = {
		1, 0, 0, 1,
		0, 1, 0, 1,
		0, 0, 1, 1,
		1, 1, 1, 1
	};
	f32 texcoords[] = {
		0, 0,
		0, 1,
		1, 1,
		1, 0
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

	tdesc.name = "data/scout_body.png";
	scout_texture = Render::Texture::Build(tdesc);
	if(scout_texture < 0) {
		LogErr("Error creating texture");
		return false;
	}

	Material::Desc mat_desc(col3f(1,0,0), col3f(0,1,1), 0.4f);

	Material::Handle mat = scene->Add(mat_desc);
	if(mat < 0) {
		LogErr("Error adding material");
		return false;
	}

	Object::Desc odesc(test_mesh, Render::Mesh::ANIM_NONE, Render::Shader::SHADER_3D_MESH, dummy_texture, mat);
	odesc.mesh = sphere;
	odesc.texture = dummy_texture;

	for(int j = 0; j < 4; ++j) {
		for(int i = 0; i < 4; ++i) {
			odesc.model_matrix.Identity();
			odesc.Translate(vec3f(2 - i * 3.f, 0.f, 2 -j * 3.f));

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
