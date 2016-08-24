#include "scene.h"
#include "device.h"
#include "imgui.h"

#include <algorithm>

#define SCENE_MAX_LIGHTS 64
#define SCENE_MAX_OBJECTS 2048
#define SCENE_MAX_TEXTS 64

Material::Handle Material::DEFAULT_MATERIAL = -1;

void Camera::Update(float dt) {
	static Device &device = GetDevice();

	vec3f posDiff;

	// Translation
	if (device.IsKeyHit(K_LShift))
		speedMode += 1;
	if (device.IsKeyUp(K_LShift))
		speedMode -= 1;
	if (device.IsKeyHit(K_LCtrl))
		speedMode -= 1;
	if (device.IsKeyUp(K_LCtrl))
		speedMode += 1;

	if(device.IsKeyDown(K_D))
		posDiff += right;
	if(device.IsKeyDown(K_A))
		posDiff -= right;
	if(device.IsKeyDown(K_W))
		posDiff += forward;
	if(device.IsKeyDown(K_S))
		posDiff -= forward;
	if (device.IsKeyDown(K_Space))
		posDiff += up;
	if (device.IsKeyDown(K_LAlt))
		posDiff -= up;

	float len = posDiff.Len();
	if(len > 0.f) {
		posDiff /= len;
		hasMoved = true;
		posDiff *= dt * translationSpeed * (speedMode ? (speedMode > 0 ? speedMult : 0.5f / speedMult) : 1.f);

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

			theta = std::max(-M_PI_OVER_TWO+1e-5f, std::min(M_PI_OVER_TWO-1e-5f, theta));

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
	void Desc::Identity() {
		model_matrix.Identity();
		position = vec3f();
		rotation = vec3f();
		scale = 1.f;
	}

	void Desc::Translate(const vec3f &t) {
		position += t;
	}

	void Desc::Scale(const vec3f &s) {
		scale *= s;
	}

	void Desc::Rotate(const vec3f &r) {
		rotation += r;
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

	camera.hasMoved = false;
	camera.speedMode = false;
	camera.freeflyMode = false;
    camera.dist = 7.5f;
	camera.speedMult = config.cameraSpeedMult;
    camera.translationSpeed = config.cameraBaseSpeed;
    camera.rotationSpeed = 0.01f * config.cameraRotationSpeed;
    camera.position = config.cameraPosition;
    camera.target = config.cameraTarget;
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

	pickedObject = -1;

	// initialize shader matrices
	UpdateView();

	skyboxes.reserve(16);
	texts.reserve(256);
	pointLights.reserve(32);
	areaLights.reserve(32);
	objects.reserve(1024);
	materials.reserve(64);
	for(u32 i = 0; i < SCENE_MAX_ACTIVE_LIGHTS; ++i) {
		active_pointLights[i] = -1;
		active_areaLights[i] = -1;
	}

	Render::Font::Desc fdesc("../../data/DejaVuSans.ttf", 12);
	Render::Font::Handle fhandle = Render::Font::Build(fdesc);
	if (fhandle < 0) {
		LogErr("Error loading DejaVuSans font.");
		return false;
	}

	Material::Desc mat_desc;
	Material::DEFAULT_MATERIAL = Add(mat_desc);
	if(Material::DEFAULT_MATERIAL < 0) {
		LogErr("Error adding Default Material");
		return false;
	}

	// Default Skybox (white)
	Skybox::Desc sd;
	sd.filenames[0] = "../../data/default_diff.png";
	sd.filenames[1] = "../../data/default_diff.png";
	sd.filenames[2] = "../../data/default_diff.png";
	sd.filenames[3] = "../../data/default_diff.png";
	sd.filenames[4] = "../../data/default_diff.png";
	sd.filenames[5] = "../../data/default_diff.png";

	Skybox::Handle sh = Add(sd);
	if(sh < 0) {
		LogErr("Error creating default white skybox.");
		return false;
	}
	SetSkybox(sh);
	skyboxMesh = Render::Mesh::BuildBox();
	if(skyboxMesh < 0) {
		LogErr("Error creating skybox mesh.");
		return false;
	}

	// Light UBO init 
	if(!InitLightUniforms()) {
		return false;
	}

	bool customInitResult = true;
	if(customInitFunc)
		customInitResult = customInitFunc(this);

	return customInitResult;
}

void Scene::Clean() {
	pointLights.clear();
	areaLights.clear();
	objects.clear();
	texts.clear();
	materials.clear();
	skyboxes.clear();
	models.clear();
}

void Scene::UpdateView() {
	camera.target = camera.position + camera.forward;
	view_matrix = mat4f::LookAt(camera.position, camera.target, camera.up);

	Render::UpdateView(view_matrix, camera.position);
}

bool Scene::ShowGBufferWindow() {
	bool show = true;
	
	ImGui::SetNextWindowPos(ImVec2(20,40), ImGuiSetCond_Once);
	ImGui::SetNextWindowSize(ImVec2(355, 655));
	ImGui::Begin("GBuffer Window", &show, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysVerticalScrollbar);
	for(int i = 0; i < Render::FBO::_ATTACHMENT_N; ++i) {
		Render::FBO::GBufferAttachment att = Render::FBO::GBufferAttachment(i);
		std::stringstream ss;
		ss << "[" << i << "] " << Render::FBO::GetGBufferAttachmentName(att);
		if(ImGui::CollapsingHeader(ss.str().c_str())) {
			u32 tex = Render::FBO::GetGBufferAttachment(att);
			if(tex > 0) {
				ImTextureID tid = reinterpret_cast<ImTextureID>((u64)tex);
				ImGui::Image(tid, ImVec2(320, 180), ImVec2(0,1), ImVec2(1,0), ImVec4(1,1,1,1), ImVec4(1,1,1,0.7f));
			}
		}
	}
	ImGui::End();

	return show;
}

void Scene::UpdateGUI() {
	ImGuiIO &io = ImGui::GetIO();
	static bool showGBufferWindow = false;
	static bool showTestWindow = false;

	// Main Menu
	if(ImGui::BeginMainMenuBar()) {
		if(ImGui::BeginMenu("File")) {
			if(ImGui::MenuItem("Show ImGui Test Window")) {
				showTestWindow = true;
			}
			ImGui::EndMenu();
		}
		if(ImGui::BeginMenu("Edit")) {
			ImGui::EndMenu();
		}
		if(ImGui::BeginMenu("Debug")) {
			if(ImGui::MenuItem("Show GBuffer")) {
				showGBufferWindow = true;
			}
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}

	// Show Main Menu activated windows
	if(showGBufferWindow) {
		showGBufferWindow = ShowGBufferWindow();
	}

	if(showTestWindow) {
		ImGui::ShowTestWindow(&showTestWindow);
	}

	// Static info panel
	const float fps = io.Framerate;
	const float mspf = 1000.f / fps;
	const vec3f cpos = camera.position;
	const vec3f ctar = camera.target;

	static char fpsText[512];
	snprintf(fpsText, 512, "Average %.3f ms/frame (%.1f FPS)", mspf, fps);
	const f32 fpsTLen = ImGui::CalcTextSize(fpsText).x;

	static char camText[512];
	snprintf(camText, 512, "Camera <%.2f, %.2f, %.2f> <%.2f, %.2f, %.2f>", cpos.x, cpos.y, cpos.z, ctar.x, ctar.y, ctar.z);
	const f32 camTLen = ImGui::CalcTextSize(camText).x;

	static char pickText[512];
	snprintf(pickText, 512, "Pick Object : %d", (int)pickedObject);
	const f32 pickTLen = ImGui::CalcTextSize(pickText).x;

	vec2f wSizef = GetDevice().windowSize;
	ImVec2 wSize(wSizef.x, wSizef.y);
	const ImVec2 panelSize(410, 50);
	const ImVec2 panelPos(wSize.x - panelSize.x, 19);
	
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImColor::HSV(0, 0, 0.9f, 0.15f));
	ImGui::PushStyleColor(ImGuiCol_Text, ImColor::HSV(0, 0, 0.4f, 1));
	ImGui::SetNextWindowPos(panelPos);
	ImGui::SetNextWindowSize(panelSize);
	ImGui::Begin("InfoPanel", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
	ImGui::SameLine(ImGui::GetContentRegionMax().x - fpsTLen);
	ImGui::Text("%s", fpsText);
	ImGui::Text("%s", "");
	ImGui::SameLine(ImGui::GetContentRegionMax().x - camTLen);
	ImGui::Text("%s", camText);
	ImGui::Text("%s", "");
	ImGui::SameLine(ImGui::GetContentRegionMax().x - pickTLen);
	ImGui::Text("%s", pickText);
	ImGui::End();
	ImGui::PopStyleColor(2);
}

void Scene::Update(float dt) {
	Device &device = GetDevice();


	// Ctrl+R to hot reload shaders
	if(device.IsKeyHit(K_R) && device.IsKeyDown(K_LCtrl)) {
		Render::ReloadShaders();
		camera.hasMoved = true; // to reupload view matrices
		device.UpdateProjection(); // to reupload projection matrices
	}

	// Ctrl+G to toggle Ground Truth ray tracing mode
	if(device.IsKeyHit(K_G) && device.IsKeyDown(K_LCtrl)) {
		Render::ToggleGTRaytracing();
	}

	// Real-time camera updating
	camera.Update(dt);
	if(camera.hasMoved) {
		camera.hasMoved = false;
		UpdateView();
	}

	// Mouse Picking
	if (device.IsMouseHit(MouseButton::MB_Left)) {
		pickedObject = (Object::Handle) Render::FBO::ReadObjectID(device.GetMouseX(), device.windowSize.y - device.GetMouseY());
	}

	UpdateGUI();

	static f32 ai_timer = 0.f, one_sec = 0.f;
	ai_timer += dt; one_sec += dt;

	// Updates 100 times per second
	if (ai_timer >= 0.01f) {
		ai_timer = 0.f;

		// update animation for all objects
		//for (u32 j = 0; j < objects.size(); ++j) {
		//	Object::Desc &object = objects[j];
			//if (object.animation > Render::Mesh::ANIM_NONE) {
				//Render::Mesh::UpdateAnimation(object.mesh, object.animation_state, 0.01f);
			//}
		//}
	}

	// Update every second
	if (one_sec >= 1.f) {
		one_sec = 0.f;
		
	}

	if(customUpdateFunc)
		customUpdateFunc(this, dt);
}

bool Scene::InitLightUniforms() {
	// Point Lights
	Render::UBO::Desc ubo_desc(NULL, SCENE_MAX_ACTIVE_LIGHTS * sizeof(PointLight::UniformBufferData), Render::UBO::ST_DYNAMIC);
	pointLightsUBO = Render::UBO::Build(ubo_desc);
	if(pointLightsUBO < 0) {
		LogErr("Error creating point light's UBO.");
		return false;
	}

	// Area Lights
	Render::UBO::Desc area_ubo_desc(NULL, SCENE_MAX_ACTIVE_LIGHTS * sizeof(AreaLight::UniformBufferData), Render::UBO::ST_DYNAMIC);
	areaLightsUBO = Render::UBO::Build(area_ubo_desc);
	if(areaLightsUBO < 0) {
		LogErr("Error creating area light's UBO.");
		return false;
	}

	return true;
}
void InitRectPoints(AreaLight::UniformBufferData &rect, vec3f points[4]) {
    vec3f ex = rect.dirx * rect.hwidthx;
    vec3f ey = rect.diry * rect.hwidthy;

	// LogInfo(rect.diry.x, " ", rect.diry.y, " ", rect.diry.z, " | ", ey.x, " ", ey.y);

    points[0] = rect.position - ex - ey;
    points[1] = rect.position + ex - ey;
    points[2] = rect.position + ex + ey;
    points[3] = rect.position - ex + ey;
}
u32 Scene::AggregateAreaLightUniforms() {
	static AreaLight::UniformBufferData fullUBO[SCENE_MAX_ACTIVE_LIGHTS];

	u32 numActiveLights = 0;
	for (u32 l = 0; l < SCENE_MAX_ACTIVE_LIGHTS; ++l) {
		if (active_areaLights[l] < 0) break;
		AreaLight::Desc &src = areaLights[active_areaLights[l]];

		++numActiveLights;

		fullUBO[l].position = src.position;
		fullUBO[l].Ld = src.Ld;
		fullUBO[l].hwidthx = src.width.x * 0.5f;
		fullUBO[l].hwidthy = src.width.y * 0.5f;


		mat4f m = mat4f::Scale(vec3f(src.width.x, src.width.y, 1));
		m = m.RotateX(src.rotation.x);
		m = m.RotateY(src.rotation.y);
		m = m.RotateZ(src.rotation.z);


		fullUBO[l].dirx = m * vec3f(1,0,0);
		fullUBO[l].dirx.Normalize();
		fullUBO[l].diry = m * vec3f(0,1,0);
		fullUBO[l].diry.Normalize();

		vec3f N = fullUBO[l].dirx.Cross(fullUBO[l].diry);
		N.Normalize();
		fullUBO[l].plane = vec4f(N.x, N.y, N.z, - N.Dot(src.position));

		// Create mesh representation
		if(src.fixture < 0) {
			f32 pos[] = { 
				-0.5, -0.5, 0,
				0.5, -0.5, 0,
				0.5, 0.5, 0,
				-0.5, 0.5, 0
			};
			f32 normals[] = {
				0, 0, 1,
				0, 0, 1,
				0, 0, 1,
				0, 0, 1
			};
			u32 idx[] = { 0, 1, 2, 0, 2, 3, 3, 2, 1, 3, 1, 0 };
			Render::Mesh::Desc md("Quad", false, 12, idx, 4, pos, normals);
			Render::Mesh::Handle mh = Render::Mesh::Build(md);
			if(mh < 0) {
				LogErr("err");
				continue;
			}

			// TODO : other shader for light fixtures
			Object::Desc od(Render::Shader::SHADER_3D_MESH);
			Material::Desc matd(col3f(src.Ld.x, src.Ld.y, src.Ld.z), col3f(0,0,0), col3f(0,0,0), 0);
			Material::Handle math = Add(matd);
			if(math < 0) {
				LogErr("mat err");
				continue;
			}
			od.AddSubmesh(mh, math);
			od.Identity();
			od.Translate(src.position);
			od.Rotate(src.rotation);
			od.Scale(vec3f(src.width.x, src.width.y, 1.f));
			src.fixture = Add(od);
			if(src.fixture < 0) {
				LogErr("obj err");
				continue;
			}
		} else {
			// if it exists, just modify it
			Object::Desc *od = GetObject(src.fixture);
			if(od) {
				od->Identity();
				od->Translate(src.position);
				od->Rotate(src.rotation);
				od->Scale(vec3f(src.width.x, src.width.y, 1.f));
			}
		}
	}

	// Update UBO
	Render::UBO::Desc ubo_desc((f32*)fullUBO, numActiveLights * sizeof(AreaLight::UniformBufferData), Render::UBO::ST_DYNAMIC);
	Render::UBO::Update(areaLightsUBO, ubo_desc);

	// Bind it
	Render::UBO::Bind(Render::Shader::UNIFORMBLOCK_AREALIGHTS, areaLightsUBO);

	return numActiveLights;
}

u32 Scene::AggregatePointLightUniforms() {
	static PointLight::UniformBufferData fullUBO[SCENE_MAX_ACTIVE_LIGHTS];

	u32 numActiveLights = 0;
	for (u32 l = 0; l < SCENE_MAX_ACTIVE_LIGHTS; ++l) {
		if (active_pointLights[l] < 0) break;
		const PointLight::Desc &src = pointLights[active_pointLights[l]];

		++numActiveLights;

		fullUBO[l].position = src.position;
		fullUBO[l].Ld = src.Ld;
		fullUBO[l].radius = src.radius;
	}
	// Update UBO
	Render::UBO::Desc ubo_desc((f32*)fullUBO, numActiveLights * sizeof(PointLight::UniformBufferData), Render::UBO::ST_DYNAMIC);
	Render::UBO::Update(pointLightsUBO, ubo_desc);

	// Bind it
	Render::UBO::Bind(Render::Shader::UNIFORMBLOCK_POINTLIGHTS, pointLightsUBO);

	return numActiveLights;
}

void Scene::Render() {
	const Device &device = GetDevice();

	if (device.IsWireframe()) {
		glDisable(GL_CULL_FACE);
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	}

	// GBuffer Pass
	Render::StartGBufferPass();
	for (u32 j = 0; j < objects.size(); ++j) {
		Object::Desc &object = objects[j];
		Render::Shader::SendInt(Render::Shader::UNIFORM_OBJECTID, j);

		// TODO : model_matrix from TRS should be done in update function, not in render
		object.model_matrix.FromTRS(object.position, object.rotation, object.scale);
		Render::Shader::SendMat4(Render::Shader::UNIFORM_MODELMATRIX, object.model_matrix);

		// Render all submeshes
		for(u32 m = 0; m < object.numSubmeshes; ++m) {
			Render::Mesh::Render(object.meshes[m]);
		}
	}
	Render::StopGBufferPass();

	Render::StartPolygonRendering();

	// glEnable(GL_FRAMEBUFFER_SRGB);

	// Update light uniform buffer
	u32 numPointLights = AggregatePointLightUniforms();
	u32 numAreaLights = AggregateAreaLightUniforms();

	float globalTime = (float)glfwGetTime();
	for (u32 j = 0; j < objects.size(); ++j) {
		Object::Desc &object = objects[j];
		Render::Shader::Bind(object.shader);
		Render::Shader::SendFloat(Render::Shader::UNIFORM_GLOBALTIME, globalTime);
		Render::Shader::SendInt(Render::Shader::UNIFORM_NPOINTLIGHTS, numPointLights);
		Render::Shader::SendInt(Render::Shader::UNIFORM_NAREALIGHTS, numAreaLights);

		object.model_matrix.FromTRS(object.position, object.rotation, object.scale);
		Render::Shader::SendMat4(Render::Shader::UNIFORM_MODELMATRIX, object.model_matrix);

		// Render all submeshes
		for(u32 m = 0; m < object.numSubmeshes; ++m) {
			const Material::Data &material = materials[object.materials[m]];

			// send material parameters
			Render::UBO::Bind(Render::Shader::UNIFORMBLOCK_MATERIAL, material.ubo);
			Render::Texture::Bind(material.diffuseTex, Render::Texture::TARGET0);
			Render::Texture::Bind(material.specularTex, Render::Texture::TARGET1);
			Render::Texture::Bind(material.normalTex, Render::Texture::TARGET2);
			Render::Texture::Bind(material.occlusionTex, Render::Texture::TARGET3);
			Render::Texture::Bind(material.ltcMatrix, Render::Texture::TARGET4);
			Render::Texture::Bind(material.ltcAmplitude, Render::Texture::TARGET5);

			Render::Mesh::Render(object.meshes[m]);
		}
	}

	if(customRenderFunc)
		customRenderFunc(this);

	// non-wireframe mode for text rendering
	if (device.IsWireframe()) {
		glEnable(GL_CULL_FACE);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}

	// Draw Skybox
	glDisable(GL_CULL_FACE);
	glDepthFunc(GL_LEQUAL);
	Render::Shader::Bind(Render::Shader::SHADER_SKYBOX);
	Render::Mesh::Render(skyboxMesh);
	glDepthFunc(GL_LESS);
	glEnable(GL_CULL_FACE);

	// In case we are doing GT raytracing, accumulate backbuffer
	Render::AccumulateGT();

	// glDisable(GL_FRAMEBUFFER_SRGB);

	Render::StartTextRendering();
	for (u32 i = 0; i < texts.size(); ++i) {
		Text::Desc &text = texts[i];
		Render::Shader::SendMat4(Render::Shader::UNIFORM_MODELMATRIX, text.model_matrix);
		Render::Shader::SendVec4(Render::Shader::UNIFORM_TEXTCOLOR, text.color);
		Render::Font::Bind(text.font, Render::Texture::TARGET0);
		Render::TextMesh::Render(text.mesh);
	}
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


Object::Handle Scene::InstanciateModel(const ModelResource::Handle &h) {
	size_t index = objects.size();
	ModelResource::Data &model = models[h];

		// Material::Desc mat_desc(col3f(0.181,0.1,0.01), col3f(.9,.5,.5), col3f(1,.8,0.2), 0.8);

	Object::Desc odesc(Render::Shader::SHADER_3D_MESH);//, model.subMeshes[i], model.materials[matIdx]);
	for(u32 i = 0; i < model.numSubMeshes; ++i) {
		u32 matIdx = model.materialIdx[i];
		odesc.AddSubmesh(model.subMeshes[i], model.materials[matIdx]);
		// odesc.Translate(vec3f(-25,0,0));
		// odesc.Rotate(vec3f(0,-2.f*M_PI/2.3f,0));
		// odesc.Scale(vec3f(15,15,15));
		// odesc.model_matrix *= mat4f::Scale(10,10,10);
	}

	Object::Handle obj_h = Add(odesc);
	if(obj_h < 0) {
		LogErr("Error creating Object from Model.");
		return -1;
	}
	return obj_h;
}

Object::Handle Scene::Add(const Object::Desc &d) {
	if (!Render::Shader::Exists(d.shader)) {
		LogErr("Given shader is not registered in renderer.");
		return -1;
	}

	// Check submesh existence
	for(u32 i = 0; i < d.numSubmeshes; ++i) {
		if (!Render::Mesh::Exists(d.meshes[i])) {
			LogErr("Submesh ", i, " is not registered in renderer.");
			return -1;
		}
		if(!MaterialExists(d.materials[i])) {
			LogErr("Material ", i, " is not registered in the scene.");
			return -1;
		}
	}

	size_t index = objects.size();

	if (index >= SCENE_MAX_OBJECTS) {
		LogErr("Reached maximum number (", SCENE_MAX_OBJECTS, ") of objects in scene.");
		return -1;
	}

	objects.push_back(d);

	return (Object::Handle)index;
}

PointLight::Handle Scene::Add(const PointLight::Desc &d) {
	size_t index = pointLights.size();

	if (index >= SCENE_MAX_LIGHTS) {
		LogErr("Reached maximum number (", SCENE_MAX_LIGHTS, ") of point lights in scene.");
		return -1;
	}

	pointLights.push_back(d);

	PointLight::Desc &light = pointLights[index];
	light.active = true;
	// look for active slot
	for(u32 i = 0; i < SCENE_MAX_ACTIVE_LIGHTS; ++i) {
		if(active_pointLights[i] < 0) {
			active_pointLights[i] = (int) index;
			break;
		}
	}

	return (PointLight::Handle)index;
}

AreaLight::Handle Scene::Add(const AreaLight::Desc &d) {
	size_t index = areaLights.size();

	if (index >= SCENE_MAX_LIGHTS) {
		LogErr("Reached maximum number (", SCENE_MAX_LIGHTS, ") of area lights in scene.");
		return -1;
	}

	areaLights.push_back(d);

	AreaLight::Desc &light = areaLights[index];
	light.active = true;
	// look for active slot
	for(u32 i = 0; i < SCENE_MAX_ACTIVE_LIGHTS; ++i) {
		if(active_areaLights[i] < 0) {
			active_areaLights[i] =(int) index;
			break;
		}
	}

	return (AreaLight::Handle)index;
}

AreaLight::Desc *Scene::GetLight(AreaLight::Handle h) {
	if(AreaLightExists(h))
		return &areaLights[h];
	return nullptr;
}

bool Scene::AreaLightExists(AreaLight::Handle h) {
	return h >= 0 && h < (int)areaLights.size();
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
	size_t idx = materials.size();

	Material::Data mat;
	mat.desc = d;

	// Create UBO to accomodate it on GPU
	Render::UBO::Desc ubo_desc((f32*)&d.uniform, sizeof(Material::Desc::UniformBufferData), Render::UBO::ST_STATIC);
	mat.ubo = Render::UBO::Build(ubo_desc);
	if(mat.ubo < 0) {
		LogErr("Error creating Material");
		return -1;
	}

	// Load textures if present
	if(d.diffuseTexPath != "") {	// Diffuse
		Render::Texture::Desc t_desc(d.diffuseTexPath);
        Render::Texture::Handle t_h = Render::Texture::Build(t_desc);
        if(t_h < 0) {
        	LogErr("Error loading diffuse texture ", d.diffuseTexPath);
        	return -1;
        }
		mat.diffuseTex = t_h;
	} else {
		// Default diffuse texture
		mat.diffuseTex = Render::Texture::DEFAULT_DIFFUSE;
	}

	if(d.specularTexPath != "") {
		Render::Texture::Desc t_desc(d.specularTexPath);
        Render::Texture::Handle t_h = Render::Texture::Build(t_desc);
        if(t_h < 0) {
        	LogErr("Error loading specular texture ", d.specularTexPath);
        	return -1;
        }
		// LogDebug("Loaded specular texture at idx ", t_h);
		mat.specularTex = t_h;
	} else {
		mat.specularTex = Render::Texture::DEFAULT_DIFFUSE;	 // 1 specular, multiplied by the mat.shininess
	}

	if(d.normalTexPath != "") {
		Render::Texture::Desc t_desc(d.normalTexPath);
		Render::Texture::Handle t_h = Render::Texture::Build(t_desc);
		if(t_h < 0) {
			LogErr("Error loading normal texture ", d.normalTexPath);
			return -1;
		}
		mat.normalTex = t_h;
	} else {
		mat.normalTex = Render::Texture::DEFAULT_NORMAL;
	}

	if(d.occlusionTexPath != "") {
		Render::Texture::Desc t_desc(d.occlusionTexPath);
		Render::Texture::Handle t_h = Render::Texture::Build(t_desc);
		if(t_h < 0) {
			LogErr("Error loading occlusion texture ", d.occlusionTexPath);
			return -1;
		}
		mat.occlusionTex = t_h;
	} else {
		mat.occlusionTex = Render::Texture::DEFAULT_DIFFUSE;
	}

	if(d.ltcMatrixPath != "") {
		Render::Texture::Desc t_desc(d.ltcMatrixPath);
		Render::Texture::Handle t_h = Render::Texture::Build(t_desc);
		if(t_h < 0) {
			LogErr("Error loading ltcMatrix texture ", d.ltcMatrixPath);
			return -1;
		}
		mat.ltcMatrix = t_h;
	} else {
		mat.ltcMatrix = Render::Texture::DEFAULT_DIFFUSE;
	}

	if(d.ltcAmplitudePath != "") {
		Render::Texture::Desc t_desc(d.ltcAmplitudePath);
		Render::Texture::Handle t_h = Render::Texture::Build(t_desc);
		if(t_h < 0) {
			LogErr("Error loading ltcAmplitude texture ", d.ltcAmplitudePath);
			return -1;
		}
		mat.ltcAmplitude = t_h;
	} else {
		mat.ltcAmplitude = Render::Texture::DEFAULT_DIFFUSE;
	}

	materials.push_back(mat);
	return (Material::Handle) idx;
}

Object::Desc *Scene::GetObject(Object::Handle h) {
	if(ObjectExists(h))
		return &objects[h];
	return nullptr;
}

bool Scene::ObjectExists(Object::Handle h) {
	return h >= 0 && h < (int)objects.size();
}

Skybox::Handle Scene::Add(const Skybox::Desc &d) {
	size_t idx = skyboxes.size();

	Skybox::Data sky;
	Render::Texture::Desc td;
	td.type = Render::Texture::Cubemap;
	for(u32 i = 0; i < 6; ++i) {
		td.name[i] = d.filenames[i];
	}
	sky.cubemap = Render::Texture::Build(td);
	if(sky.cubemap < 0) {
		LogErr("Error creating Skybox.");
		return -1;
	}

	skyboxes.push_back(sky);
	return (Skybox::Handle) idx;
}

void Scene::SetSkybox(Skybox::Handle h) {
	currSkybox = h;
	Render::Texture::BindCubemap(skyboxes[currSkybox].cubemap, Render::Texture::TARGET0);
}