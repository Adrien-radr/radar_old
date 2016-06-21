
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

}