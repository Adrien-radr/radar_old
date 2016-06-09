#pragma once

#include "../render.h"     // for common.h and MeshAttributes


namespace Render {
	// Forward decl of _Font
	namespace Font {
		struct Data;
	}

	namespace TextMesh {
		/// Dynamic mesh used to display text.
		/// VBOs updated every time its string text is changed.
		struct Data {
			Data() : vbo(0), vertices_n(0), curr_letter_n(0), font(NULL) {}
			u32 vbo;            //!< Contains position data and texcoord data
			u32 vertices_n;     //!< Number of vertices the mesh has (string_len*6)

			/// TextMesh data is initialized in steps
			/// Depending on the displayed string length, data will be alloc'd either :
			/// 16, 32, 128, 512, 1024, or 2048 letters. (1 letter is 4*6*sizeof(f32)=96bytes)
			/// data will be realloc'd if the text is updated with a string > current letter
			/// capacity. If updated with number of letters < current, data size is not changed
			std::vector<f32> data;
			u32 curr_letter_n;      //!< data size is curr_letter_n*6*4*sizeof(f32) bytes

			Font::Data *font; //!< Font style (must be loaded in renderer as a resource)
		};

		/// Modify the given TextMesh data to contains the given string with the given font
		bool CreateFromString(Data &mesh, const Font::Data &font, const std::string &str);

	}
	namespace Mesh {
		/// Define a Position or Scale Key for a keyframe animation
		//struct PosKey {
			//vec3 xyz;
			//f32  time;
		//};


		/// Define a Rotation Key for a keyframe animation
		//struct quat_key{
			//quaternion quat;
			//f32        time;
		//};

		/// Sequence of position & rotation keys defining the anim of 1 bone
		//struct _AnimSequence {
		//vec3_keyArray pos_keys;
		//quat_keyArray rot_keys;
		//};

		/// Describe a Animated Joint Node of an armature. Might or not be weighted to a bone.
		//struct _AnimNode {
		//int bone_index;
		//bool has_bone;
		//_AnimSequence animations[ANIM_N];

		//struct _AnimNode *children; //!< Always initialized to MESH_MAX_BONES
		//u32 children_n;
		//};

		/// Maps an _AnimNode to its string name. Used during animation loading.
		//struct _AnimNodeName {
		//str64       name;
		//_AnimNode   *node;
		//};

		/// Header information about an armature animation
		struct _Animation {
			std::vector<f32>	frame_duration;    //!< Duration of each frame
			f32					duration;
			int					frame_n;
			bool				used;
		};


		/// Can use animation or not.
		struct Data {
			Data() : vao(0), vertices_n(0), attrib_flags(MESH_POSITIONS), animation_n(0) {
				vbo[0] = vbo[1] = vbo[2] = vbo[3] = 0;
			}

			// Model
			u32 vao;                //!< GL VAO ID
			u32 vbo[4];             //!< 0: positions, 1: normals, 2: texcoords,
			//!< 3: colors

			u32			vertices_n;      //!< Number of vertices the mesh has
			int			attrib_flags;	 //!< OR'ed enum defining which vertex attribs it has

			// Animations
			u32         animation_n;        //!< Number of loaded animations
			_Animation  animations[ANIM_N]; //!< All animations for this mesh. Some might not be
			//!< available. Test animations[i].used.
		};


	}
}
