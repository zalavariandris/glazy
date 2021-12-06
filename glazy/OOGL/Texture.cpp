#include "Texture.h"
#include <iostream>

/* file utils */
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <fstream>
#include <sstream>


inline unsigned char* read_image(std::string path, int* width, int* height, int* channels) {
	unsigned char* data = stbi_load(path.c_str(), width, height, channels, 0);
	return data;
}

inline void free_image(void* data) {
	stbi_image_free(data);
}

namespace OOGL {
	Texture Texture::from_data(
		GLsizei width,
		GLsizei height,
		const unsigned char* data,
		GLint internalformat,
		GLenum format,
		GLint type,
		GLint min_filter,
		GLint mag_filter,
		GLint wrap_s,
		GLint wrap_t
	)
	{
		auto texture = Texture();
		glBindTexture(GL_TEXTURE_2D, texture);
		glBindTexture(GL_TEXTURE_2D, texture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_s);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_t);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);

		glTexImage2D(GL_TEXTURE_2D, 0, internalformat, width, height, 0, format, type, data);
		//glGenerateMipmap(GL_TEXTURE_2D);

		glBindTexture(GL_TEXTURE_2D, 0);
		return texture;
	}

	Texture Texture::from_file(std::string path) {
		int width, height, nrChannels;
		unsigned char* data = read_image(path.c_str(), &width, &height, &nrChannels);

		if (!data)
			std::cout << "Failed to load texture from file: " << path << std::endl;

		auto texture = Texture::from_data(width, height, data);

		free_image(data);
		return texture;
	}
}