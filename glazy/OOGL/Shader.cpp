#include "Shader.h"
#include <map>
#include <iostream>
#include <glad/glad.h>
#include <fstream>
#include <sstream>

inline std::string read_text(const char* path) {
	std::string content;
	std::cout << "read: " << path << std::endl;
	std::string code;
	std::ifstream file(path, std::ios::in);
	std::stringstream stream;

	if (!file.is_open()) {
		std::cerr << "Could not read file " << path << ". File does not exist." << std::endl;
	}

	std::string line = "";
	while (!file.eof()) {
		std::getline(file, line);
		content.append(line + "\n");
	}
	file.close();
	return content;
}

OOGL::Shader::Shader(GLuint id) {
	if (!glIsShader(id)) {
		throw std::invalid_argument("OOGL: Invalid shader id");
	}

	if (refs.find(id) == refs.end()) {
		refs[id] = 0;
	}

	refs[id] += 1;
	this->_id = id;
	std::cout << "create shader: " << this->_id << " refs: " << refs[this->_id] << std::endl;
};

OOGL::Shader::Shader(const OOGL::Shader& other) {
	std::cout << "copy shader: " << refs[other._id] << " refs: " << refs[other._id] << std::endl;

	if (glIsShader(other._id)) {
		refs[other._id] += 1;
	}
	this->_id = other._id;
}

OOGL::Shader& OOGL::Shader::operator=(const OOGL::Shader& other)
{
	if (glIsShader(this->_id)) {
		refs[this->_id] -= 1;
	}
	if (glIsShader(other.id())) {
		refs[other.id()] += 1;
	}

	this->_id = other.id();

	return *this;
}

OOGL::Shader::~Shader() {
	std::cout << "delete shader: " << this->id() << " refs: " << refs[this->id()] << std::endl;

	refs[this->id()] -= 1;
	// clenup gl if no references left
	if (refs[this->id()] > 0) {
		return;
	}

	if (glIsShader(this->id())) {
		std::cout << "GC shader: " << this->id() << std::endl;
		glDeleteShader(this->id());
	}
}

OOGL::Shader OOGL::Shader::from_source(GLenum type, const char* shaderSource) {
	// create gl shader
	GLuint shader_id = glCreateShader(type);
	glShaderSource(shader_id, 1, &shaderSource, NULL);
	glCompileShader(shader_id);

	// print compile errors if any
	int success;
	char infoLog[512];
	glGetShaderiv(shader_id, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(shader_id, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
	};

	// Create Object
	auto instance = Shader(shader_id);
	return instance;
}

OOGL::Shader OOGL::Shader::from_file(GLenum type, const char* shader_path)
{
	return OOGL::Shader::from_source(type, read_text(shader_path).c_str());
}

GLuint OOGL::Shader::id() const {
	return _id;
}

std::map<GLuint, int> OOGL::Shader::refs;
