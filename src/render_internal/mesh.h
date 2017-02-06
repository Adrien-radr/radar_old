#pragma once

namespace Render
{
	// Forward decl of _Font
	namespace Font
	{
		typedef int Handle;
		namespace _internal
		{
			struct Data;
		}
	}

	namespace TextMesh
	{
		struct Desc
		{
			Desc( Font::Handle font, const std::string &str, const vec4f &strcolor ) :
				font_handle( font ), string( str ), color( strcolor )
			{}

			Font::Handle font_handle;
			std::string string;
			vec4f color;
		};

		/// TextMesh Handle.
		/// TextMeshes are stored and worked on internally by the renderer.
		/// Outside of render.c, TextMeshes are referred by those handles
		typedef int Handle;

		// Creates the TextMesh with the given descriptor
		Handle Build( Desc &desc );

		/// Deallocate GL data for the given textmesh handle
		void Destroy( Handle h );

		/// Binds the given textmesh's VBO as current
		void Bind( Handle h );

		/// Render the given textmesh. Binds it if not currently bound
		void Render( Handle h );

		/// Returns true if the given mesh exists in renderer
		bool Exists( Handle h );

		/// Change the string displayed by a TextMesh
		/// @param h : handle of the text mesh to be modified. -1 for a new handle
		/// @param mesh : Handle of the used font
		/// @param str : new string to be displayed
		/// @return : h if it is >= 0, or a new handle if h is -1
		Handle SetString( Handle h, Font::Handle fh, const std::string &str );

		namespace _internal
		{
			/// Dynamic mesh used to display text.
			/// VBOs updated every time its string text is changed.
			struct Data
			{
				Data() : vbo( 0 ), vertices_n( 0 ), curr_letter_n( 0 ), font( NULL ) {}
				u32 vbo;            //!< Contains position data and texcoord data
				u32 vertices_n;     //!< Number of vertices the mesh has (string_len*6)

				/// TextMesh data is initialized in steps
				/// Depending on the displayed string length, data will be alloc'd either :
				/// 16, 32, 128, 512, 1024, or 2048 letters. (1 letter is 4*6*sizeof(f32)=96bytes)
				/// data will be realloc'd if the text is updated with a string > current letter
				/// capacity. If updated with number of letters < current, data size is not changed
				std::vector<f32> data;
				u32 curr_letter_n;      //!< data size is curr_letter_n*6*4*sizeof(f32) bytes

				Font::_internal::Data *font; //!< Font style (must be loaded in renderer as a resource)
			};

			/// Modify the given TextMesh data to contains the given string with the given font
			bool CreateFromString( Data &mesh, const Font::_internal::Data &font, const std::string &str );
		}
	}

	namespace Mesh
	{
		enum Attribute
		{
			MESH_POSITIONS = 1,
			MESH_NORMALS = 2,
			MESH_TEXCOORDS = 4,
			MESH_TANGENT = 8,
			MESH_BINORMAL = 16,
			MESH_COLORS = 32,
			MESH_INDICES = 64,
			MESH_ADD1 = 128
		};

		enum AnimType
		{
			ANIM_NONE,      // static mesh, no bone animation
			ANIM_IDLE,

			ANIM_N // Do not Use
		};

		struct AnimState
		{
			int curr_frame;
			f32 curr_time;
			f32 duration;

			AnimType type;
		};


		/// Mesh Description
		/// param vertices_n : number of vertices the mesh has.    
		/// @param positions : array of vertex positions.          
		/// @param normals : array of vertex normals.
		/// @param texcoords : array of vertex texture UV coords
		/// @param colors : array of vertex colors.
		struct Desc
		{
			Desc( const std::string &resource_name, bool empty_mesh, u32 icount, u32 *idx_arr,
				u32 vcount, f32 *pos_arr, f32 *normal_arr = nullptr, f32 *texcoord_arr = nullptr,
				f32 *tangent_arr = nullptr, f32 *bitangent_arr = nullptr, f32 *col_arr = nullptr ) :
				name( resource_name ), empty_mesh( empty_mesh ), vertices_n( vcount ), indices_n( icount ),
				indices( idx_arr ), positions( pos_arr ), normals( normal_arr ), texcoords( texcoord_arr ),
				tangents( tangent_arr ), bitangents( bitangent_arr ), colors( col_arr ), additional(nullptr),
				additional_elt(0), additional_fmt(0), additional_n(1)
			{}

