
namespace Mesh {
    Handle Build(const Desc &desc) {
        f32 *vp = nullptr, *vn = nullptr, *vt = nullptr, *vc = nullptr;
        u32 *idx = nullptr;

        int free_index;
        bool found_resource = FindResource(renderer->mesh_resources, desc.name, free_index);

        if (found_resource) {
            return free_index;
        }


        Mesh::Data mesh;

        if(!desc.empty_mesh) {
            if (desc.indices_n > 0 && desc.indices && desc.vertices_n > 0 && desc.positions) {
                vp = (f32*)desc.positions;
                idx = (u32*)desc.indices;

                if (desc.normals)
                    vn = (f32*)desc.normals;
                if (desc.texcoords)
                    vt = (f32*)desc.texcoords;
                if (desc.colors)
                    vc = (f32*)desc.colors;
                // Create empty mesh (i.e. only a vao, used to create the text vao for example)
            }
            else {
                // TODO : Here we allow mesh creation without data
                // It is used for TextMeshes VAO, so for now, no error
                LogErr("Tried to create a Mesh without giving indices or positions array.");
                return -1;
            }
        }


        mesh.vertices_n = desc.vertices_n;
        mesh.indices_n = desc.indices_n;

        glGenVertexArrays(1, &mesh.vao);
        // Disallow 0-Vao. If given VAO with index 0, ask for another one
        // this should never happen because VAO-0 is already constructed for the text
        if (!mesh.vao) glGenVertexArrays(1, &mesh.vao);
        glBindVertexArray(mesh.vao);

        if (vp) {
            mesh.attrib_flags = MESH_POSITIONS;
            glEnableVertexAttribArray(0);

            // fill position buffer
            glGenBuffers(1, &mesh.vbo[0]);
            glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo[0]);
            glBufferData(GL_ARRAY_BUFFER, mesh.vertices_n * sizeof(vec3f),
                            vp, GL_STATIC_DRAW);

            // add it to the vao
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (GLubyte*)NULL);
        }

        // Check for indices
        if(idx) {
            mesh.attrib_flags |= MESH_INDICES;
            glGenBuffers(1, &mesh.ibo);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ibo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.indices_n * sizeof(u32),
                            idx, GL_STATIC_DRAW);
        }

        // check for normals
        if (vn) {
            mesh.attrib_flags |= MESH_NORMALS;
            glEnableVertexAttribArray(1);

            // fill normal buffer
            glGenBuffers(1, &mesh.vbo[1]);
            glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo[1]);
            glBufferData(GL_ARRAY_BUFFER, mesh.vertices_n * sizeof(vec3f),
                            vn, GL_STATIC_DRAW);

            // add it to vao
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (GLubyte*)NULL);
        }

        // check for texcoords
        if (vt) {
            mesh.attrib_flags |= MESH_TEXCOORDS;
            glEnableVertexAttribArray(2);

            // fill normal buffer
            glGenBuffers(1, &mesh.vbo[2]);
            glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo[2]);
            glBufferData(GL_ARRAY_BUFFER, mesh.vertices_n * sizeof(vec2f),
                            vt, GL_STATIC_DRAW);

            // add it to vao
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, (GLubyte*)NULL);
        }

        // check for colors
        if (vc) {
            mesh.attrib_flags |= MESH_COLORS;
            glEnableVertexAttribArray(3);

            // fill normal buffer
            glGenBuffers(1, &mesh.vbo[3]);
            glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo[3]);
            glBufferData(GL_ARRAY_BUFFER, mesh.vertices_n * sizeof(vec4f),
                            vc, GL_STATIC_DRAW);

            // add it to vao
            glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, 0, (GLubyte*)NULL);
        }

        glBindVertexArray(0);

        int mesh_i = (int)renderer->meshes.size();
        renderer->meshes.push_back(mesh);

        // Add created mesh to renderer resources
        AddResource(renderer->mesh_resources, free_index, desc.name, mesh_i);

        return mesh_i;
    }
    
    void Destroy(Handle h) {
        if (Exists(h)) {
            Data &mesh = renderer->meshes[h];
            glDeleteBuffers(4, mesh.vbo);
            glDeleteBuffers(1, &mesh.ibo);
            glDeleteVertexArrays(1, &mesh.vao);
            mesh.attrib_flags = MESH_POSITIONS;
            mesh.vertices_n = 0;
            mesh.indices_n = 0;


            // Remove it as a loaded resource
            for (u32 i = 0; i < renderer->mesh_resources.size(); ++i)
                if ((int)renderer->mesh_resources[i].handle == h) {
                    renderer->mesh_resources[i].handle = -1;
                    renderer->mesh_resources[i].name = "";
                }


            mesh.animation_n = 0;
        }
    }

    void Bind(Handle h) {
        GLint vao = Exists(h) ? h : -1;

        if (renderer->curr_GL_vao != vao) {
            renderer->curr_GL_vao = vao;
            glBindVertexArray(vao >= 0 ? (int)renderer->meshes[vao].vao : -1);
        }
    }

    void Render(Handle h) {
        if (Exists(h)) {
            const Mesh::Data &md = renderer->meshes[h];

            Bind(h);
            glDrawElements(GL_TRIANGLES, md.indices_n, GL_UNSIGNED_INT, 0);
        }
            //if (state && state.type > ANIM_NONE) {
            //}
            //else {
            // identity bone matrix for meshes having no mesh animation
            //mat4 identity;
            //mat4_identity(identity);
            //shader_send_mat4(UNIFORM_BONEMATRIX0, identity);
            //}
    }


    bool Exists(Handle h) {
        return h >= 0 && h < (int)renderer->meshes.size() && renderer->meshes[h].vao > 0;
    }
    
    bool Exists(const std::string &resourceName, Handle &res) {
        return FindResource(renderer->mesh_resources, resourceName, res, false);
    }
