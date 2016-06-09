#pragma once

#include "common/common.h"

/// Enumeration of the different mouse buttons
enum MouseButton {
    MB_Left,
    MB_Right,   
    MB_Middle,   
    MB_4, 
    MB_5,
    MB_6,
    MB_7,
    MB_ENDFLAG
};

/// Enumeration of the different Keyboard keys
enum Key {
    K_Space		= 32,
    K_Apostrophe= 39,
    K_Comma     = 44,
    K_Minus         ,
    K_Period        ,
    K_Slash         ,
    K_Num0	    = 48,
    K_Num1		 	,
    K_Num2		 	,
    K_Num3		 	,
    K_Num4		 	,
    K_Num5		 	,
    K_Num6		 	,
    K_Num7		 	,
    K_Num8		 	,
    K_Num9			,
    K_SemiColon = 59,
    K_Equal     = 61,
    K_A			= 65,
    K_B				,	
    K_C				,
    K_D				,
    K_E				,
    K_F				,
    K_G				,
    K_H				,
    K_I				,
    K_J				,
    K_K				,
    K_L				,
    K_M				,
    K_N				,
    K_O				,
    K_P				,
    K_Q				,
    K_R				,
    K_S				,
    K_T				,
    K_U				,
    K_V				,
    K_W				,
    K_X				,
    K_Y				,
    K_Z				,
    K_LeftBracket   ,
    K_BackSlash     ,
    K_RightBracket  ,
    K_GraveAccent   ,



    K_Escape     = 256 	,
    K_Enter		 	,
    K_Tab			 	,
    K_BackSpace	 	,
    K_Insert		 	,
    K_Delete		 	,
    K_Right		 	,
    K_Left		 	,
    K_Down		 	,
    K_Up			 	,
    K_PageUp		 	,
    K_PageDown	 	,
    K_Home		 	,
    K_End			 	,
    K_CapsLock          ,
    K_ScrollLock        ,
    K_NumLock           ,
    K_PrintScreen       ,
    K_Pause             ,
    K_F1	     = 290  ,
    K_F2				,
    K_F3				,
    K_F4				,	
    K_F5				,
    K_F6				,
    K_F7				,	
    K_F8				,		
    K_F9				, 
    K_F10			 	,
    K_F11			 	,
    K_F12			 	,
    K_F13			 	,
    K_F14			 	,
    K_F15			 	,
    K_F16			 	,
    K_F17			 	,
    K_F18			 	,
    K_F19			 	,
    K_F20			 	,
    K_F21			 	,
    K_F22			 	,
    K_F23			 	,
    K_F24			 	,
    K_F25			 	,





    K_N0        = 320   ,
    K_N1		 	    ,
    K_N2		 	    ,
    K_N3		 	    ,
    K_N4		 	    ,
    K_N5		 	    ,
    K_N6		 	    ,
    K_N7		 	    ,
    K_N8		 	    ,
    K_N9		 	    ,
    K_NDecimal		    ,
    K_NDivide		    ,
    K_NMultiply	 	    ,
    K_NSubtract	 	    ,
    K_NAdd			    ,
    K_NEnter		    ,
    K_NEqual		    ,
    K_LShift	= 340   ,
    K_LControl	 	    ,
    K_LAlt		 	    ,
    K_LSuper            ,
    K_RShift		    ,
    K_RControl	 	    ,
    K_RAlt		 	    ,
    K_RSuper            ,

    K_Menu              ,
    K_ENDFLAG = K_Menu  ,
};


/// Types of event that can occur
enum EventType{
    EMouseMoved,
    EMousePressed,
    EMouseReleased,
    EMouseWheelMoved,
    EKeyPressed,
    EKeyReleased,
    ECharPressed,

    EWindowResized
};

/// Event object binding a Type of Event to the value
/// changed by the event.
/// This is used primalarly by the GLFW Callback funcs
/// to distribute the event recorded to all Listeners
struct Event {
    EventType type;				///< Type of the event

	vec2i v;					///< Mouse pos
    int i;						///< Mouse wheel relative

    union {
        MouseButton button;     ///< Mouse button used
        Key         key;        ///< Keyboard key used
    };
};


/// Initialize the Eventmanager and its states
//bool EventManager_init();

/// Free the EventManager
//void EventManager_destroy();

/// Update the input states of the EventManager and propagate all recorded events
/// to all registered listeners
/// Call this every frame
//void EventManager_update();

// Listeners
// To create a event listener, one must register it to the Eventmanager
// You can send a void* when registering. It will be available everytime the callback is used
// Exemple : if pData is a pointer on Camera, one could do this in his listener func :
//         ((Camera*)pData).mPosition.x = pEvent.MousePos.x;
//         ((Camera*)pData).mPosition.y = pEvent.MousePos.y;

/// Listener function type
/// @param event : Event recorded that can be processed
/// @param data : Void pointer on anything that could be usefull in the callback
typedef void (*ListenerFunc)(const Event &event, void *data );

/// Different types of listener
enum ListenerType {
    LT_KeyListener,
    LT_MouseListener,
    LT_ResizeListener
};

