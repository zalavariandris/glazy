#pragma once

#include <map>
#include <glad/glad.h>
#include <iostream>

#include "Shader.h"
#include "SmartGLObject.h"

#include "glm/glm.hpp"

namespace OOGL {
	class Program : public SmartGLObject {
	public:
		Program() : SmartGLObject(
			[]()->GLuint {
				return glCreateProgram();
			},
			[](GLuint program_id) {
				glDeleteProgram(program_id);
			},
			[](GLuint program_id)->bool {
				std::cout << "call program exist" << std::endl;
				return glIsProgram(program_id);
			}
		){}

		static Program from_shaders(Shader vertexShader, Shader fragmentShader);

		static Program from_file(const char* vertex_path, const char* fragment_path);

		void set_uniform(const char* name, const glm::mat4& value);

		void set_uniform(const char* name, bool value);

		void set_uniform(const char* name, glm::vec3 value);
		void use() const;
	};
	//std::map<GLuint, int> Program::refs; // init static member ?
}