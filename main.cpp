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
		-30, 0, -30,
		30, 0, 30,
		30, 0, -30 };
	f32 colors[] = {
		1, 0, 0, 1,
		0, 1, 0, 1,
		0, 0, 1, 1,
		1, 0, 0, 1,
		0, 0, 1, 1,
		1, 1, 1, 1 };
	f32 texcoords[] = {
		0, 0,
		0, 1,
		1, 1,
		0, 0,
		1, 1,
		1, 0
	};

	Render::Mesh::Desc mdesc("TestMesh", 6, pos, NULL, texcoords, colors);

	test_mesh = Render::Mesh::Build(mdesc);
	if(test_mesh < 0) {
		LogErr("Error creating test mesh");
		return false;
	}

	Render::Texture::Desc tdesc;
	tdesc.name = "data/scout_body.png";
	scout_texture = Render::Texture::Build(tdesc);
	if(scout_texture < 0) {
		LogErr("Error creating texture");
		return false;
	}

	Object::Desc odesc(test_mesh, Render::Mesh::ANIM_NONE, Render::Shader::SHADER_3D_MESH, scout_texture);
	Object::Handle oh = scene->Add(odesc);
	if(oh < 0) {
		LogErr("Error registering object to scene");
		return false;
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
		LogErr("Error initializing Device. Aborting.");
		device.Destroy();
		return 1;
	}

	device.Run();

	device.Destroy();
	Log::Close();
    return 0;
}
