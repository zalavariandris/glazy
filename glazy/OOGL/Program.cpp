#include "Program.h"

#include <map>
#include <glad/glad.h>
#include <iostream>

#include "Shader.h"
#include "SmartGLObject.h"

#include "glm/glm.hpp"
#include "glm/gtc/type_ptr.hpp"

std::vector<GLint> program_stack;

//GLint restoreId;
//void push_state() {
//	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &restoreId);
//}
//void pop_state() {
//	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, restoreId);
//}

void push_state(GLuint program) {
	GLint current_prog = 0;
	glGetIntegerv(GL_CURRENT_PROGRAM, &current_prog);

	program_stack.push_back(current_prog);

	if (program != current_prog) {
		glUseProgram(program);
	}
}

GLuint pop_state() {
	assert(program_stack.size() > 0);

	GLint current_prog = 0;
	glGetIntegerv(GL_CURRENT_PROGRAM, &current_prog);

	GLuint program = program_stack.back();
	if (program != current_prog) {
		glUseProgram(program);
	}
	program_stack.pop_back();
	return program;
}

OOGL::Program OOGL::Program::from_shaders(const OOGL::Shader & vertexShader, const OOGL::Shader & fragmentShader)
{
	assert(glIsShader(vertexShader));
	assert(glIsShader(fragmentShader));

	auto program = OOGL::Program();


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

void OOGL::Program::set_uniforms(std::map<std::string, UniformVariant> uniforms) {
	push_state(this->_id);
	glUseProgram(this->_id);
	for (auto& [name, data] : uniforms)
	{
		GLint location = glGetUniformLocation(this->_id, name.c_str());
		if (location < 0) {
			std::cout << "WARNING:GLAZY: '" << name << "' uniform is not used!" << std::endl;
		}

		if (auto value = std::get_if<bool>(&data)) {
			glUniform1i(location, *value ? 1 : 0);
		}
		else if (auto value = std::get_if<int>(&data)) {
			glUniform1i(location, *value);
		}
		else if (auto value = std::get_if<float>(&data)) {
			glUniform1f(location, *value);
		}
		else if (auto value = std::get_if<GLuint>(&data)) {
			glUniform1i(location, *value);
		}
		else if (auto value = std::get_if<glm::vec3>(&data)) {
			glUniform3f(location, value->x, value->y, value->z);
		}
		else if (auto value = std::get_if<glm::mat4>(&data)) {
			glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(*value));
		}
		else {
			std::cout << "WARNING:GLAZY: '" << location << "' type is not handled" << std::endl;
		}
	}
	pop_state();
}

void OOGL::Program::use() const {
	if (!glIsProgram(_id)) {
		std::cout << "ERROR:GLObject not initalized; call Make fiirst" << std::endl;
		return;
	}
	glUseProgram(this->id());
}

//std::map<GLuint, int> Program::refs; // init static member ?
