#include "scene.h"
#include "device.h"

#include <algorithm>

#define SCENE_MAX_LIGHTS 64
#define SCENE_MAX_ACTIVE_LIGHTS 8
#define SCENE_MAX_OBJECTS 2048
#define SCENE_MAX_TEXTS 64

Material::Handle Material::DEFAULT_MATERIAL = -1;

void Camera::Update(float dt) {
	static Device &device = GetDevice();

	vec3f posDiff;

	// Translation
	if(device.IsKeyHit(K_LShift))
		speedMode = true;
	if(device.IsKeyUp(K_LShift))
		speedMode = false;

	if(device.IsKeyDown(K_D))
		posDiff += right;
	if(device.IsKeyDown(K_A))
		posDiff -= right;
	if(device.IsKeyDown(K_W))
		posDiff += forward;
	if(device.IsKeyDown(K_S))
		posDiff -= forward;

	float len = posDiff.Len();
	if(len > 0.f) {
		posDiff /= len;
		hasMoved = true;
		posDiff *= dt * translationSpeed * (speedMode ? speedMult : 1.f);

		position += posDiff;
	}

	if(device.IsMouseHit(MB_Right)) {
		freeflyMode = true;
		device.SetMouseX(device.windowCenter.x);
		device.SetMouseY(device.windowCenter.y);
		device.mousePosition = device.windowCenter;
		device.ShowCursor(false);
	}
	
	if(device.IsMouseUp(MB_Right)) {
		freeflyMode = false;
		device.SetMouseX(device.windowCenter.x);
		device.SetMouseY(device.windowCenter.y);
		device.mousePosition = device.windowCenter;
		device.ShowCursor(true);
	}
	
	// Freefly mouse mode
	if(freeflyMode) {
		// Rotation
		vec2i mouseOffset = device.mousePosition - device.windowCenter;
		// LogInfo("", mouseOffset.x, " ", mouseOffset.y);
		{
			//reposition mouse at the center of the screen
			device.SetMouseX(device.windowCenter.x);
			device.SetMouseY(device.windowCenter.y);
			device.mousePosition = device.windowCenter;
		}

		if(mouseOffset.x != 0 || mouseOffset.y != 0) {
			phi += mouseOffset.x * dt * rotationSpeed;
			theta -= mouseOffset.y * dt * rotationSpeed;

			if(phi > M_TWO_PI)
				phi -= M_TWO_PI;
			if(phi < 0.f)
				phi += M_TWO_PI;

			theta = std::max(-M_PI_OVER_TWO, std::min(M_PI_OVER_TWO, theta));

			const f32 cosTheta = std::cos(theta);
			forward.x = cosTheta * std::cos(phi);
			forward.y = std::sin(theta);
			forward.z = cosTheta * std::sin(phi);

			right = forward.Cross(vec3f(0,1,0));
			right.Normalize();
			up = right.Cross(forward);
			up.Normalize();

			hasMoved = true;
		}
	}
}

namespace Object {
	void Desc::Translate(const vec3f &t) {
		model_matrix *= mat4f::Translation(t);
	}

	void Desc::Scale(const vec3f &s) {
		model_matrix *= mat4f::Scale(s);
	}
		
	// void Desc::SetPosition(const vec2f &pos) {
	// 	model_matrix = mat4f::Translation(vec3f(pos.x, pos.y, 0.f));
	// }

	// void Desc::SetRotation(float angle) {
	// 	vec3f curr_pos(model_matrix[3].x, model_matrix[3].y, model_matrix[3].z);
	// 	model_matrix.Identity();
	// 	model_matrix.RotateZ(angle);
	// 	model_matrix *= mat4f::Translation(curr_pos);
	// }
}

namespace Text {
	void Desc::SetPosition(const vec2f &pos) {
		model_matrix = mat4f::Translation(vec3f(pos.x, pos.y, 0.f));
	}
}

void SceneResizeEventListener(const Event &event, void *data) {
	Scene *scene = static_cast<Scene*>(data);
	// scene->UpdateProjection(event.v);
}

Scene::Scene() : customInitFunc(nullptr), customUpdateFunc(nullptr) {
	Clean();
}

