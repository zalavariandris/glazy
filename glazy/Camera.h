#pragma once
#include <glm/glm.hpp>
#include <string>

class Camera {
public:
	glm::vec3 eye;
	glm::vec3 target;
	glm::vec3 up;

	float fovy;
	glm::vec2 tiltshift;
	float aspect; // x/y
	float near_plane;
	float far_plane;
	bool ortho;

	Camera() {
		eye = glm::vec3(0, 0, -5);
		target = glm::vec3(0, 0, 0);
		up = glm::vec3(0, 1, 0);
		fovy = 1.57 / 2;
		tiltshift = glm::vec2(0.0, 0.0);
		aspect = 1.0;
		near_plane = 0.1;
		far_plane = 10000;
		ortho = false;
	}

	/// <summary>
	/// A 3D camera
	/// </summary>
	/// <param name="fov">y field of view in radians</param>
	Camera(glm::vec3 eye, glm::vec3 target, glm::vec3 up, bool ortho=false, float aspect = 1.0, float fovy=1.57/2, glm::vec2 tiltshift=glm::vec2(0.0f,0.0f), float near_plane=0.1, float far_plane=100000) :
		eye(eye),
		target(target),
		up(up),
		ortho(ortho),
		fovy(fovy),
		aspect(aspect),
		tiltshift(tiltshift),
		near_plane(near_plane),
		far_plane(far_plane)
	{

	};

	float fovx() {
		return 2 * atan(tan(fovy / 2) * aspect);
	}

	glm::mat4 getProjection() const;

	glm::mat4 getView() const;

	glm::vec3 forward() const;

	double get_target_distance() const;

	void orbit(double yaw, double pitch);

	void pan(double horizontal, double vertical);

	glm::vec3 screen_to_planeXY(double mouseX, double mouseY) const;
	glm::vec3 screen_to_planeXY(double mouseX, double mouseY, glm::vec4 viewport) const;

	void dolly(double offset);
	void dolly(double offset, float mouseX, float mouseY, glm::vec4 viewport);

	void to_program(unsigned int shader_program, std::string projection_name = "projection", std::string view_name = "view") const;

	void fit(float width, float height);
};


