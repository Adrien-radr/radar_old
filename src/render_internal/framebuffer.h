#pragma once
#include "common/common.h"

namespace Render {
	namespace FBO {
		enum GBufferAttachment {
			OBJECTID,
			DEPTH,
			NORMAL,
			WORLDPOS,
			_ATTACHMENT_N
		};

		struct Desc {
			std::vector<Texture::TextureFormat> textures;	//!< Ordered texture attachments
			vec2i size;
		};

		typedef int Handle;

		Handle Build(Desc d);
		void Destroy(Handle h);
		void Bind(Handle h);
		bool Exists(Handle h);
		// void BindTexture(Handle h);

		/// Returns the OpenGL Texture ID of the given attachment of the main GBuffer (FBO0)
		u32 GetGBufferAttachment(GBufferAttachment idx);

		/// Returns the string literal name of the given attachment
		const char *GetGBufferAttachmentName(GBufferAttachment idx);

		/// General GBuffer query. Query only 1 texel value of the idx attachment.
		vec4f ReadGBuffer(GBufferAttachment idx, int x, int y);

		/// GBuffer picking : returns the object ID & Vertex ID under the given position (x,y)
		vec2i ReadVertexID(int x, int y);

		namespace _internal {
			struct Data {
				u32 framebuffer;							//!< FB handle
				u32 depthbuffer;							//!< Associated Depth Render Buffer
				std::vector<Texture::Handle> attachments;	//!< Associated textures, in attachment order
			};
		}
	};
}