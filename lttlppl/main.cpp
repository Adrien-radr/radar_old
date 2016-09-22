#include "src/device.h"

bool Init(Scene *scene) {
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