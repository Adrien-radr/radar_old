#pragma once

#include "render.h"
#include "common/event.h"


/// Camera
struct Camera {
    vec3f    position,
            target,
            up,
            forward,
            right;

    f32  mov_speed,
         rot_speed;
    f32  dist;
    f32  theta,
         phi;
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

namespace Object {
	using namespace Render;
	struct Desc {
		Desc(Mesh::Handle mh, Mesh::AnimType anim, Shader::Handle sh, Texture::Handle th)
		: mesh(mh), animation(anim), shader(sh), texture(th) {}

		void SetPosition(const vec2f &pos);
		void SetRotation(float angle);

		Mesh::Handle	mesh;
		Mesh::AnimType  animation;
		Mesh::AnimState	animation_state;
		Shader::Handle  shader;
		Texture::Handle texture;

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

	Object::Handle Add(const Object::Desc &d);
	Light::Handle Add(const Light::Desc &d);
	Text::Handle Add(const Text::Desc &d);

	void SetTextString(Text::Handle h, const std::string &str);

private:
	//u32 text_n;			//!< Number of registered texts
	std::vector<Text::Desc> texts;

	//u32 light_n;		//!< Number of registered lights
	std::vector<Light::Desc> lights;
	std::vector<int> active_lights;

	//u32 object_n;		//!< Number of registered objects
	std::vector<Object::Desc> objects;

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
