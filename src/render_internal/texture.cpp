#include "texture.h"
#include "../common/resource.h"
#include "GL/glew.h"

// PNG loading (header in ext/ directory)
#include <png.h>




namespace Render {
	namespace Texture {
		static void get_PNG_info(int color_type, _tex *ti) {
			switch (color_type) {
			case PNG_COLOR_TYPE_GRAY:
				ti->fmt = GL_RED;
				ti->int_fmt = GL_RED;
				break;
			case PNG_COLOR_TYPE_GRAY_ALPHA:
				ti->fmt = GL_RG;
				ti->int_fmt = GL_RG;
				break;
			case PNG_COLOR_TYPE_RGB:
				ti->fmt = GL_RGB;
				ti->int_fmt = GL_RGB;
				break;
			case PNG_COLOR_TYPE_RGB_ALPHA:
				ti->fmt = GL_RGBA;
				ti->int_fmt = GL_RGBA;
				break;
			default:
				LogErr("Invalid color type!");
				break;
			}
		}

		_tex *LoadPNG(const std::string &filename) {
			_tex *t = NULL;
			FILE *f = NULL;

			png_byte    sig[8];
			png_structp img = NULL;
			png_infop   img_info = NULL;

			int bpp, color_type;
			png_bytep *row_pointers = NULL;
			png_uint_32 w, h;


			f = fopen(filename.c_str(), "rb");
			if (!f) {
				LogErr("Could not open '", filename, "'.");
				return NULL;
			}

			// get png file signature
			fread(sig, 1, 8, f);
			if (!png_sig_cmp(sig, 0, 8) == 0) {
				LogErr("'", filename, "' is not a valid PNG image.");
				fclose(f);
				return NULL;
			}



			img = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
			if (!img) {
				LogErr("Error while creating PNG read struct.");
				fclose(f);
				return NULL;
			}

			img_info = png_create_info_struct(img);
			if (!img_info) {
				LogErr("Error while creating PNG info struct.");
				if (img) png_destroy_read_struct(&img, &img_info, NULL);
				fclose(f);
				return NULL;
			}

			// our texture handle
			t = new _tex();//

            int rowbytes;
			// setjmp for any png loading error
			if (setjmp(png_jmpbuf(img)))
				goto error;

			// Load png image
			png_init_io(img, f);
			png_set_sig_bytes(img, 8);
			png_read_info(img, img_info);

			//get info
			//bpp = png_get_bit_depth( img, img_info );
			color_type = png_get_color_type(img, img_info);

			// if indexed, make it RGB
			if (PNG_COLOR_TYPE_PALETTE == color_type)
				png_set_palette_to_rgb(img);

			// if 1/2/4 bits gray, make it 8-bits gray
			if (PNG_COLOR_TYPE_GRAY && 8 > bpp)
				png_set_expand_gray_1_2_4_to_8(img);

			if (png_get_valid(img, img_info, PNG_INFO_tRNS))
				png_set_tRNS_to_alpha(img);

			if (16 == bpp)
				png_set_strip_16(img);
			else if (8 > bpp)
				png_set_packing(img);

			// get img info
			png_read_update_info(img, img_info);
			png_get_IHDR(img, img_info, &w, &h, &bpp, &color_type, NULL, NULL, NULL);
			t->width = w;
			t->height = h;


			get_PNG_info(color_type, t);


			// create actual texel array
			rowbytes = png_get_rowbytes(img, img_info);
			t->texels = (GLubyte*)calloc(1, rowbytes * h);
			row_pointers = (png_bytep*)calloc(1, sizeof(png_bytep) * h);

			// Set pointers to rows in flipped-y order (GL need (0,0) at bottomleft)
			for (u32 i = 0; i < t->height; ++i)
				row_pointers[i] = t->texels + (h - 1 - i) * rowbytes;

			png_read_image(img, row_pointers);
			//png_read_end( img, NULL );
			png_destroy_read_struct(&img, &img_info, NULL);
			free(row_pointers);
			fclose(f);

			return t;

		error:
			if (f) fclose(f);
			if (img_info) png_destroy_read_struct(&img, &img_info, NULL);
			if (t) delete t;
			free(row_pointers);
			return NULL;
		}

		bool Load(Data &texture, const std::string &filename) {
			_tex *t = NULL;

			if (Resource::CheckExtension(filename, "png"))
				t = LoadPNG(filename);
			else {
				LogErr("Format of '", filename, "' can't be loaded.");
				return false;
			}

			if (!t || !t->texels)
				return false;

			glGenTextures(1, &t->id);
			glBindTexture(GL_TEXTURE_2D, t->id);

			// Nearest Mipmapping for minification
			// Nearest for magnification
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

			//    GLint curr_alignment;
			//    glGetIntegerv(GL_UNPACK_ALIGNMENT, &curr_alignment);
			//    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

			glTexImage2D(GL_TEXTURE_2D, 0, t->int_fmt, t->width, t->height, 0,
				t->fmt, GL_UNSIGNED_BYTE, t->texels);

			glGenerateMipmap(GL_TEXTURE_2D);

			//    glPixelStorei(GL_UNPACK_ALIGNMENT, curr_alignment);

			texture.id = t->id;
			texture.size = vec2i(t->width, t->height);

			free(t->texels);
			free(t);

			return true;

		}

	}
}
