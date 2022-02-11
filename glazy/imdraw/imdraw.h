#pragma once

// glm
#include "glm/glm.hpp"
#include <glad/glad.h>
#include <vector>

namespace imdraw {
	enum Mode {
		FILL,
		LINE
	};
	struct Material {
		Mode mode = FILL;
		glm::vec3 color = glm::vec3(1);
		float opacity = 0.0;
		GLuint texture=0;
	};

	void set_view(glm::mat4 view_matrix);
	void set_projection(glm::mat4 projection_matrix);

	// draw shapes
	void triangle();
	
	void disc(glm::vec3 center, float diameter=1.0, glm::vec3 color=glm::vec3(1));
	void disc(std::vector<glm::vec3> center, float diameter=1.0, glm::vec3 color=glm::vec3(1));
	

	void quad(GLuint texture, glm::vec2 min_rect = { -1,-1 }, glm::vec2 max_rect = { 1,1 }, glm::vec2 uv_tiling = { 1,1 }, glm::vec2 uv_offset = { 0,0 }, float opacity=0.0);
	void rect(glm::vec2 rect_min, glm::vec2 rect_max);
	void rect(glm::vec2 rect_min, glm::vec2 rect_max, Material material);

	void cross(glm::vec3 center, float size=1.0);
	void grid();
	void axis(float size=1.0);

	void cube(glm::vec3 center, float size=1.0, glm::vec3 color=glm::vec3(1,1,1));
	void sharp_cube(glm::vec3 center, float size = 1.0);
	void cylinder(glm::vec3 center, float size=1.0);
	void sphere(glm::vec3 center, float diameter=1.0);

	void lines(std::vector<glm::vec3> P, std::vector<glm::vec3> Q);

	void arrow(glm::vec3 A, glm::vec3 B, glm::vec3 color);
	
	void text(glm::vec3 basepoint, char* string);
}
