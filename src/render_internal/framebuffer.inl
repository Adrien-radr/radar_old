namespace Render {
	namespace FBO {
		GLuint fbo_attachments[5] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2,
			GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4 };

		Handle Build(Desc d) {
			_internal::Data fbo;
			glGenFramebuffers(1, &fbo.framebuffer);
			glBindFramebuffer(GL_FRAMEBUFFER, fbo.framebuffer);

			// Create all attachments
			size_t numAttachments = d.textures.size();
			for (size_t i = 0; i < numAttachments; ++i) {
				Texture::Desc td;
				td.size = d.size;
				td.type = Texture::Empty;
				Texture::Handle th = Texture::Build(td);
				if (th < 0) {
					LogErr("Error creating FBO's associated texture.");
					return -1;
				}
				GLuint tex_id = renderer->textures[th].id;

				Texture::FormatDesc fdesc = Texture::GetTextureFormat(d.textures[i]);
				Texture::Bind(th, Texture::TARGET0);
				glTexImage2D(GL_TEXTURE_2D, 0, fdesc.formatInternalGL, d.size.x, d.size.y, 0, fdesc.formatGL, fdesc.type, NULL);

				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				glFramebufferTexture2D(GL_FRAMEBUFFER, (GLenum) (GL_COLOR_ATTACHMENT0 + i), GL_TEXTURE_2D, tex_id, 0);
				fbo.attachments.push_back(th);
			}

			glDrawBuffers((GLsizei) numAttachments, fbo_attachments);

			// Attach Depth Buffer
			glGenRenderbuffers(1, &fbo.depthbuffer);
			glBindRenderbuffer(GL_RENDERBUFFER, fbo.depthbuffer);
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, d.size.x, d.size.y);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, fbo.depthbuffer);

			// Check if everything is good
			if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
				LogErr("Framebuffer not complete!");
				glBindFramebuffer(GL_FRAMEBUFFER, 0);
				return -1;
			}

			glBindFramebuffer(GL_FRAMEBUFFER, 0);

			int fbo_i = (int) renderer->fbos.size();
			renderer->fbos.push_back(fbo);

			return fbo_i;
		}

		void Destroy(Handle h) {
			if (Exists(h)) {
				_internal::Data &f = renderer->fbos[h];
				for (Texture::Handle &h : f.attachments)
					Texture::Destroy(h);
				if (f.framebuffer > 0) {
					glDeleteFramebuffers(1, &f.framebuffer);
					f.framebuffer = 0;
				}
				if (f.depthbuffer > 0) {
					glDeleteRenderbuffers(1, &f.depthbuffer);
					f.depthbuffer = 0;
				}
			}
		}

		bool Exists(Handle h) {
			return h >= 0 && h < (int) renderer->fbos.size();
		}

		void Bind(Handle h) {
			if (Exists(h)) {
				glBindFramebuffer(GL_FRAMEBUFFER, renderer->fbos[h].framebuffer);
			}
		}

		u32 GetGBufferAttachment(GBufferAttachment idx) {
			if (idx < _ATTACHMENT_N) {
				// GBuffer is FBO 0
				_internal::Data &fbo = renderer->fbos[0];

				if ((int) idx <= fbo.attachments.size()) {
					return Texture::GetGLID(fbo.attachments[idx]);
				}
			}
			return 0;
		}

		static const char *GBufferAttachmentNames[_ATTACHMENT_N + 1] = {
			"Object ID",
			"Depth",
			"Normal",
			"World Position"
		};

		const char *GetGBufferAttachmentName(GBufferAttachment idx) {
			if (idx < _ATTACHMENT_N) {
				return GBufferAttachmentNames[idx];
			}

			return nullptr; // should never happen if you're not idiot
		}

		vec2i ReadVertexID(int x, int y) {
			vec2i ret(-1, -1);

			vec4f fbpx = ReadGBuffer(GBufferAttachment::OBJECTID, x, y);

			// data : 
			// x - ObjectID
			// y - PrimitiveID
			// z - Depth
			if (fbpx.z > 0) {
				ret.x = (int) fbpx.x;
				ret.y = (int) fbpx.y;
			}

			return ret;
		}

		vec4f ReadGBuffer(GBufferAttachment idx, int x, int y) {
			Device &d = GetDevice();
			const vec2i &ws = d.windowSize;
			if (x >= ws.x || y >= ws.y)
				return vec4f(-1.f);

			_internal::Data &fbo = renderer->fbos[0]; // GBuffer is always fbo 0
			glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo.framebuffer);
			glReadBuffer((GLenum) GL_COLOR_ATTACHMENT0 + idx);

			vec4f data;
			glReadPixels(x, ws.y - y, 1, 1, GL_RGBA, GL_FLOAT, (void*) &data);

			glReadBuffer(GL_NONE);
			glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

			return data;
		}
	}
}