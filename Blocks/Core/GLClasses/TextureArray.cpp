#include "TextureArray.h"

namespace GLClasses
{
	TextureArray::TextureArray()
	{

	}

	void TextureArray::CreateArray(std::vector<std::string> paths, std::pair<int, int> texture_size, bool gen_mips, GLint mag_filter, bool limit_textures)
	{
		sort(paths.begin(), paths.end());
		paths.erase(unique(paths.begin(), paths.end()), paths.end());

		int layer_count = paths.size();

		if (limit_textures)
		{
			if (layer_count > 255)
			{
				throw "Textures limited = true. The number of paths exceed the maximum number of 255!";
			}
		}
		
		glGenTextures(1, &m_TextureArray);
		glBindTexture(GL_TEXTURE_2D_ARRAY, m_TextureArray);

		glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, texture_size.first, texture_size.second, layer_count, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

		if (gen_mips)
		{
			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		}
		
		else
		{
			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		}

		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, mag_filter);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);

		for (int i = 0; i < layer_count; i++)
		{
			int w, h, bpp;
			unsigned char* image = stbi_load(paths[i].c_str(), &w, &h, &bpp, 4);

			glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, i, texture_size.first, texture_size.second,
				1, GL_RGBA, GL_UNSIGNED_BYTE, image);

			m_TextureLocations[paths[i]] = i;
			stbi_image_free(image);
		}

		if (gen_mips)
		{
			glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
		}
	}
}