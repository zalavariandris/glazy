#include "Program.h"

#include <map>
#include <glad/glad.h>
#include <iostream>

#include "Shader.h"
#include "SmartGLObject.h"

#include "glm/glm.hpp"
#include "glm/gtc/type_ptr.hpp"


OOGL::Program OOGL::Program::from_shaders(OOGL::Shader vertexShader, OOGL::Shader fragmentShader)
{
	auto program = OOGL::Program();

	assert(glIsShader(vertexShader));
	assert(glIsShader(fragmentShader));

	glAttachShader((GLuint)program, (GLuint)vertexShader);
	glAttachShader((GLuint)program, (GLuint)fragmentShader);
	glLinkProgram((GLuint)program);

	// print linking errors if any
	int success;
	char infoLog[512];
	glGetProgramiv((GLuint)program, GL_LINK_STATUS, &success);
	if (!success)
	{
		glGetProgramInfoLog((GLuint)program, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
	}

	return program;
}

OOGL::Program OOGL::Program::from_file(const char* vertex_path, const char* fragment_path) {
	return OOGL::Program::from_shaders(
		OOGL::Shader::from_file(GL_VERTEX_SHADER, vertex_path),
		OOGL::Shader::from_file(GL_FRAGMENT_SHADER, fragment_path)
	);
}

void OOGL::Program::set_uniform(const char* name, const glm::mat4& value)
{
	if (glIsProgram(_id)) {
		std::cout << "ERROR:GLObject not initalized; call Make first" << std::endl;
		return;
	}
	auto location = glGetUniformLocation(this->id(), name);
	glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(value));
}

void OOGL::Program::set_uniform(const char* name, bool value) {
	if (glIsProgram(_id)) {
		std::cout << "ERROR:GLObject not initalized; call Make fiirst" << std::endl;
		return;
	}
	auto location = glGetUniformLocation(this->id(), name);
	glUniform1i(location, value ? 1 : 0);
}

void OOGL::Program::set_uniform(const char* name, glm::vec3 value) {
	if (!this->_existFunc(this->_id)) {
		std::cout << "ERROR:GLObject not initalized; call Make fiirst" << std::endl;
	}
	auto location = glGetUniformLocation(this->id(), name);
	glUniform3f(location, value.x, value.y, value.z);
}

void OOGL::Program::use() const {
	if (glIsProgram(_id)) {
		std::cout << "ERROR:GLObject not initalized; call Make fiirst" << std::endl;
		return;
	}
	glUseProgram(this->id());
}

//std::map<GLuint, int> Program::refs; // init static member ?
