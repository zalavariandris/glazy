#pragma once
#include <map>
#include <vector>
#include <assert.h>
#include <glad/glad.h>

namespace OOGL {
	namespace state {
		std::map<GLenum, std::vector<GLuint>> state{
			{GL_CURRENT_PROGRAM, {}},

			{GL_ARRAY_BUFFER, {}},
			{GL_ELEMENT_ARRAY_BUFFER, {}},
	
			{GL_TEXTURE_BUFFER, {}}
		};

		void push(GLenum target, GLuint program);
		GLuint pop(GLenum target);
	}
}