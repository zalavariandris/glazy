#pragma once
#include <glm/glm.hpp>
#include <string>

class Camera {
public:
	glm::vec3 eye;
	glm::vec3 target;
	float fov; //fovy
	glm::vec2 tiltshift;
	float aspect;
	float near_plane;
	float far_plane;
	bool ortho;

	Camera() {
		eye = glm::vec3(0, 0, -3);
		target = glm::vec3(0, 0, 0);
		fov = 1.57 / 2;
		tiltshift = glm::vec2(0.0, 0.0);
		aspect = 1.0;
		near_plane = 0.1;
		far_plane = 10000;
		ortho = false;
	}

	glm::mat4 getProjection() const;

	glm::mat4 getView() const;

	void orbit(double yaw, double pitch);

	void pan(double horizontal, double vertical);

	glm::vec3 screen_to_planeXY(double mouseX, double mouseY, int screen_width, int screen_height) const;

	void dolly(double offset);

	void to_program(unsigned int shader_program, std::string projection_name = "projection", std::string view_name = "view") const;
};