/*
    void SetAnimation(Handle h, AnimState &state, AnimType type) {
        if (Exists(h)) {
            state.curr_frame = 0;
            state.curr_time = 0.f;
            state.type = type;

            if (type > ANIM_NONE) {
                state.duration = renderer->meshes[h].animations[type - 1].duration;
                UpdateAnimation(h, state, 0.f);
            }
        }
    }

    void UpdateAnimation(Handle h, AnimState &state, float dt) {
        mat4f root_mat;
        root_mat.Identity();
        root_mat = root_mat.RotateX(-M_PI_OVER_TWO);	// rotate root by 90X for left-handedness

        if (Exists(h)) {
            const _Animation &anim = renderer->meshes[h].animations[state.type - 1];
            state.curr_time += dt;

            if (anim.used && state.curr_time > anim.frame_duration[state.curr_frame + 1]) {
                ++state.curr_frame;
                if (state.curr_frame == anim.frame_n - 1) {
                    state.curr_frame = 0;
                    state.curr_time = 0.f;
                }
            }
        }
    }
*/
}



namespace TextMesh {
    Handle Build(Desc &desc) {
        // Create a new textmesh with the
        Handle tmesh_handle = SetString(-1, desc.font_handle, desc.string);
        return tmesh_handle;
    }

    Handle SetString(Handle h, Font::Handle fh, const std::string &str) {
        if (!Font::Exists(fh)) {
            LogErr("Font handle ", fh, " is not a valid renderer resource.");
            return false;
        }

        bool updated = true;        // true if this is an updated of an existing text
        TextMesh::Data *dst;// = renderer->textmeshes[0];	// temporary reference

        if (h >= 0) {
            Assert(h < (int)renderer->textmeshes.size());
            dst = &renderer->textmeshes[h];
        }
        else {
            updated = false;
            renderer->textmeshes.push_back(TextMesh::Data());
            dst = &renderer->textmeshes.back();
            *dst = TextMesh::Data();
        }

        if (!CreateFromString(*dst, renderer->fonts[fh], str)) {
            LogErr("Error creating TextMesh from given string.");
            return -1;
        }

        return updated ? h : (Handle)(renderer->textmeshes.size()-1);
    }

