#include "camera.h"
#include "device.h"
#include "imgui.h"

#include <algorithm>

void Camera::Update(float dt) {
	static Device &device = GetDevice();
	ImGuiIO &io = ImGui::GetIO();

	const bool captureMouse = !io.WantCaptureMouse;
	vec3f posDiff(0);

	// Translation
	if (device.IsKeyHit(K_LShift))
		speedMode += 1;
	if (device.IsKeyUp(K_LShift))
		speedMode -= 1;
	if (device.IsKeyHit(K_LCtrl))
		speedMode -= 1;
	if (device.IsKeyUp(K_LCtrl))
		speedMode += 1;

	if (device.IsKeyDown(K_D))
		posDiff += right;
	if (device.IsKeyDown(K_A))
		posDiff -= right;
	if (device.IsKeyDown(K_W))
		posDiff += forward;
	if (device.IsKeyDown(K_S))
		posDiff -= forward;
	if (device.IsKeyDown(K_Space))
		posDiff += up;
	if (device.IsKeyDown(K_LAlt))
		posDiff -= up;

	float len = posDiff.Len();
	if (len > 0.f) {
		posDiff /= len;
		hasMoved = true;
		posDiff *= dt * translationSpeed * (speedMode ? (speedMode > 0 ? speedMult : 0.5f / speedMult) : 1.f);

		position += posDiff;
	}

	if (captureMouse && device.IsMouseHit(MB_Right)) {
		freeflyMode = true;
		lastMousePos = vec2i(device.GetMouseX(), device.GetMouseY());
		device.SetMouseX(device.windowCenter.x);
		device.SetMouseY(device.windowCenter.y);
		device.mousePosition = device.windowCenter;
		device.ShowCursor(false);
	}

	if (captureMouse && freeflyMode && device.IsMouseUp(MB_Right)) {
		freeflyMode = false;
		device.SetMouseX(lastMousePos.x);
		device.SetMouseY(lastMousePos.y);
		device.mousePosition = device.windowCenter;
		device.ShowCursor(true);
	}

	// Freefly mouse mode
	if (freeflyMode) {
		// Rotation
		vec2i mouseOffset = device.mousePosition - device.windowCenter;
		// LogInfo("", mouseOffset.x, " ", mouseOffset.y);
		{
			//reposition mouse at the center of the screen
			device.SetMouseX(device.windowCenter.x);
			device.SetMouseY(device.windowCenter.y);
			device.mousePosition = device.windowCenter;
		}

		if (mouseOffset.x != 0 || mouseOffset.y != 0) {
			phi += mouseOffset.x * dt * rotationSpeed;
			theta -= mouseOffset.y * dt * rotationSpeed;

			if (phi > M_TWO_PI)
				phi -= M_TWO_PI;
			if (phi < 0.f)
				phi += M_TWO_PI;

			theta = std::max(-M_PI_OVER_TWO + 1e-5f, std::min(M_PI_OVER_TWO - 1e-5f, theta));

			const f32 cosTheta = std::cos(theta);
			forward.x = cosTheta * std::cos(phi);
			forward.y = std::sin(theta);
			forward.z = cosTheta * std::sin(phi);

			right = forward.Cross(vec3f(0, 1, 0));
			right.Normalize();
			up = right.Cross(forward);
			up.Normalize();

			hasMoved = true;
		}
	}
}