#pragma once

// glm
#include "glm/glm.hpp"
#include <glad/glad.h>

namespace imdraw {

	// set brush state
	void set_color(glm::vec3 rgb, float opacity = 0.0);
	void set_texture(GLuint texture);
	inline void set_mode(GLenum mode, GLenum face = GL_FRONT_AND_BACK) {
		glPolygonMode(face, mode);
	}
	void set_model(glm::mat4 model_matrix);
	void set_view(glm::mat4 view_matrix);
	void set_projection(glm::mat4 projection_matrix);

	// draw shapes
	void triangle();
	void quad();
	void disc();

	void cross();
	void grid();

	void cube();
	void cylinder();
	void sphere();

	void lines(std::vector<glm::vec3> P, std::vector<glm::vec3> Q);

	void text(char* string);
}