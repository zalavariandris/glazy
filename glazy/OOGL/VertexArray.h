#pragma once
#include "SmartGLObject.h"
#include "Program.h"
#include <string>
#include <assert.h>

namespace OOGL {
	class VertexArray : public SmartGLObject
	{

	public:
		VertexArray() : SmartGLObject(
			[]()->GLuint {
				GLuint vao;
				glGenVertexArrays(1, &vao);
				return vao;
			},
			[](GLuint vao_id) {
				assert(glIsVertexArray(vao_id));
				glDeleteVertexArrays(1, &vao_id);
			}
		) {}

		static VertexArray from_attributes(
			std::map <GLuint, std::tuple<GLuint, GLsizei>> attributes
		);

		static VertexArray from_attributes(
			const Program& program,
			std::map <std::string, std::tuple<GLuint, GLsizei>> attributes
		);
	};
}
