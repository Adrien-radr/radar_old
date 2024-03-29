#pragma once

#include "common/common.h"
#include "common/resource.h"

#include "scene.h"

#include "GL/glew.h"
#include "GLFW/glfw3.h"

struct Config
{
	vec2i   windowSize;
	bool    fullscreen;
	u32     MSAASamples;
	f32     fov;
	bool    vSync;
	u32		anisotropicFiltering;

	f32		cameraBaseSpeed;
	f32		cameraSpeedMult;
	f32		cameraRotationSpeed;

	vec3f 	cameraPosition;
	vec3f	cameraTarget;
};

typedef void ( *LoopFunction )( float dt );

/// The Device encompass :
///     - Context Creation
///     - Bridge with OpenGL
///     - Window Managment
///     - Event & Input managment
/// Only one device is created per game session.
class Device
{
public:
	Device() : window( nullptr ) {}
	bool Init( LoopFunction loopFunction );
	void Destroy();

	void Run();
	

	/// Listener registering function
	/// @param type : Key or Mouse listener
	/// @param func : Callback function of the listener (of type ListenerFunc)
	/// @param data : Void pointer on anything that could be useful in the callback
	bool AddEventListener( ListenerType type, ListenerFunc func, void *data );

	void UpdateProjection();

	void SetMouseX( int x ) const;
	void SetMouseY( int y ) const;
	void ShowCursor( bool flag ) const;

	// Input querying functions. EventManaget needs to be initialized
	u32  GetMouseX() const;
	u32  GetMouseY() const;

	bool IsKeyUp( Key k ) const;
	bool IsKeyHit( Key k ) const;
	bool IsKeyDown( Key k ) const;

	bool IsMouseUp( MouseButton m ) const;
	bool IsMouseHit( MouseButton m ) const;
	bool IsMouseDown( MouseButton m ) const;

	bool IsWheelUp() const;
	bool IsWheelDown() const;
	
	const mat4f &Get3DProjectionMatrix() const { return projection_matrix_3d; }
	const mat4f &Get2DProjectionMatrix() const { return projection_matrix_2d; }

	const Config &GetConfig() const { return config; }

private:
	bool ImGui_Init();
	bool ImGui_CreateDeviceObjects();
	void ImGui_Destroy();
	void ImGui_NewFrame();

public:
	vec2i       windowSize;
	vec2i       windowCenter;

	vec2i		mousePosition;
	vec2i		mouseLastPosition;

private:
	GLFWwindow  *window;
	Config		config;

	mat4f		projection_matrix_2d;
	mat4f		projection_matrix_3d;
	float 		fov;

	f64         engineTime;    //!< Time since the device has started

	LoopFunction mainLoop;
};

// Only instance of the device, creation, access, and destruction
Device &GetDevice();
void DestroyDevice();

// Defined in device_imgui.cpp
void ImGui_MouseListener( const Event &evt, void *data );
void ImGui_KeyListener( const Event &evt, void *data );