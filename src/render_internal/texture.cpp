#include "texture.h"
#include "../common/resource.h"
#include "device.h"
#include "GL/glew.h"

// PNG loading (header in ext/ directory)
#include <png.h>




namespace Render {
	namespace Texture {
		FormatDesc GetTextureFormat(TextureFormat fmt) {
			static FormatDesc fmts[_FORMAT_MAX] = {
				{ 0, 0, 0, GL_NONE, GL_NONE, GL_NONE },

				// unsigned
				{ 1,  8, 1, GL_RED, GL_RED, GL_UNSIGNED_BYTE }, // R8U
				{ 2, 16, 2, GL_RG, GL_RG, GL_UNSIGNED_BYTE }, // RG8U
				{ 3, 24, 3, GL_RGB, GL_RGB, GL_UNSIGNED_BYTE }, // RGB8U
				{ 4, 32, 4, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE }, // RGBA8U

				// float
				{  2,  16, 1, GL_R16F, GL_RED, GL_FLOAT }, // R16F
				{  4,  32, 2, GL_RG16F, GL_RG, GL_FLOAT }, // RG16F
				{  6,  48, 3, GL_RGB16F, GL_RGB, GL_FLOAT }, // RGB16F
				{  8,  64, 4, GL_RGBA16F, GL_RGBA, GL_FLOAT }, // RGBA16F

				{  4,  32, 1, GL_RED, GL_RED, GL_FLOAT }, // R32F
				{  8,  64, 2, GL_RG, GL_RG, GL_FLOAT }, // RG32F
				{ 12,  96, 3, GL_RGB, GL_RGB, GL_FLOAT }, // RGB32F
				{ 16, 128, 4, GL_RGBA, GL_RGBA, GL_FLOAT }, // RGBA32F

				{ 2, 16, 1, GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT, GL_FLOAT }, // DEPTH16F
				{ 4, 32, 1, GL_DEPTH_COMPONENT32, GL_DEPTH_COMPONENT, GL_FLOAT }, // DEPTH32F

				{  8, 4, 4, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, GL_NONE, GL_NONE },	// DXT1
				{ 16, 8, 4, GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, GL_NONE, GL_NONE },	// DXT3
				{ 16, 8, 4, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, GL_NONE, GL_NONE },	// DXT5
			};

			return fmts[fmt];
		}

