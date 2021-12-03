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

OOGL::Shader OOGL::Shader::from_source(GLenum type, const char* shaderSource) {
	// create gl shader
	auto shader = Shader(type);
	glShaderSource(shader, 1, &shaderSource, NULL);
	glCompileShader(shader);

	// print compile errors if any
	int success;
	char infoLog[512];
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(shader, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
	};

	// Create Object
	
	return shader;
}

OOGL::Shader OOGL::Shader::from_file(GLenum type, const char* shader_path)
{
	return OOGL::Shader::from_source(type, read_text(shader_path).c_str());
}