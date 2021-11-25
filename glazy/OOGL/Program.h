#pragma once

#include <map>
#include <glad/glad.h>
#include <iostream>

#include "Shader.h"
#include "SmartGLObject.h"

#include "glm/glm.hpp"

namespace OOGL {
	class Program : SmartGLObject {
	public:
		using SmartGLObject::SmartGLObject;

		void GLCreate() override;

		void GLDestroy() override;

		bool HasGLObject() const override;

		//Program(GLuint id)
		//{
		//	if (!glIsProgram(id)) {
		//		throw std::invalid_argument("OOGL: Invalid program id");
		//	}

		//	this->_id = id;
		//	inc();

		//	std::cout << "create program: " << this->_id << " refs: " << this->count() << std::endl;
		//};

		static Program from_shaders(Shader vertexShader, Shader fragmentShader);

		static Program from_file(const char* vertex_path, const char* fragment_path);

		void set_uniform(const char* name, const glm::mat4& value);

		void set_uniform(const char* name, bool value);

		void set_uniform(const char* name, glm::vec3 value);
		void use() const;
	};
	//std::map<GLuint, int> Program::refs; // init static member ?
}