    bool Exists(Handle h) {
        return h >= 0 && h < (int)renderer->textmeshes.size() && renderer->textmeshes[h].vbo > 0;
    }

    void Destroy(Handle h) {
        if (Exists(h)) {
            Data &textmesh = renderer->textmeshes[h];
            glDeleteBuffers(1, &textmesh.vbo);
            textmesh.vbo = 0;
        }
    }

    void Bind(Handle h) {
        if (Exists(h)) {
            Data &textmesh = renderer->textmeshes[h];

            glBindBuffer(GL_ARRAY_BUFFER, textmesh.vbo);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (GLvoid*)NULL);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0,
                (GLvoid*)(textmesh.vertices_n * 2 * sizeof(f32)));
            glEnableVertexAttribArray(0);   // enable positions
            glDisableVertexAttribArray(1);  // no normals
            glEnableVertexAttribArray(2);   // enable texcoords
        }
    }

    void Render(Handle h) {
        Bind(h);
        glDrawArrays(GL_TRIANGLES, 0, renderer->textmeshes[h].vertices_n);
    }
}

namespace SpriteSheet {
    Handle LoadFromFile(const std::string &filename) {
        Json ss_json;
        if (!ss_json.Open(filename)) {
            LogErr("Error loading spritesheet json file.");
            return -1;
        }

        SpriteSheet::Data ss;

        ss.file_name = filename;
        ss.resource_name = Json::ReadString(ss_json.root, "name", "");
        std::string tex_file = Json::ReadString(ss_json.root, "texture", "");

        // try to find the resource
        int free_index;
        bool found_resource = FindResource(renderer->spritesheets_resources,
            ss.resource_name, free_index);

        if (found_resource) {
            return free_index;
        }

        // try to load the texture resource or get it if it exists
        Texture::Desc tdesc(tex_file);
        ss.sheet_texture = Texture::Build(tdesc);

        if (ss.sheet_texture == -1) {
            LogErr("Error loading texture file during spritesheet creation.");
            return -1;
        }

        cJSON *sprite_arr = cJSON_GetObjectItem(ss_json.root, "sprites");
        if (sprite_arr) {
            int sprite_count = cJSON_GetArraySize(sprite_arr);
            for (int i = 0; i < sprite_count; ++i) {
                Sprite sprite;
                cJSON *sprite_root = cJSON_GetArrayItem(sprite_arr, i);
                if (cJSON_GetArraySize(sprite_root) != 5) {
                    LogErr("A Sprite definition in a spritesheet must have 5 fields : x y w h name.");
                    continue;
                }
                sprite.geometry.x = cJSON_GetArrayItem(sprite_root, 0)->valueint;
                sprite.geometry.y = cJSON_GetArrayItem(sprite_root, 1)->valueint;
                sprite.geometry.w = cJSON_GetArrayItem(sprite_root, 2)->valueint;
                sprite.geometry.h = cJSON_GetArrayItem(sprite_root, 3)->valueint;
                sprite.name = cJSON_GetArrayItem(sprite_root, 4)->valuestring;

                ss.sprites.push_back(sprite);
            }
        }

        Handle ss_i = renderer->spritesheets.size();
        renderer->spritesheets.push_back(ss);

        AddResource(renderer->spritesheets_resources, free_index, ss.resource_name, ss_i);

        return ss_i;
    }

    void Destroy(Handle h) {
        if (h >= 0 && h < (int)renderer->spritesheets.size()) {
            Texture::Destroy(renderer->spritesheets[h].sheet_texture);
            renderer->spritesheets[h].sprites.clear();
        }
    }
}