bool Scene::Init(SceneInitFunc initFunc, SceneUpdateFunc updateFunc, SceneRenderFunc renderFunc) {
	const Device &device = GetDevice();
	const Config &config = device.GetConfig();

	customInitFunc = initFunc;
	customUpdateFunc = updateFunc;
	customRenderFunc = renderFunc;

	// Create camera and update shader to use it
	//camera.zoom_level = 1.f;
	//camera.zoom_speed = 100.f;
	//camera.pan_speed = 250.f;
	//camera.pan_fast_speed = 1000.f;
	//camera.pan_fast = false;
	//camera.center = device.window_center;
	//camera.position = vec2f(0, 0);

	camera.hasMoved = false;
	camera.speedMode = false;
	camera.freeflyMode = false;
    camera.dist = 7.5f;
	camera.speedMult = config.cameraSpeedMult;
    camera.translationSpeed = config.cameraBaseSpeed;
    camera.rotationSpeed = 0.05f;
    camera.position = vec3f(-27.6, 2.52, -2.44);
    camera.target = vec3f(-23.5, 1.f, 0);
    camera.up = vec3f(0,1,0);
    camera.forward = camera.target - camera.position;
    camera.forward.Normalize();

    camera.right = camera.forward.Cross(camera.up);
    camera.right.Normalize();
    camera.up = camera.right.Cross(camera.forward);
    camera.up.Normalize();

    vec2f azimuth(camera.forward[0], camera.forward[2]);
    azimuth.Normalize();
    camera.phi = std::atan2(azimuth[1], azimuth[0]);
    camera.theta = std::atan2(camera.forward[1], std::sqrt(azimuth.Dot(azimuth)));

	// initialize shader matrices
	UpdateView();

	texts.reserve(256);
	lights.reserve(32);
	active_lights.reserve(8);
	objects.reserve(1024);
	materials.reserve(64);

	// Fps text
	Render::Font::Desc fdesc("data/DejaVuSans.ttf", 12);
	Render::Font::Handle fhandle = Render::Font::Build(fdesc);
	if (fhandle < 0) {
		LogErr("Error loading DejaVuSans font.");
		return false;
	}

	Text::Desc tdesc("FPS : ", fhandle, vec4f(0.3f, 0.3f, 0.3f, 1.f));
	tdesc.SetPosition(vec2f(10, 10));
	fps_text = Add(tdesc);
	if (fps_text < 0) {
		LogErr("Error adding scene's FPS text.");
		return false;
	}

	tdesc.str = "Camera : ";
	tdesc.SetPosition(vec2f(10, 22));
	camera_text = Add(tdesc);
	if (camera_text < 0) {
		LogErr("Error adding scene's camera text.");
		return false;
	}

	Material::Desc mat_desc;
	Material::DEFAULT_MATERIAL = Add(mat_desc);
	if(Material::DEFAULT_MATERIAL < 0) {
		LogErr("Error adding Default Material");
		return false;
	}

	bool customInitResult = true;
	if(customInitFunc)
		customInitResult = customInitFunc(this);

	return customInitResult;
}

void Scene::Clean() {
	lights.clear();
	objects.clear();
	texts.clear();
	materials.clear();
}

void Scene::UpdateView() {
	camera.target = camera.position + camera.forward;
	view_matrix = mat4f::LookAt(camera.position, camera.target, camera.up);

	for(int i = Render::Shader::_SHADER_3D_PROJECTION_START; i < Render::Shader::_SHADER_3D_PROJECTION_END; ++i) {
		Render::Shader::Bind(i);
		Render::Shader::SendMat4(Render::Shader::UNIFORM_VIEWMATRIX, view_matrix);
		Render::Shader::SendVec3(Render::Shader::UNIFORM_EYEPOS, camera.position);
	}
}

void Scene::Update(float dt) {
	const Device &device = GetDevice();

	// Real-time camera updating
	camera.Update(dt);
	if(camera.hasMoved) {
		camera.hasMoved = false;
		UpdateView();
	}

	static f32 ai_timer = 0.f, one_sec = 0.f;
	ai_timer += dt; one_sec += dt;

	// Updates 100 times per second
	if (ai_timer >= 0.01f) {
		ai_timer = 0.f;

		// update animation for all objects
		for (u32 j = 0; j < objects.size(); ++j) {
			Object::Desc &object = objects[j];
			//if (object.animation > Render::Mesh::ANIM_NONE) {
				//Render::Mesh::UpdateAnimation(object.mesh, object.animation_state, 0.01f);
			//}
		}

		std::stringstream cam_str;
		cam_str << "Camera : <" << camera.position.x << ", " << camera.position.y << ", "
			<< camera.position.z << ">";
		SetTextString(camera_text, cam_str.str());
	}

	// Update every second
	if (one_sec >= 1.f) {
		one_sec = 0.f;


		std::stringstream fps_str;
		fps_str << "FPS : " << (int)(1.f / dt);
		SetTextString(fps_text, fps_str.str());
	}

	if(customUpdateFunc)
		customUpdateFunc(this, dt);
}

