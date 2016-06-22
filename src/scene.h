#pragma once

#include "render.h"
#include "common/event.h"

class Scene;

/// Camera
struct Camera {
	void Update(float dt);
    vec3f	position,
            target,
            up,
            forward,
            right;

    f32  translationSpeed,
         rotationSpeed;
	f32  speedMult;
	
    f32  dist;
    f32  theta,
         phi;

	bool hasMoved;
	bool speedMode;
	bool freeflyMode;
};

namespace Light {
	/// Light Descriptor
	struct Desc {
		vec3f   position;   //!< 3D position in world-coordinates
		vec4f   amb_color;  //!< Light Ambient Color. w-coord unused
		vec4f   diff_color; //!< Light Diffuse Color. w-coord unused
		f32		radius;     //!< Light radius. Light is 0 at the r
		bool    active;
	};

	/// Handle representing a light in the scene.
	/// This can be used to modify or delete the light after creation.
	typedef int Handle;
}

namespace Material {
	using namespace Render;
	struct Desc {
		Desc() :	// Default debug material
			uniform(col3f(0.3, 0.0, 0.3), col3f(0.51,0.4,0.51), col3f(0.7,0.04,0.7), 0.95f), 
			diffuseTexPath(""), specularTexPath(""), normalTexPath("") {}

		Desc(const col3f &ka, const col3f &kd, const col3f &ks, float s, const std::string diffuse = "") : 
			uniform(ka, kd, ks, s), diffuseTexPath(diffuse), specularTexPath(""), normalTexPath("") {}

		struct UniformBufferData {
			UniformBufferData(const col3f &ka, const col3f &kd, const col3f &ks, float s) : Ka(ka), Kd(kd), Ks(ks), shininess(s) {}
			//----
			col3f 	Ka;			//!< ambient color
			f32 	dummy0;
			//----
			col3f 	Kd;			//!< diffuse color
			f32		dummy1;
			//----
			col3f 	Ks;			//!< specular color
			f32		shininess; 	//!< between 0.0 and 1.0, inverse of roughness
			//----
		};

		UniformBufferData uniform;
		std::string diffuseTexPath;
		std::string specularTexPath;
		std::string normalTexPath;
	};

	struct Data {
		Data() : ubo(-1), diffuseTex(-1), specularTex(-1), normalTex(-1) {}
		Desc desc;
		UBO::Handle ubo;

		Texture::Handle diffuseTex;
		Texture::Handle specularTex;
		Texture::Handle normalTex;
	};

	typedef int Handle;

	extern Handle DEFAULT_MATERIAL;	//!< Default material (pink diffuse), defined in scene.cpp Init()
}

namespace Object {
	using namespace Render;
	struct Desc {
		friend Scene;

		Desc(Shader::Handle shader_h)//, Mesh::Handle mesh_h, Material::Handle mat_h = Material::DEFAULT_MATERIAL)
		: shader(shader_h), numSubmeshes(0) {
			position = vec3f();
			rotation = vec3f();
			scale = 1.f;
			model_matrix.Identity();
		}

		void AddSubmesh(Mesh::Handle mesh_h, Material::Handle mat_h) {
			meshes.push_back(mesh_h);
			materials.push_back(mat_h);
			++numSubmeshes;
		}

		void ClearSubmeshes() {
			numSubmeshes = 0;
			meshes.clear();
			materials.clear();
		}

		void Identity();
		void Translate(const vec3f &t);
		void Scale(f32 s);
		void Rotate(const vec3f &r);

		Shader::Handle   shader;

		// Mesh::AnimType   animation;
		// Mesh::AnimState  animation_state;

	private:
		std::vector<Mesh::Handle>		meshes;
		std::vector<Material::Handle> 	materials;
		mat4f					model_matrix;
		vec3f	position;
		vec3f	rotation;
		f32 	scale;

		u32 numSubmeshes;
	};

	/// Handle representing an object in the scene.
	/// This can be used to modify or delete the object after creation.
	typedef int Handle;
}

class aiNode;
class aiScene;
class aiMesh;

namespace ModelResource {
	typedef int Handle;

	struct Data {
		Data() : resourceName("UNNAMED"), pathName(""), numSubMeshes(0) {}

		// Loaded resources 
		// std::vector<Render::Texture::Handle> textures;
		std::vector<Material::Handle> materials;

		// Per-submesh data
		std::vector<Render::Mesh::Handle> subMeshes;
		std::vector<u32> materialIdx;	// indexes the materials arrays
		// TODO : relative matrices


		std::string resourceName;
		std::string pathName;
		u32			numSubMeshes;
	};

    bool _ProcessAssimpNode(Scene *gameScene, Data &model, aiNode *node, const aiScene *scene);
    bool _ProcessAssimpMesh(Scene *gameScene, Data &model, aiMesh *mesh, const aiScene *scene);
	bool _ProcessAssimpMaterials(Scene *gameScene, Data &model, const aiScene *scene);
};

namespace Text {
	struct Desc {
		Desc(const std::string &string, Render::Font::Handle fh, const vec4f &col)
		: color(col), str(string), font(fh), mesh(-1) {}

		void SetPosition(const vec2f &pos);

		mat4f						model_matrix;
		vec4f						color;
		std::string					str;
		Render::Font::Handle		font;
		Render::TextMesh::Handle	mesh;
	};

	/// Handle representing a text in the scene.
	/// This can be used to modify or delete the text after creation.
	typedef int Handle;
}

class Scene;
typedef bool (*SceneInitFunc)(Scene *scene);
typedef void (*SceneUpdateFunc)(Scene *scene, float dt);
typedef void (*SceneRenderFunc)(Scene *scene);

class Scene {
public:
	Scene();

	bool Init(SceneInitFunc initFunc, SceneUpdateFunc updateFunc, SceneRenderFunc renderFunc);
	void Clean();

	void Update(f32 dt);
	void Render();

	void UpdateView();

	const mat4f& GetViewMatrix() const { return view_matrix; }
	Camera &GetCamera() { return camera; }

	bool MaterialExists(Material::Handle h) const;
	void SetTextString(Text::Handle h, const std::string &str);

	ModelResource::Handle GetModelResource(const std::string &modelName);

	ModelResource::Handle LoadModelResource(const std::string &fileName);
	Object::Handle AddFromModel(const ModelResource::Handle &h);
	Object::Handle Add(const Object::Desc &d);
	Light::Handle Add(const Light::Desc &d);
	Text::Handle Add(const Text::Desc &d);
	Material::Handle Add(const Material::Desc &d);

	// Object functions
	Object::Desc *GetObject(Object::Handle h);
	// ModelResource::Data *GetModelInstance(ModelResource::Handle h);
	inline bool Exists(Object::Handle h);

private:
	std::vector<Text::Desc> texts;

	std::vector<Light::Desc> lights;
	std::vector<int> active_lights;

	std::vector<Object::Desc> objects;
	std::vector<Material::Data> materials;

	std::vector<ModelResource::Data> models;

	mat4f view_matrix;
	Camera camera;

	SceneInitFunc 	customInitFunc;
	SceneUpdateFunc customUpdateFunc;
	SceneRenderFunc customRenderFunc;

	// Debug texts
	Text::Handle fps_text;
	Text::Handle camera_text;
};

/// Listener callback function for the scene
void SceneResizeEventListener(const Event &event, void *data);