		static void get_PNG_info(int color_type, _tex *ti) {
			switch (color_type) {
			case PNG_COLOR_TYPE_GRAY:
				ti->format = R8U;
				break;
			case PNG_COLOR_TYPE_GRAY_ALPHA:
				ti->format = RG8U;
				break;
			case PNG_COLOR_TYPE_RGB:
				ti->format = RGB8U;
				break;
			case PNG_COLOR_TYPE_RGB_ALPHA:
				ti->format = RGBA8U;
				break;
			default:
				LogErr("Invalid color type!");
				break;
			}

			ti->desc = GetTextureFormat(ti->format);
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
			t->texels = nullptr;

            int rowbytes;
			// setjmp for any png loading error
			if (setjmp(png_jmpbuf(img)))
				goto error;

			// Load png image
			png_init_io(img, f);
			png_set_sig_bytes(img, 8);
			png_read_info(img, img_info);

			//get info
			bpp = png_get_bit_depth( img, img_info );
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

		/// DDS CONSTANTS

		#define FOURCC_DXT1 0x31545844 // Equivalent to "DXT1" in ASCII
		#define FOURCC_DXT2 0x32545844 // Equivalent to "DXT2" in ASCII
		#define FOURCC_DXT3 0x33545844 // Equivalent to "DXT3" in ASCII
		#define FOURCC_DXT4 0x34545844 // Equivalent to "DXT4" in ASCII
		#define FOURCC_DXT5 0x35545844 // Equivalent to "DXT5" in ASCII
		#define DDS_DX10    0x30315844 // "DX10" in ASCII, for non-special types.

		// pixel formats
		#define DDPF_ALHAPIXELS 0x00000001l
		#define DDPF_FOURCC 	0x00000004l
		#define DDPF_RGB    	0x00000040l
		enum DXGI_FORMAT 
		{
			DXGI_FORMAT_UNKNOWN                      = 0,
			DXGI_FORMAT_R32G32B32A32_TYPELESS        = 1,
			DXGI_FORMAT_R32G32B32A32_FLOAT           = 2,
			DXGI_FORMAT_R32G32B32A32_UINT            = 3,
			DXGI_FORMAT_R32G32B32A32_SINT            = 4,
			DXGI_FORMAT_R32G32B32_TYPELESS           = 5,
			DXGI_FORMAT_R32G32B32_FLOAT              = 6,
			DXGI_FORMAT_R32G32B32_UINT               = 7,
			DXGI_FORMAT_R32G32B32_SINT               = 8,
			DXGI_FORMAT_R16G16B16A16_TYPELESS        = 9,
			DXGI_FORMAT_R16G16B16A16_FLOAT           = 10,
			DXGI_FORMAT_R16G16B16A16_UNORM           = 11,
			DXGI_FORMAT_R16G16B16A16_UINT            = 12,
			DXGI_FORMAT_R16G16B16A16_SNORM           = 13,
			DXGI_FORMAT_R16G16B16A16_SINT            = 14,
			DXGI_FORMAT_R32G32_TYPELESS              = 15,
			DXGI_FORMAT_R32G32_FLOAT                 = 16,
			DXGI_FORMAT_R32G32_UINT                  = 17,
			DXGI_FORMAT_R32G32_SINT                  = 18,
			DXGI_FORMAT_R32G8X24_TYPELESS            = 19,
			DXGI_FORMAT_D32_FLOAT_S8X24_UINT         = 20,
			DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS     = 21,
			DXGI_FORMAT_X32_TYPELESS_G8X24_UINT      = 22,
			DXGI_FORMAT_R10G10B10A2_TYPELESS         = 23,
			DXGI_FORMAT_R10G10B10A2_UNORM            = 24,
			DXGI_FORMAT_R10G10B10A2_UINT             = 25,
			DXGI_FORMAT_R11G11B10_FLOAT              = 26,
			DXGI_FORMAT_R8G8B8A8_TYPELESS            = 27,
			DXGI_FORMAT_R8G8B8A8_UNORM               = 28,
			DXGI_FORMAT_R8G8B8A8_UNORM_SRGB          = 29,
			DXGI_FORMAT_R8G8B8A8_UINT                = 30,
			DXGI_FORMAT_R8G8B8A8_SNORM               = 31,
			DXGI_FORMAT_R8G8B8A8_SINT                = 32,
			DXGI_FORMAT_R16G16_TYPELESS              = 33,
			DXGI_FORMAT_R16G16_FLOAT                 = 34,
			DXGI_FORMAT_R16G16_UNORM                 = 35,
			DXGI_FORMAT_R16G16_UINT                  = 36,
			DXGI_FORMAT_R16G16_SNORM                 = 37,
			DXGI_FORMAT_R16G16_SINT                  = 38,
			DXGI_FORMAT_R32_TYPELESS                 = 39,
			DXGI_FORMAT_D32_FLOAT                    = 40,
			DXGI_FORMAT_R32_FLOAT                    = 41,
			DXGI_FORMAT_R32_UINT                     = 42,
			DXGI_FORMAT_R32_SINT                     = 43,
			DXGI_FORMAT_R24G8_TYPELESS               = 44,
			DXGI_FORMAT_D24_UNORM_S8_UINT            = 45,
			DXGI_FORMAT_R24_UNORM_X8_TYPELESS        = 46,
			DXGI_FORMAT_X24_TYPELESS_G8_UINT         = 47,
			DXGI_FORMAT_R8G8_TYPELESS                = 48,
			DXGI_FORMAT_R8G8_UNORM                   = 49,
			DXGI_FORMAT_R8G8_UINT                    = 50,
			DXGI_FORMAT_R8G8_SNORM                   = 51,
			DXGI_FORMAT_R8G8_SINT                    = 52,
			DXGI_FORMAT_R16_TYPELESS                 = 53,
			DXGI_FORMAT_R16_FLOAT                    = 54,
			DXGI_FORMAT_D16_UNORM                    = 55,
			DXGI_FORMAT_R16_UNORM                    = 56,
			DXGI_FORMAT_R16_UINT                     = 57,
			DXGI_FORMAT_R16_SNORM                    = 58,
			DXGI_FORMAT_R16_SINT                     = 59,
			DXGI_FORMAT_R8_TYPELESS                  = 60,
			DXGI_FORMAT_R8_UNORM                     = 61,
			DXGI_FORMAT_R8_UINT                      = 62,
			DXGI_FORMAT_R8_SNORM                     = 63,
			DXGI_FORMAT_R8_SINT                      = 64,
			DXGI_FORMAT_A8_UNORM                     = 65,
			DXGI_FORMAT_R1_UNORM                     = 66,
			DXGI_FORMAT_R9G9B9E5_SHAREDEXP           = 67,
			DXGI_FORMAT_R8G8_B8G8_UNORM              = 68,
			DXGI_FORMAT_G8R8_G8B8_UNORM              = 69,
			DXGI_FORMAT_BC1_TYPELESS                 = 70,
			DXGI_FORMAT_BC1_UNORM                    = 71,
			DXGI_FORMAT_BC1_UNORM_SRGB               = 72,
			DXGI_FORMAT_BC2_TYPELESS                 = 73,
			DXGI_FORMAT_BC2_UNORM                    = 74,
			DXGI_FORMAT_BC2_UNORM_SRGB               = 75,
			DXGI_FORMAT_BC3_TYPELESS                 = 76,
			DXGI_FORMAT_BC3_UNORM                    = 77,
			DXGI_FORMAT_BC3_UNORM_SRGB               = 78,
			DXGI_FORMAT_BC4_TYPELESS                 = 79,
			DXGI_FORMAT_BC4_UNORM                    = 80,
			DXGI_FORMAT_BC4_SNORM                    = 81,
			DXGI_FORMAT_BC5_TYPELESS                 = 82,
			DXGI_FORMAT_BC5_UNORM                    = 83,
			DXGI_FORMAT_BC5_SNORM                    = 84,
			DXGI_FORMAT_B5G6R5_UNORM                 = 85,
			DXGI_FORMAT_B5G5R5A1_UNORM               = 86,
			DXGI_FORMAT_B8G8R8A8_UNORM               = 87,
			DXGI_FORMAT_B8G8R8X8_UNORM               = 88,
			DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM   = 89,
			DXGI_FORMAT_B8G8R8A8_TYPELESS            = 90,
			DXGI_FORMAT_B8G8R8A8_UNORM_SRGB          = 91,
			DXGI_FORMAT_B8G8R8X8_TYPELESS            = 92,
			DXGI_FORMAT_B8G8R8X8_UNORM_SRGB          = 93,
			DXGI_FORMAT_BC6H_TYPELESS                = 94,
			DXGI_FORMAT_BC6H_UF16                    = 95,
			DXGI_FORMAT_BC6H_SF16                    = 96,
			DXGI_FORMAT_BC7_TYPELESS                 = 97,
			DXGI_FORMAT_BC7_UNORM                    = 98,
			DXGI_FORMAT_BC7_UNORM_SRGB               = 99,
			DXGI_FORMAT_FORCE_UINT                   = 0xffffffffUL 
		};


		_tex *LoadDDS(const std::string &filename) {
			_tex *t = NULL;
			u32 bufSize;

			// f = fopen(filename.c_str(), "rb");
			std::ifstream f(filename, std::ifstream::in | std::ifstream::binary);
			if (f.fail()) {
				LogErr("Could not open '", filename, "'.");
				return NULL;
			}

			// validate filecode
			char filecode[4];
			f.read((char*)filecode, sizeof(filecode));
			if(strncmp(filecode, "DDS ", 4) != 0) {
				f.close();
				return NULL;
			}

			t = new _tex();//
			t->texels = nullptr;

			// get DX9 header	
			u8 header[124];
			f.read((char*)header, sizeof(header));

			t->height 		= *(u32*)&(header[8]);
			t->width  		= *(u32*)&(header[12]);
			u32 linearSize 	= *(u32*)&(header[16]);
			t->mipmapCount 	= *(u32*)&(header[24]);
			u32 pixelFlags  = *(u32*)&(header[76]);
			u32 fourCC 		= *(u32*)&(header[80]);
			u32 bitCount	= *(u32*)&(header[84]);
			u32 *bitMask 	=  (u32*)&(header[88]); // 4 uints

			u32 dxgiFormat = 0;

			// Check pixel format
			if(pixelFlags & DDPF_RGB) {
				switch(bitCount) {
				case 8:
					t->format = R8U;
					break;
				case 16:
					t->format = RG8U;
					break;
				case 24:
					t->format = RGB8U;
					break;
				case 32:
					t->format = RGBA8U;
					break;
				}
			} else if(pixelFlags & DDPF_FOURCC) {
				u32 comp = (fourCC == FOURCC_DXT1) ? 3 : 4;
				switch(fourCC) {
				case DDS_DX10: {
					// read DX10 header info
					u8 dxgi[20];
					f.read((char*)dxgi, sizeof(dxgi));
					dxgiFormat 	   = *(u32*)&(dxgi[0]);
					// u32 dims 	   = *(u32*)&(dxgi[4]);
					// u32 miscFlags  = *(u32*)&(dxgi[8]);
					// u32 arraySize  = *(u32*)&(dxgi[12]);
					// u32 miscFlags2 = *(u32*)&(dxgi[16]);

					switch(dxgiFormat) {
					case DXGI_FORMAT_R32_FLOAT :
						t->format = R32F;
						break;
					case DXGI_FORMAT_R32G32_FLOAT :
						t->format = RG32F;
						break;
					case DXGI_FORMAT_R32G32B32_FLOAT :
						t->format = RGB32F;
						break;
					case DXGI_FORMAT_R32G32B32A32_FLOAT :
						t->format = RGBA32F;
						break;
					default:
						LogErr("Cannot load this DXGI format (", dxgiFormat, "). Add it.");
						goto err;
					}
					break;
				}
				case FOURCC_DXT1:
					t->format = DXT1;
					break;
				case FOURCC_DXT2:
				case FOURCC_DXT3:
					t->format = DXT3;
					break;
				case FOURCC_DXT4:
				case FOURCC_DXT5:
					t->format = DXT5;
					break;
				default:
					LogErr("Cannot load this DDS FOURCC format (", fourCC, "). Add it.");
					goto err;	
				}
			} else {
				LogErr("Cannot load this DDS pixelFormat (", pixelFlags, "). Add it.");
				goto err;
			}

			// retrieve format info
			t->desc = GetTextureFormat(t->format);

			{
				std::streamoff cur = f.tellg();
				f.seekg(0, std::ios_base::end);
				std::streamoff end = f.tellg();
				f.seekg(cur, std::ios_base::beg);
				bufSize = end - cur;
			}

			// bufSize = t->mipmapCount > 1 ? linearSize * 2 : linearSize;
			t->texels = (GLubyte*)malloc(bufSize * sizeof(GLubyte));
			f.read((char*)t->texels, bufSize * sizeof(GLubyte));
			f.close();

			return t;

		err:
			delete t;
			f.close();
			return NULL;
		}

		bool Load(Data &texture, const std::string &filename) {
			const Config &deviceConfig = GetDevice().GetConfig();
			_tex *t = NULL;

			if (Resource::CheckExtension(filename, "png"))
				t = LoadPNG(filename);
			else if(Resource::CheckExtension(filename, "dds"))
				t = LoadDDS(filename);
			else {
				LogErr("Format of '", filename, "' can't be loaded.");
				return false;
			}

			if (!t || !t->texels)
				return false;

			glGenTextures(1, &t->id);
			glBindTexture(GL_TEXTURE_2D, t->id);

			GLint curr_alignment;
			glGetIntegerv(GL_UNPACK_ALIGNMENT, &curr_alignment);
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);


			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, deviceConfig.anisotropicFiltering);

