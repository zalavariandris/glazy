#pragma once

// glm
#include "glm/glm.hpp"
#include <glad/glad.h>

namespace imdraw {
	struct State {
		glm::mat4 transform=glm::mat4(1);
		glm::vec3 color=glm::vec3(1);
		GLuint texture = 0;;
		GLenum mode=GL_FILL;
	};

	// set brush state
	//void set_color(glm::vec3 rgb, float opacity = 0.0);
	//inline void set_mode(GLenum mode, GLenum face = GL_FRONT_AND_BACK) {
	//	glPolygonMode(face, mode);
	//}
	//void set_model(glm::mat4 model_matrix);
	void set_view(glm::mat4 view_matrix);
	void set_projection(glm::mat4 projection_matrix);

	// draw shapes
	void triangle();
	void quad(GLuint texture);
	void disc(glm::vec3 center, float diameter=1.0, glm::vec3 color=glm::vec3(1));
	void disc(std::vector<glm::vec3> center, float diameter=1.0, glm::vec3 color=glm::vec3(1));

	void cross(glm::vec3 center, float size=1.0);
	void grid();

	void cube(glm::vec3 center, float size=1.0);
	void sharp_cube(glm::vec3 center, float size = 1.0);
	void cylinder(glm::vec3 center, float size=1.0);
	void sphere(glm::vec3 center, float diameter=1.0);

	void lines(std::vector<glm::vec3> P, std::vector<glm::vec3> Q);
	
	void text(glm::vec3 basepoint, char* string);
}