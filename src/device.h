#pragma once

#include "common/common.h"
#include "common/resource.h"

#include "scene.h"

#include "GL/glew.h"
#include "GLFW/glfw3.h"

struct Config {
    vec2i   windowSize;
    bool    fullscreen;
    u32     MSAASamples;
    f32     fov;
    bool    vSync;
	u32		anisotropicFiltering;

	f32		cameraBaseSpeed;
	f32		cameraSpeedMult;
};

/// The Device encompass :
///     - Context Creation
///     - Bridge with OpenGL
///     - Window Managment
///     - Event & Input managment
/// Only one device is created per game session.
class Device {
public:
	Device() : window(nullptr) {}
	bool Init(SceneInitFunc sceneInitFunc = nullptr, SceneUpdateFunc sceneUpdateFunc = nullptr, SceneRenderFunc sceneRenderFunc = nullptr);
	void Destroy();

	void Run();

	/// Listener registering function
	/// @param type : Key or Mouse listener
	/// @param func : Callback function of the listener (of type ListenerFunc)
	/// @param data : Void pointer on anything that could be useful in the callback
	bool AddEventListener(ListenerType type, ListenerFunc func, void *data);

	void UpdateProjection();

	void SetMouseX(int x) const;
	void SetMouseY(int y) const;
	void ShowCursor(bool flag) const;

	// Input querying functions. EventManaget needs to be initialized
	u32  GetMouseX() const;
	u32  GetMouseY() const;

	bool IsKeyUp(Key k) const;
	bool IsKeyHit(Key k) const;
	bool IsKeyDown(Key k) const;

	bool IsMouseUp(MouseButton m) const;
	bool IsMouseHit(MouseButton m) const;
	bool IsMouseDown(MouseButton m) const;

	bool IsWheelUp() const;
	bool IsWheelDown() const;

	bool IsWireframe() const { return wireframe; }

	const mat4f &Get3DProjectionMatrix() const { return projection_matrix_3d; }
	const mat4f &Get2DProjectionMatrix() const { return projection_matrix_2d; }

	const Config &GetConfig() const { return config; }
public:
	vec2i       windowSize;
	vec2i       windowCenter;

	vec2i		mousePosition;
	vec2i		mouseLastPosition;

private:
    GLFWwindow  *window;
	Config		config;

	bool		wireframe;		//!< True for wireframe mode activated
	mat4f		projection_matrix_2d;
	mat4f		projection_matrix_3d;
	float 		fov;

    f64         engineTime;    //!< Time since the device has started

	Scene		scene;			//!< Game scene
};

// Only instance of the device
Device &GetDevice();
