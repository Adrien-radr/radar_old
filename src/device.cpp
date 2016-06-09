#include "device.h"
#include "json/cJSON.h"

#include <cstring>
#include <algorithm>
////////////////////////////////////////////////////////////////
///     EVENT & INPUT
////////////////////////////////////////////////////////////////

/// Structure containing the state of all inputs at a given time
struct InputState{
    bool keyboard[K_ENDFLAG];   ///< Keyboard keys
    bool mouse[MB_ENDFLAG];     ///< Mouse buttons
    int  wheel;                 ///< Mouse wheel absolute pos

    bool close_signal;          ///< Window closing signal

    vec2i mouse_pos;            ///< Mouse absolute position
};


struct Listener {
	Listener(ListenerFunc func, void *d) : function(func), data(d) {}

    ListenerFunc    function;   ///< actual listener function
    void            *data;      ///< data to be set when registering listener
};



/// Manage real time events from GLFW callbacks
/// Distribute these events to registered listeners
class EventManager {
public:
	EventManager();

	/// Called every frame to update the input & event states
	void Update();

	/// Adds a new listener of the given type to the event manager
	/// @return : true if the operation was successful
	bool AddListener(ListenerType type, ListenerFunc func, void *data);

	/// Sends an event to the connected listeners
	void PropagateEvent(const Event &evt);

    InputState				curr_state,         ///< Inputs of current frame
							prev_state;         ///< Inputs of previous frame

    std::vector<Listener>   key_listeners,      ///< List of all registered mouse listeners
							mouse_listeners,    ///< List of all registered key listeners
							resize_listeners;   ///< List of all registered resize listeners

    std::vector<Event>      frame_key_events,    ///< All key events recorded during the frame
							frame_mouse_events,  ///< All mouse events recorded during the frame
							frame_resize_events; ///< All resize events recorded during the frame

};

static EventManager *em = NULL;


EventManager::EventManager() {
    // Init states
    memset(curr_state.keyboard, false, K_ENDFLAG * sizeof(bool));
    memset(curr_state.mouse, false, MB_ENDFLAG * sizeof(bool));
    curr_state.wheel = 0;
    curr_state.close_signal = false;

    memset(prev_state.keyboard, false, K_ENDFLAG * sizeof(bool));
    memset(prev_state.mouse, false, MB_ENDFLAG * sizeof(bool));
    prev_state.wheel = 0;
    prev_state.close_signal = false;

    // Init Listener arrays
	key_listeners.reserve(10);
	mouse_listeners.reserve(10);
	resize_listeners.reserve(5);

	frame_key_events.reserve(50);
	frame_mouse_events.reserve(50);
	frame_resize_events.reserve(50);

    LogInfo("Event manager successfully initialized!");
}


void EventManager::Update() {
    // if there has been key events during the frame, send them to all keylisteners
	if (frame_key_events.size()) {
		for (u32 j = 0; j < frame_key_events.size(); ++j)
			for(u32 i = 0; i < key_listeners.size(); ++i) {
				const Listener &ls = key_listeners[i];
                ls.function(frame_key_events[j], ls.data);
			}

		frame_key_events.clear();
    }
    // if there has been mouse events during the frame, send them to all mouselisteners
	if (frame_mouse_events.size()) {
		for (u32 j = 0; j < frame_mouse_events.size(); ++j)
			for(u32 i = 0; i < mouse_listeners.size(); ++i) {
				const Listener &ls = mouse_listeners[i];
                ls.function(frame_mouse_events[j], ls.data);
			}

		frame_mouse_events.clear();
    }
    // if there has been resize events during the frame, send them to all resizelisteners
	if (frame_resize_events.size()) {
		for (u32 j = 0; j < frame_resize_events.size(); ++j)
			for(u32 i = 0; i < resize_listeners.size(); ++i) {
				const Listener &ls = resize_listeners[i];
                ls.function(frame_resize_events[j], ls.data);
			}

		frame_resize_events.clear();
    }



    // Set previous state to current state.
    memcpy(prev_state.keyboard, curr_state.keyboard, K_ENDFLAG * sizeof(bool));
    memcpy(prev_state.mouse, curr_state.mouse, MB_ENDFLAG * sizeof(bool));

    prev_state.wheel = curr_state.wheel;
    prev_state.close_signal = curr_state.close_signal;
    prev_state.mouse_pos[0] = curr_state.mouse_pos[0];
    prev_state.mouse_pos[1] = curr_state.mouse_pos[1];
}

