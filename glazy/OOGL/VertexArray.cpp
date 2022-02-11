#include "VertexArray.h"
#include "Program.h"

namespace OOGL {
	VertexArray VertexArray::from_attributes(
		std::map <GLuint, std::tuple<GLuint, GLsizei>> attributes)
	{
		auto vao = VertexArray();
		glBindVertexArray(vao);
		for (const auto& [location, attribute] : attributes) {
			const auto& [vbo, size] = attribute;
			glBindBuffer(GL_ARRAY_BUFFER, vbo);
			glEnableVertexAttribArray(location);
			glVertexAttribPointer(location, size, GL_FLOAT, GL_FALSE, 0, nullptr);
		}
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
		return vao;
	}

	VertexArray VertexArray::from_attributes(const Program & program,
		std::map <std::string, std::tuple<GLuint, GLsizei>> attributes)
	{
		auto vao = VertexArray();
		glBindVertexArray(vao);
		for (const auto& [name, attribute] : attributes) {
			auto location = glGetAttribLocation(program, name.c_str());
			if (location < 0) {
				std::cout << "WARNING::GL: attribute '" << name << "' is not active!" << std::endl;
			}
			const auto& [vbo, size] = attribute;
			glBindBuffer(GL_ARRAY_BUFFER, vbo);
			glEnableVertexAttribArray(location);
			glVertexAttribPointer(location, size, GL_FLOAT, GL_FALSE, 0, nullptr);
		}
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
		return vao;
	}
}
