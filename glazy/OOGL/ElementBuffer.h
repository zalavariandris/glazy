#pragma once
#include "SmartGLObject.h"
#include <vector>
#include "glm/glm.hpp"

#include <assert.h>

namespace OOGL {
	class ElementBuffer : public SmartGLObject
	{
	public:
		ElementBuffer() : SmartGLObject(
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

		static ElementBuffer from_data(
			const std::vector<unsigned int>& data,
			GLenum usage = GL_STATIC_DRAW
		);
	};
}
