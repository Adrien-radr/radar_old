
namespace Font {
    Handle Build(Desc &desc) {
        if (desc.name.empty()) {
            LogErr("Font descriptor must includea font filename.");
            return -1;
        }
        if (!Resource::CheckExtension(desc.name, "ttf")) {
            LogErr("Font file must be a Truetype .ttf file.");
            return -1;
        }

        // Check if this font resource already exist
        int free_index;
        std::stringstream resource_name;
        resource_name << desc.name << "_" << desc.size;
        bool found_resource = FindResource(renderer->font_resources, resource_name.str(), free_index);

        if (found_resource) {
            return free_index;
        }


        Font::Data font;
        font.size = vec2i(0, 0);
        font.handle = -1;

        if (!CreateAtlas(font, desc.name, desc.size)) {
            LogErr("Error while loading font '", desc.name, "'.");
            return -1;
        }

        D(LogInfo("[DEBUG] Loaded font ", desc.name, ".");)

        u32 font_i = (int)renderer->fonts.size();
        renderer->fonts.push_back(font);

        // Add created font to renderer resources
        AddResource(renderer->font_resources, free_index, resource_name.str(), font_i);

        return font_i;
    }

    void Destroy(Handle h) {
        if (Exists(h)) {
            Font::Data &font = renderer->fonts[h];
            FT_Done_Face(font.face);
            Texture::Destroy(font.handle);
            font.handle = -1;
        }
    }

    void Bind(Handle h, Texture::Target target) {
        if (Exists(h))
            Texture::Bind(renderer->fonts[h].handle, target);
    }

    bool Exists(Handle h) {
        return h >= 0 && h < (int)renderer->fonts.size() && renderer->fonts[h].handle >= 0;
    }
}