			switch(t->format) {
			case DXT1:
			case DXT3:
			case DXT5: {
				// load mipmaps from file
				u32 offset = 0;
				u32 w = t->width, h = t->height;
				for(u32 l = 0; l < t->mipmapCount && (w || h); ++l) {
					u32 size = ((w+3)/4)*((h+3)/4)*t->desc.blockSize;
					glCompressedTexImage2D(GL_TEXTURE_2D, l, t->desc.formatInternalGL, w, h, 0, size, t->texels + offset);

					offset += size;
					w /= 2;
					h /= 2;
					if(w < 1) w = 1;	// for N-Pow2 sized textures
					if(h < 1) h = 1;
				}
				break;
				}
			case R8U:
			case RG8U:
			case RGB8U:
			case RGBA8U:
				glTexImage2D(GL_TEXTURE_2D, 0, t->desc.formatInternalGL, t->width, t->height, 0,
					t->desc.formatInternalGL, t->desc.type, t->texels);

				glGenerateMipmap(GL_TEXTURE_2D);
				break;
			case R32F:
			case RG32F:
			case RGB32F:
			case RGBA32F:
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexImage2D(GL_TEXTURE_2D, 0, t->desc.formatInternalGL, t->width, t->height, 0,
					t->desc.formatInternalGL, t->desc.type, t->texels);
				break;
			default:
				LogErr("Invalid texture format (", t->format, ") at GL Texure creation.");
				glPixelStorei(GL_UNPACK_ALIGNMENT, curr_alignment);
				free(t->texels);
				delete t;
				return false;
			}

			glPixelStorei(GL_UNPACK_ALIGNMENT, curr_alignment);

			texture.id = t->id;
			texture.size = vec2i(t->width, t->height);

			free(t->texels);
			delete t;

			return true;

		}

		
	}
}
