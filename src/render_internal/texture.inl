
namespace Texture {
    Handle Build(const Desc &desc) {
        // Check if this font resource already exist
        int free_index;
        bool found_resource = FindResource(renderer->texture_resources, desc.name, free_index);

        if (found_resource) {
            return free_index;
        }


        Texture::Data texture;

        if (desc.from_file) {
            if (!Load(texture, desc.name)) {
                LogErr("Error while loading '", desc.name, "' image.");
                return -1;
            }
        }
        else {
            glGenTextures(1, &texture.id);
            glBindTexture(GL_TEXTURE_2D, texture.id);
            texture.size = desc.size;
        }


        int tex_i = (int)renderer->textures.size();
        renderer->textures.push_back(texture);

        // Add texture to render resources
        AddResource(renderer->texture_resources, free_index, desc.name, tex_i);

        return tex_i;
    }

    void Destroy(Handle h) {
        if (h >= 0 && h < (int)renderer->textures.size()) {
            Texture::Data &tex = renderer->textures[h];
            glDeleteTextures(1, &tex.id);
            tex.id = 0;
        }
    }

    void Bind(Handle h, Target target) {
        GLint tex = Exists(h) ? h : -1;

        // Switch Texture Target if needed
        if (target != renderer->curr_GL_texture_target) {
            renderer->curr_GL_texture_target = target;
            glActiveTexture(GL_TEXTURE0 + target);
        }

        // Switch active Texture if needed
        if (renderer->curr_GL_texture[target] != tex) {
            renderer->curr_GL_texture[target] = tex;
            glBindTexture(GL_TEXTURE_2D, tex >= 0 ? (int)renderer->textures[tex].id : -1);
        }
    }

    bool Exists(Handle h) {
        return h >= 0 && h < (int)renderer->textures.size() && renderer->textures[h].id > 0;
    }

    u32 GetGLID(Handle h) {
        if(Exists(h)) {
            return renderer->textures[h].id;
        }
        return 0;
    }
}


namespace FBO {
    GLuint fbo_attachments[5] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, 
                                  GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4 };

    Handle Build(Desc d) {
        Data fbo;
        glGenFramebuffers(1, &fbo.framebuffer); 
        glBindFramebuffer(GL_FRAMEBUFFER, fbo.framebuffer);

        // Create all attachments
        int numAttachments = d.textures.size();
        for(int i = 0; i < numAttachments; ++i) {
            Texture::Desc td;
            td.size = d.size;
            td.from_file = false;
            Texture::Handle th = Texture::Build(td);
            if(th < 0) {
                LogErr("Error creating FBO's associated texture.");
                return -1;
            }
            GLuint tex_id = renderer->textures[th].id;

            Texture::FormatDesc fdesc = Texture::GetTextureFormat(d.textures[i]);
            Texture::Bind(th, Texture::TARGET0);
            glTexImage2D(GL_TEXTURE_2D, 0, fdesc.formatGL, d.size.x, d.size.y, 0, fdesc.formatInternalGL, fdesc.type, NULL);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, tex_id, 0);
            fbo.attachments.push_back(th);
        }     

        glDrawBuffers(numAttachments, fbo_attachments);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        int fbo_i = (int)renderer->fbos.size();
        renderer->fbos.push_back(fbo);

        return fbo_i;
    }

    void Destroy(Handle h) {
        if(Exists(h)) {
            FBO::Data &f = renderer->fbos[h];
            for(Texture::Handle &h : f.attachments)
                Texture::Destroy(h);
            if(f.framebuffer > 0) {
                glDeleteFramebuffers(1, &f.framebuffer);
                f.framebuffer = 0;
            }
        }
    }

    bool Exists(Handle h) {
        return h >= 0 && h < (int)renderer->fbos.size();
    }

    void Bind(Handle h) {
        if(Exists(h)) {
            glBindFramebuffer(GL_FRAMEBUFFER, renderer->fbos[h].framebuffer);
        }    
    }

	u32 GetGBufferAttachment(GBufferAttachment idx) {
        if(idx < _ATTACHMENT_N) {
            // GBuffer is FBO 0
            Data &fbo = renderer->fbos[0];

            if((int)idx <= fbo.attachments.size()) {
                return Texture::GetGLID(fbo.attachments[idx]);
            }
        }
        return 0;
    }

    /*void BindTexture() {
        Texture::Bind(texture);
    }*/
}