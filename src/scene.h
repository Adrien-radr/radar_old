#pragma once

#include "render.h"
#include "common/event.h"


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
	struct Desc {
		Desc() : Kd(1,0,1), Ks(0,1,1), shininess(0.8f) {}	// Default debug material	
		Desc(const col3f &kd, const col3f &ks, float s) : Kd(kd), Ks(ks), shininess(s) {}

		//----
		col3f 	Kd;			//!< diffuse color
		float dummy1;
		//----
		col3f 	Ks;			//!< specular color
		f32		shininess; 	//!< between 0.0 and 1.0, inverse of roughness
		//----
	};

	struct Data {
		Data() : ubo(-1) {}
		Desc desc;
		Render::UBO::Handle ubo;
	};

	typedef int Handle;
}

namespace Object {
	using namespace Render;
	struct Desc {
		Desc(Mesh::Handle mesh_h, Mesh::AnimType anim, Shader::Handle shader_h,
			 Texture::Handle tex_h, Material::Handle mat_h)
		: mesh(mesh_h), animation(anim), shader(shader_h), texture(tex_h), material(mat_h) {
			model_matrix.Identity();
		}

		// void SetPosition(const vec3f &pos);
		// void SetRotation(float angle);
		// void SetScale(const vec3f &scale);

		void Translate(const vec3f &t);
		void Scale(const vec3f &s);

		Mesh::Handle	 mesh;
		Mesh::AnimType   animation;
		Mesh::AnimState  animation_state;
		Shader::Handle   shader;
		Texture::Handle  texture;
		Material::Handle material;

		mat4f					model_matrix;
	};

	/// Handle representing an object in the scene.
	/// This can be used to modify or delete the object after creation.
	typedef int Handle;
}

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

	Object::Handle Add(const Object::Desc &d);
	Light::Handle Add(const Light::Desc &d);
	Text::Handle Add(const Text::Desc &d);
	Material::Handle Add(const Material::Desc &d);
	bool MaterialExists(Material::Handle h) const;

	void SetTextString(Text::Handle h, const std::string &str);

private:
	std::vector<Text::Desc> texts;

	std::vector<Light::Desc> lights;
	std::vector<int> active_lights;

	std::vector<Object::Desc> objects;
	std::vector<Material::Data> materials;

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
