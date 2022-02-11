#pragma once
#include "SmartGLObject.h"
#include <string>
#include <assert.h>

namespace OOGL {
	class Texture : public SmartGLObject
	{
	public:
		Texture() : SmartGLObject(
			[]() {
				GLuint texture_id;
				glGenTextures(1, &texture_id);
				return texture_id;
			},
			[](GLuint texture_id) {
				assert(glIsTexture(texture_id));
				glDeleteTextures(1, &texture_id);
			}
		) {}

		static Texture from_data(
			GLsizei width = -1,
			GLsizei height = -1,
			const unsigned char* data = NULL,
			GLint internalformat = GL_RGB,
			GLenum format = GL_RGB,
			GLint type = GL_UNSIGNED_BYTE,
			GLint min_filter = GL_LINEAR,
			GLint mag_filter = GL_NEAREST,
			GLint wrap_s = GL_REPEAT,
			GLint wrap_t = GL_REPEAT
		);

		static Texture from_file(std::string path);
	};
}