bool EventManager::AddListener(ListenerType type, ListenerFunc func, void *data) {
	Listener s(func, data);

	// switch on Listener type
	switch (type) {
	case LT_KeyListener:
		key_listeners.push_back(s);
		break;
	case LT_MouseListener:
		mouse_listeners.push_back(s);
		break;
	case LT_ResizeListener:
		resize_listeners.push_back(s);
		break;
	default:
		return false;
	}


	return true;
}

void EventManager::PropagateEvent(const Event &evt) {
    switch(evt.type) {
        case EKeyPressed:
        case EKeyReleased:
        case ECharPressed:
			frame_key_events.push_back(evt);
            break;
        case EMouseMoved:
        case EMousePressed:
        case EMouseReleased:
        case EMouseWheelMoved:
			frame_mouse_events.push_back(evt);
            break;
        case EWindowResized:
			frame_resize_events.push_back(evt);
            break;
		default:
			break;
    }
}

u32  Device::GetMouseX() const {
    return em->curr_state.mouse_pos[0];
}

u32  Device::GetMouseY() const {
    return em->curr_state.mouse_pos[1];
}


bool Device::IsKeyDown(Key pK) const {
	return em->curr_state.keyboard[pK];
}

bool Device::IsKeyUp(Key pK) const {
	return !em->curr_state.keyboard[pK] && em->prev_state.keyboard[pK];
}

bool Device::IsKeyHit(Key pK) const {
	return em->curr_state.keyboard[pK] && !em->prev_state.keyboard[pK];
}


bool Device::IsMouseDown(MouseButton pK) const {
	return em->curr_state.mouse[pK];
}

bool Device::IsMouseUp(MouseButton pK) const {
	return em->curr_state.mouse[pK] && !em->prev_state.mouse[pK];
}

bool Device::IsMouseHit(MouseButton pK) const {
	return em->curr_state.mouse[pK] && !em->prev_state.mouse[pK];
}


bool Device::IsWheelUp() const {
	return em->curr_state.wheel > em->prev_state.wheel;
}

bool Device::IsWheelDown() const {
	return em->curr_state.wheel < em->prev_state.wheel;
}

// GLFW Event Callback functions
// TODO : block events from being propagated to other listeners in some cases
//		  example : when using inputs on GUI, they aren't used on the game itself(the scene)
    static void KeyPressedCallback(GLFWwindow *win, int key, int scancode, int action, int mods) {
        bool pressed = (action == GLFW_PRESS || action == GLFW_REPEAT);

        em->curr_state.keyboard[key] = pressed ? true : false;

		Event e;
		e.type = (pressed ? EKeyPressed : EKeyReleased);
		e.i = key;
        e.key = (Key)key;

        em->PropagateEvent(e);
    }

    static void CharPressedCallback(GLFWwindow *win, unsigned int c) {
		Event e;
		e.type = ECharPressed;
		e.i = c;

        em->PropagateEvent(e);
    }

    static void MouseButtonCallback(GLFWwindow *win, int button, int action, int mods) {
        bool pressed = action == GLFW_PRESS;
        em->curr_state.mouse[button] = pressed;

		Event e;
		e.type = pressed ? EMousePressed : EMouseReleased;
		e.v = em->curr_state.mouse_pos;
        e.button = (MouseButton)button;

        em->PropagateEvent(e);
    }

    static void MouseWheelCallback(GLFWwindow *win, double off_x, double off_y) {
        em->curr_state.wheel += (int)off_y;

		Event e;
		e.type = EMouseWheelMoved;
		e.i = (em->curr_state.wheel - em->prev_state.wheel);

		em->PropagateEvent(e);
    }

    static void MouseMovedCallback(GLFWwindow *win, double x, double y) {
        em->curr_state.mouse_pos = vec2i((int)x, (int)y);

		Event e;
		e.type = EMouseMoved;
		e.v = vec2i((int)x, (int)y);

        em->PropagateEvent(e);
    }

    static void WindowResizeCallback(GLFWwindow *win, int width, int height) {
		Event e;
		e.type = EWindowResized;
		e.v = vec2i(width, height);

		em->PropagateEvent(e);
    }

    static void ErrorCallback(int error, char const *desc) {
        LogErr(desc);
    }

//////////////////////
struct Config {
    vec2i   window_size;
    bool    fullscreen;
    u32     MSAA_samples;
    f32     fov;
    bool    vsync;
};

