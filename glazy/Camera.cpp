#include "Camera.h"

#pragma once
#include "glm/ext.hpp"
#include "glad/glad.h"
#include <iostream>

glm::vec3 ray_plane_intersection(glm::vec3 p0, glm::vec3 p1,
	glm::vec3 p_co, glm::vec3 p_no,
	double epsilon = 1e-6)
{
	/*p0, p1 : Define the line.
		p_co, p_no : define the plane :
		p_co Is a point on the plane(plane coordinate).
		p_no Is a normal vector defining the plane direction;
		(does not need to be normalized).

		Return a Vector or None(when the intersection can't be found).
	*/
	auto u = p1 - p0;
	auto dot = glm::dot(p_no, u);

	if (abs(dot) > epsilon) {
		/*
		The factor of the point between p0->p1(0 - 1)
		if 'fac' is between (0 - 1) the point intersects with the segment.
		Otherwise:
			< 0.0: behind p0.
			> 1.0: infront of p1.
			*/
		auto w = p0 - p_co;
		auto fac = -glm::dot(p_no, w) / dot;
		u = u * fac;
		return p0 + u;
	}
	else {
		//The segment is parallel to plane.
		return glm::vec3(0, 0, 0);
	}
}

glm::mat4 Camera::getProjection() const {
	if (ortho) {
		auto target_distance = glm::distance(eye, target);
		auto t = tan(fov / 2) * target_distance;
		return glm::ortho(
			-1.0f * aspect * t,
			+1.0f * aspect * t,
			-1.0f * t,
			+1.0f * t,
			near_plane, far_plane);
	}
	else {
		auto tilt = tiltshift * (float)near_plane;
		auto t = tan(fov / 2);
		return glm::frustum(
			-1.0f * aspect * t * near_plane,
			+1.0f * aspect * t * near_plane,
			-1.0f * t * near_plane,
			+1.0f * t * near_plane,
			near_plane, far_plane); // left right, bottom, top, near, far
	}
}

glm::mat4 Camera::getView() const {
	return glm::lookAt(eye, target, up);
}

double Camera::get_target_distance() const {
	return glm::distance(eye, target);
}

void Camera::orbit(double yaw, double pitch) {
	const auto forward = glm::normalize(target - eye);
	const auto right = glm::cross(forward, up);
	//const auto up = glm::cross(forward, right);

	auto pivot = target;
	auto m = glm::mat4(1);
	m = glm::translate(m, pivot);

	m = glm::rotate(m, (float)yaw, up);
	m = glm::rotate(m, (float)pitch, right);

	m = glm::translate(m, -pivot);

	eye = glm::vec3(m * glm::vec4(eye, 1.0));
}

void Camera::pan(double horizontal, double vertical) {
	auto view = getView();

	auto right = glm::vec3(view[0][0], view[1][0], view[2][0]);
	auto up = glm::vec3(view[0][1], view[1][1], view[2][1]);
	auto forward = glm::vec3(view[0][2], view[1][2], view[2][2]);

	auto target_distance = glm::distance(eye, target);
	auto offset = right * (float)horizontal - up * (float)vertical;
	offset *= tan(fov / 2) * 2;
	offset *= target_distance;
	target += offset;
	eye += offset;
}

///unproject to XY plane normalized coordinates
glm::vec3 Camera::screen_to_planeXY(double mouseX, double mouseY) const {
	const auto viewport = glm::vec4(-1, -1, 2, 2);

	glm::mat4 proj = getProjection();
	glm::mat4 view = getView();

	auto near_point = glm::unProject(glm::vec3(mouseX, mouseY, 0.0), view, proj, viewport);
	auto far_point = glm::unProject(glm::vec3(mouseX, mouseY, 1.0), view, proj, viewport);

	return ray_plane_intersection(near_point, far_point, { 0,0,0 }, { 0,0,1 });
}

///unproject to XY plane to viewport coordinates
glm::vec3 Camera::screen_to_planeXY(double mouseX, double mouseY, glm::vec4 viewport) const {
	glm::mat4 proj = getProjection();
	glm::mat4 view = getView();

	mouseY = mouseY;

	auto near_point = glm::unProject(glm::vec3(mouseX, mouseY, 0.0), view, proj, viewport);
	auto far_point = glm::unProject(glm::vec3(mouseX, mouseY, 1.0), view, proj, viewport);

	return ray_plane_intersection(near_point, far_point, { 0,0,0 }, { 0,0,1 });
}

void Camera::dolly(double offset){
	auto view = getView();
	auto forward = glm::normalize(glm::vec3(view[0][2], view[1][2], view[2][2]));
	this->eye += forward * (float)offset;
}

void Camera::to_program(unsigned int shader_program, std::string projection_name, std::string view_name) const {
	int projectionLocation = glGetUniformLocation(shader_program, "projection");
	glUniformMatrix4fv(projectionLocation, 1, GL_FALSE, glm::value_ptr(getProjection()));
	int viewLocation = glGetUniformLocation(shader_program, "view");
	glUniformMatrix4fv(viewLocation, 1, GL_FALSE, glm::value_ptr(getView()));
}