#pragma once
#include "SmartGLObject.h"

#include <map>
#include <variant>
#include <glad/glad.h>
#include <iostream>

#include "Shader.h"


#include "glm/glm.hpp"

#include <assert.h>

namespace OOGL {
	using UniformVariant = std::variant<bool, int, float, glm::vec3, glm::mat4, GLuint>;
	class Program : public SmartGLObject {
		
	public:
		Program() : SmartGLObject(
			[]()->GLuint {
				return glCreateProgram();
			},
			[](GLuint program_id) {
				assert(glIsProgram(program_id));
				glDeleteProgram(program_id);
			}
		){}

		static Program from_shaders(const Shader & vertexShader, const Shader & fragmentShader);

		static Program from_file(const char* vertex_path, const char* fragment_path);

		void set_uniforms(std::map<std::string, UniformVariant> uniforms);

		void use() const;
	};
	//std::map<GLuint, int> Program::refs; // init static member ?
}