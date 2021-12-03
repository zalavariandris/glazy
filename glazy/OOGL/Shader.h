#pragma once
#include <map>
#include <iostream>
#include <glad/glad.h>
#include <fstream>
#include <sstream>

#include "SmartGLObject.h" 

namespace OOGL {
	class Shader : public SmartGLObject
	{
	public:
		Shader(GLenum type) : SmartGLObject(
			[type]()->GLuint{
				return glCreateShader(type);
			}, 
			[](GLuint shader_id) {
				glDeleteShader(shader_id);
			}, 
			[](GLuint shader_id)->bool {
				std::cout << "call shader exist" << std::endl;
				return glIsShader(shader_id);
			}){}

		static Shader from_source(GLenum type, const char* shaderSource);
		static Shader from_file(GLenum type, const char* shader_path);
	};
}