static bool LoadConfig(Config &config) {
	Json conf_file;
	if (!conf_file.Open("config.json")) {
		return false;
	}

    config.window_size.x = Json::ReadInt(conf_file.root, "WindowWidth", 1024);
    config.window_size.y = Json::ReadInt(conf_file.root, "WindowHeight", 768);
	config.MSAA_samples = Json::ReadInt(conf_file.root, "MSAA", 0);
	config.fullscreen = Json::ReadInt(conf_file.root, "FullScreen", 0) != 0;
	config.fov = Json::ReadFloat(conf_file.root, "FOV", 75.f);
	config.vsync = Json::ReadInt(conf_file.root, "VSync", 0) != 0;

	conf_file.Close();
    return true;
}
//////////////////////

static void DeviceResizeEventListener(const Event &event, void *data) {
	Device *d = static_cast<Device*>(data);
	d->window_size[0] = event.v[0];
	d->window_size[1] = event.v[1];
	d->window_center[0] = event.v[0] / 2;
	d->window_center[1] = event.v[1] / 2;

	d->UpdateProjection();
}

Device *gDevice = NULL;

Device &GetDevice() {
	if (!gDevice) {
		gDevice = new Device();
	}
	return *gDevice;
}

bool Device::Init(SceneInitFunc sceneInitFunc, SceneUpdateFunc sceneUpdateFunc, SceneRenderFunc sceneRenderFunc) {
    int v;

    // Open and parse config file
    Config config;
	if (!LoadConfig(config)) {
		LogErr("Error loading config file.");
		return false;
	}

	window_size = config.window_size;
	window_center = window_size / 2;
    fov = config.fov;

    // Initialize GLFW and callbacks
	if (!glfwInit()) {
		LogErr("Error initializing GLFW.");
		return false;
	}

    glfwWindowHint(GLFW_SAMPLES, config.MSAA_samples);

	std::stringstream window_name;
	window_name << "Radar v" << X4_2525_MAJOR << "." << X4_2525_MINOR << "." << X4_2525_PATCH;
    window = glfwCreateWindow(window_size.x, window_size.y, window_name.str().c_str(),
                              config.fullscreen ? glfwGetPrimaryMonitor() : NULL, NULL);
	if (!window) {
		LogErr("Error initializing GLFW window.");
        glfwTerminate();
        return false;
	}

    glfwSwapInterval((int)config.vsync);

    if(!config.fullscreen)
        glfwSetWindowPos(gDevice->window, 100, 100);

    glfwMakeContextCurrent(gDevice->window);
    glfwSetKeyCallback(gDevice->window, KeyPressedCallback);
    glfwSetCharCallback(gDevice->window, CharPressedCallback);
    glfwSetMouseButtonCallback(gDevice->window, MouseButtonCallback);
    glfwSetCursorPosCallback(gDevice->window, MouseMovedCallback);
    glfwSetScrollCallback(gDevice->window, MouseWheelCallback);
	glfwSetWindowSizeCallback(gDevice->window, WindowResizeCallback);
	glfwSetErrorCallback(ErrorCallback);

    LogInfo("GLFW successfully initialized.");

	if (GLEW_OK != glewInit()) {
		LogErr("Error initializing GLEW.");
        glfwDestroyWindow(window);
        glfwTerminate();
        return false;
	}

    LogInfo("GLEW successfully initialized.");


    GLubyte const *renderer = glGetString(GL_RENDERER);
    GLubyte const *version  = glGetString(GL_VERSION);
	LogInfo("Renderer: ", renderer);
	LogInfo("GL Version: ", version);

    v = 0;
    glGetIntegerv (GL_MAX_VERTEX_ATTRIBS, &v);
	if (v < SHADER_MAX_ATTRIBUTES) {
		LogErr("Your Graphics Card must support at least ", SHADER_MAX_ATTRIBUTES,
			" vertex attributes. It can only ", v, ".");
        glfwDestroyWindow(window);
        glfwTerminate();
        return false;
	}

	glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, &v);
	if (v < SHADER_MAX_UNIFORMS) {
		LogErr("Your Graphics Card must support at least ", 16 * SHADER_MAX_UNIFORMS,
			" vertex uniform components. It can only ", v, ".");
        glfwDestroyWindow(window);
        glfwTerminate();
        return false;
	}

    LogInfo("Maximum Vertex Uniforms: ", v);

	glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, &v);
	if (v < SHADER_MAX_UNIFORMS) {
		LogErr("Your Graphics Card must support at least ", 16 * SHADER_MAX_UNIFORMS,
			" fragment uniform components. It can only ", v, ".");
        glfwDestroyWindow(window);
        glfwTerminate();
        return false;
	}

	LogInfo("Maximum Fragment Uniforms: ", v);

    // Set GL States
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glClearColor(0.8f, 0.8f, 0.8f, 1.f);

	// Initialize EventManager
	if (!em) {
		em = new EventManager();
	}

	if (!Render::Init()) {
		LogErr("Error initializing Device's Renderer.");
		goto em_error;
	}

    // Initialize projection matrix
    UpdateProjection();

	wireframe = 0;
    engine_time = 0.0;

    // Initialize Game Scene
	if (!scene.Init(sceneInitFunc, sceneUpdateFunc, sceneRenderFunc)) {
		LogErr("Error initializing Device's Scene.");
		goto render_error;
	}

	// Initialize listeners in order
	if (!AddEventListener(LT_ResizeListener, SceneResizeEventListener, this)) {
		LogErr("Error registering scene as a resize event listener.");
		goto render_error;
	}
	if (!AddEventListener(LT_ResizeListener, DeviceResizeEventListener, this)) {
		LogErr("Error registering device as a resize event listener.");
		goto render_error;
	}

    LogInfo("Device successfully initialized.");

    return true;


