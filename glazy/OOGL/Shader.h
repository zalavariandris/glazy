#pragma once
#include <map>
#include <iostream>
#include <glad/glad.h>
#include <fstream>
#include <sstream>

namespace OOGL {
	class Shader
	{
	private:
		GLuint _id;
		static std::map<GLuint, int> refs;
	public:
		Shader(GLuint id);

		Shader(const Shader& other);

		Shader& operator=(const Shader& other);

		~Shader();

		static Shader from_source(GLenum type, const char* shaderSource);

		static Shader from_file(GLenum type, const char* shader_path);

		GLuint id() const;
	};
}