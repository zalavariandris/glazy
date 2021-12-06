#pragma once
#include "SmartGLObject.h"
#include <vector>
#include "glm/glm.hpp"
#include <assert.h>
namespace OOGL{
	class ArrayBuffer : public SmartGLObject
	{
	public:
		ArrayBuffer() : SmartGLObject(
			[]()->GLuint {
				GLuint buffer_id;
				glGenBuffers(1, &buffer_id);
				return buffer_id;
			},
			[](GLuint buffer_id) {
				assert(glIsBuffer(buffer_id));
				glDeleteBuffers(1, &buffer_id);
			}
		) {}

		static ArrayBuffer from_data(
			const std::vector<glm::vec2>& data,
			GLenum usage = GL_STATIC_DRAW
		);
		static ArrayBuffer from_data(
			const std::vector<glm::vec3>& data, 
			GLenum usage = GL_STATIC_DRAW
		);
		static ArrayBuffer from_data(
			const std::vector<glm::mat4>& data,
			GLenum usage = GL_STATIC_DRAW
		);
	};
}