			void SetAdditionalData(f32 *arr, u32 format, int elements, int instances);

			std::string name;	//!< name of the mesh for resource managment
			bool empty_mesh;

			u32 vertices_n;		//!< number of vertices the mesh has
			u32 indices_n;		//!< number of indices

			u32 *indices;

			f32 *positions;     //!< format vec3, attrib0
			f32 *normals;       //!< format vec3, attrib1
			f32 *texcoords;     //!< format vec2, attrib2
			f32 *tangents;		//!< format vec3, attrib3
			f32 *bitangents;	//!< format vec3, attrib4
			f32 *colors;        //!< format vec4, attrib5

			// additional data
			f32 *additional;	//!< custom format, attrib6
			int additional_elt;	//!< nb of elements for above
			u32 additional_fmt;	//!< format type for above
			int additional_n;   //!< nb of instances for instancing 
		};

		/// Mesh Handle.
		/// Meshes are stored and worked on internally by the renderer.
		/// Outside of render.c, Meshes are referred by those handles
		typedef int Handle;

		/// Build a Mesh from the given Mesh Description. See above to understand what is needed.
		/// @return : the Mesh Handle if creation successful. -1 if error occured.
		Handle Build( const Desc &desc );

		/// Build a radius 1 sphere
		Handle BuildSphere();

		/// Build a radius 1 box
		Handle BuildBox();

		/// Build a SH visualization mesh
		/// @param shNormalization : put to true if the sh visualization should be normalized by DC
		Handle BuildSHVisualization( const float *shCoeffs, const u32 bandN, const std::string &meshName, bool shNormalization, const u32 numPhi = 48, const u32 numTheta = 96 );

		/// Deallocate GL data for the given mesh handle
		void Destroy( Handle h );

		/// Bind given Mesh as current VAO for drawing
		/// If -1 is given, unbind all VAOs
		void Bind( Handle h );

		/// Returns true if the given mesh exists in renderer
		bool Exists( Handle h );
		bool Exists( const std::string &resourceName, Handle &h ); // returns the resource in h if it exists as a resource

		/// Renders the given mesh, binding it if not currently bound as GL Current VAO.
		/// The given animation state is used to transmit bone-matrix data to the shader
		/// before drawing the mesh. If NULL, an identity bonematrix is used
		void Render( Handle h );

		void RenderInstanced( Handle h );

		/// Sets the current played animation of state. Reset it at the beginning of 1st frame
		// void SetAnimation(Handle h, AnimState &state, AnimType type);

		/// Update the given animation state so that it continues playing its current animation
		/// with the added delta time, thus updating its bone matrices
		/// @param state : the animation to update
		/// @param mesh_i : the mesh resource handle from which the animation is taken
		/// @param dt : delta time to advance animation
		// void UpdateAnimation(Handle h, AnimState &state, float dt);

#if 0
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
		struct _Animation
		{
			std::vector<f32>	frame_duration;    //!< Duration of each frame
			f32					duration;
			int					frame_n;
			bool				used;
		};
#endif

		namespace _internal
		{
			struct Data
			{
				Data() : vao( 0 ), vertices_n( 0 ), indices_n( 0 ), instances_n( 1 ),
						 attrib_flags( MESH_POSITIONS ), center( 0 ), radius( 0 )
				{
					vbo[0] = vbo[1] = vbo[2] = vbo[3] = vbo[4] = vbo[5] = vbo[6] = 0;
					ibo = 0;
				}

				// Model
				u32 vao;                	//!< GL VAO ID
				u32 vbo[7];             	//!< 0: positions, 1: normals, 2: texcoords,
											//!< 3: tangent, 4: binormal, 5: colors
											//!< 6: additional_1
				u32 ibo;					//!< Element buffer

				u32			vertices_n;     //!< Number of vertices the mesh has
				u32			indices_n;		//!< Number of indices it has
				u32 		instances_n;	//!< Number of instances for additional data
				int 		attrib_flags; 	//!< OR'ed enum defining which vertex attribs it has

				vec3f		center;			//!< Mesh center of mass (from all vertices)
				float		radius;			//!< Bounding sphere radius

				// Animations
				//u32         animation_n;        //!< Number of loaded animations
				//_Animation  animations[ANIM_N]; //!< All animations for this mesh. Some might not be
				//!< available. Test animations[i].used.
			};
		}

	}
}
