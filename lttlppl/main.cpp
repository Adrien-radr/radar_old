#include "src/device.h"

bool Init(Scene *scene) {
	Skybox::Desc skyd;
	skyd.filenames[0] = "../../data/skybox/sky1/right.png";
	skyd.filenames[1] = "../../data/skybox/sky1/left.png";
	skyd.filenames[2] = "../../data/skybox/sky1/down.png";
	skyd.filenames[3] = "../../data/skybox/sky1/up.png";
	skyd.filenames[4] = "../../data/skybox/sky1/back.png";
	skyd.filenames[5] = "../../data/skybox/sky1/front.png";
	Skybox::Handle sky = scene->Add(skyd);
	if (sky < 0) {
		LogErr("Error loading skybox.");
		return false;
	}
	scene->SetSkybox(sky);

	return true;
}

void FixedUpdate(Scene *scene, float dt) {

}

void Update(Scene *scene, float dt) {

}

int main() {
	Log::Init();

	Device &device = GetDevice();
	if (!device.Init(Init)) {
		printf("Error initializing Device. Aborting.\n");
		DestroyDevice();
		Log::Close();
		system("PAUSE");
		return 1;
	}

	device.SetUpdateFunc(Update);
	device.SetFixedUpdateFunc(FixedUpdate);

	device.Run();

	DestroyDevice();
	Log::Close();

	return 0;
}