render_error:
	Render::Destroy();
em_error:
	delete em;
glfw_window_error:
	glfwDestroyWindow(window);
glfw_error:
	glfwTerminate();
	return false;
}

void Device::Destroy() {
    //scene_destroy();
	if(em) delete em;
	Render::Destroy();

	D(LogInfo("Device destroyed.");)

    if(window) {
        glfwDestroyWindow(window);
        glfwTerminate();
    }
}

bool Device::AddEventListener(ListenerType type, ListenerFunc func, void *data) {
	return em->AddListener(type, func, data);
}

void Device::SetMouseX(int x) const {
    x = std::min(x, window_size.x - 1);
    x = std::max(x, 0);
    em->curr_state.mouse_pos.x = x;
    glfwSetCursorPos(window, em->curr_state.mouse_pos.x, em->curr_state.mouse_pos.y);
}

void Device::SetMouseY(int y) const {
    y = std::min(y, window_size.y - 1);
    y = std::max(y, 0);
    em->curr_state.mouse_pos.y = y;
    glfwSetCursorPos(window, em->curr_state.mouse_pos.x, em->curr_state.mouse_pos.y);
}

void Device::UpdateProjection() {
	glViewport(0, 0, window_size.x, window_size.y);
    projection_matrix_3d = mat4f::Perspective(fov, window_size[0]/(f32)window_size[1],
                                              .1f, 1000.f);
	projection_matrix_2d = mat4f::Ortho(0, (f32)window_size.x,
								        (f32)window_size.y, 0,
								        0.f, 100.f);

    for(int i = Render::Shader::_SHADER_3D_PROJECTION_START; i < Render::Shader::_SHADER_3D_PROJECTION_END; ++i) {
        Render::Shader::Bind(i);
    	Render::Shader::SendMat4(Render::Shader::UNIFORM_PROJMATRIX, projection_matrix_3d);
    }

	// Update 2d shaders
	Render::Shader::Bind(Render::Shader::SHADER_2D_UI);
	Render::Shader::SendMat4(Render::Shader::UNIFORM_PROJMATRIX, projection_matrix_2d);
}

void Device::Run() {
    f64 dt, t, last_t = glfwGetTime();

    while(!glfwWindowShouldClose(window)) {
        // Time management
        t = glfwGetTime();
        dt = t - last_t;
        last_t = t;
        engine_time += dt;

        // Keyboard inputs for Device
        if(IsKeyUp(K_Escape))
            glfwSetWindowShouldClose(window, GL_TRUE);
        if(IsKeyUp(K_F1)) {
            wireframe = !wireframe;
			/*if(wireframe) {
				glDisable(GL_CULL_FACE);
				glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
				} else {
				glEnable(GL_CULL_FACE);
				glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
				}*/
        }

		scene.Update((f32)dt);

        em->Update();


        // Render Scene
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
		scene.Render();


        //thread_sleep(10);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
}