void Scene::Render() {
	const Device &device = GetDevice();

	if (device.IsWireframe()) {
		glDisable(GL_CULL_FACE);
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	}

	// Render::StartPolygonRendering();

	for (u32 j = 0; j < objects.size(); ++j) {
		Object::Desc &object = objects[j];
		Render::Shader::Bind(object.shader);

		for (u32 l = 0; l < active_lights.size(); ++l) {
			if (active_lights[l] == -1) break;

			std::stringstream uniform_name;
			uniform_name << "lights[" << l << "].position";
			Light::Desc &light = lights[active_lights[l]];
			Render::Shader::SendVec3(uniform_name.str(), light.position);
		}

		// send material parameters
		Render::UBO::Bind(Render::Shader::UNIFORMBLOCK_MATERIAL, materials[object.material].ubo);

		Render::Texture::Bind(object.texture, Render::Texture::TexTarget0);
		Render::Shader::SendMat4(Render::Shader::UNIFORM_MODELMATRIX, object.model_matrix);
		Render::Mesh::Render(object.mesh, object.animation_state);
	}

	if(customRenderFunc)
		customRenderFunc(this);

	// non-wireframe mode for text rendering
	glEnable(GL_CULL_FACE);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	Render::StartTextRendering();
	for (u32 i = 0; i < texts.size(); ++i) {
		Text::Desc &text = texts[i];
		Render::Shader::SendMat4(Render::Shader::UNIFORM_MODELMATRIX, text.model_matrix);
		Render::Shader::SendVec4(Render::Shader::UNIFORM_TEXTCOLOR, text.color);
		Render::Font::Bind(text.font, Render::Texture::TexTarget0);
		Render::TextMesh::Render(text.mesh);
	}
}


Object::Handle Scene::Add(const Object::Desc &d) {
	if (!Render::Mesh::Exists(d.mesh)) {
		LogErr("Given mesh is not registered in renderer.");
		return -1;
	}
	if (!Render::Shader::Exists(d.shader)) {
		LogErr("Given shader is not registered in renderer.");
		return -1;
	}
	if (!Render::Texture::Exists(d.texture)) {
		LogErr("Given texture is not registered in renderer.");
		return -1;
	}
	if(!MaterialExists(d.material)) {
		LogErr("Given material is not registered in the scene.");
		return -1;
	}
	if (d.animation >= Render::Mesh::ANIM_N) {
		LogErr("Given animation is not valid.");
		return -1;
	}

	size_t index = objects.size();

	if (index >= SCENE_MAX_OBJECTS) {
		LogErr("Reached maximum number (", SCENE_MAX_OBJECTS, ") of objects in scene.");
		return -1;
	}

	objects.push_back(d);

	Object::Desc &object = objects[index];
	Render::Mesh::SetAnimation(object.mesh, object.animation_state, object.animation);
	object.model_matrix = d.model_matrix;

	return (Object::Handle)index;
}

Light::Handle Scene::Add(const Light::Desc &d) {
	size_t index = lights.size();

	if (index >= SCENE_MAX_LIGHTS) {
		LogErr("Reached maximum number (", SCENE_MAX_LIGHTS, ") of lights in scene.");
		return -1;
	}

	lights.push_back(d);

	Light::Desc &light = lights[index];
	light.active = true;

	return (Light::Handle)index;
}

Text::Handle Scene::Add(const Text::Desc &d) {
	if (!Render::Font::Exists(d.font)) {
		LogErr("Given font is not registered in renderer.");
		return -1;
	}

	size_t index = texts.size();

	if (index >= SCENE_MAX_TEXTS) {
		LogErr("Reached maximum number (", SCENE_MAX_TEXTS, ") of texts in scene.");
		return -1;
	}


	texts.push_back(d);

	Text::Desc &text = texts[index];
	text.mesh = -1;

	SetTextString((Text::Handle)index, text.str);

	return (Text::Handle)index;
}


Material::Handle Scene::Add(const Material::Desc &d) {
	size_t index = materials.size();

	Material::Data mat;
	mat.desc = d;

	// Create UBO to accomodate it on GPU
	Render::UBO::Desc ubo_desc((f32*)&d, sizeof(Material::Desc));
	mat.ubo = Render::UBO::Build(ubo_desc);
	if(mat.ubo < 0) {
		LogErr("Error creating Material");
		return -1;
	}

	materials.push_back(mat);
	return (Material::Handle)index;
}

bool Scene::MaterialExists(Material::Handle h) const {
	return h >= 0 && h < (int)materials.size() && Render::UBO::Exists(materials[h].ubo);
}

void Scene::SetTextString(Text::Handle h, const std::string &str) {
	if (h >= 0 && h < (int)texts.size()) {
		Text::Desc &text = texts[h];
		text.str = str;
		text.mesh = Render::TextMesh::SetString(text.mesh, text.font, text.str);
	}
}
