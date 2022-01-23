#pragma once
#include <vector>
#include "imdraw.h"
#include <tuple>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <fstream>
#include <sstream>

#include <cstdlib> // malloc, free

#include <map>
#include <any>

#include "../imgeo/imgeo.h"

#include <vector>
#include <string>
#include <iostream>

// opengl
#include <glad/glad.h>

// objects

#include <string>
#include <any>
#include <map>

#include <optional>


#include <variant>

#include "imdraw_internal.h"
#include "imdraw_shader.h"

/**********
 * Utils  *
 **********/

const GLuint POSITION_LOCATION = 0;
const GLuint UV_LOCATION = 1;
const GLuint NORMAL_LOCATION = 2;
const GLuint COLOR_LOCATION = 3;


/***********
 * IMDRAW  *
 ***********/

namespace imdraw {
	std::map<std::string, GLint> uniform_locations;
	void reset_uniforms(); // forward declaration
	GLuint program() {
		// init program
		static auto prog = []() {
			// make program
			GLuint p = imdraw::make_program_from_source(
				IMDRAW_VERTEX_SHADER, 
				IMDRAW_FRAGMENT_SHADER
			);

			// cache uniform locations
			std::vector<std::string> uniform_names = {
				"model", 
				"view", 
				"projection",
				"useInstanceMatrix",
				"color",
				"useTextureMap",
				"textureMap",
				"uv_tiling",
				"uv_offset",
				"opacity"
			};
			std::cout << "init default glazy program" << "\n";
			std::cout << "uniform locations: " << "\n";
			for (std::string name : uniform_names) {
				uniform_locations[name] = glGetUniformLocation(p, name.c_str());
				std::cout << "- " << name << ": " << uniform_locations[name] << "\n";
			}

			// set default uniforms
			imdraw::set_uniforms(p, {
				{"model", glm::mat4(1)},
				{"view", glm::mat4(1)},
				{"projection", glm::mat4(1)},
				{"useInstanceMatrix", false},
				{"color", glm::vec3(1,1,1)},
				{"useTextureMap", false},
				{"textureMap", 0},
				{"uv_tiling", glm::vec2(1,1)},
				{"uv_offset", glm::vec2(0,0)},
				{"opacity", 0.0f}
				});
			return p;
		}();

		return prog;
	}

	void reset_uniforms() {
		imdraw::set_uniforms(program(), {
			{uniform_locations["model"], glm::mat4(1)},
			//{"view", glm::mat4(1)},
			//{"projection", glm::mat4(1)},
			{uniform_locations["useInstanceMatrix"], false},
			{uniform_locations["color"], glm::vec3(1,1,1)},
			{uniform_locations["useTextureMap"], false},
			{uniform_locations["textureMap"], 0},
			{uniform_locations["uv_tiling"], glm::vec2(1,1)},
			{uniform_locations["uv_offset"], glm::vec2(0,0) }
		});
	}
}

//void imdraw::set_color(glm::vec3 rgb, float opacity) {
//	imdraw::set_uniforms(imdraw::program(), {
//		{"color", rgb},
//		{"opacity", opacity}
//		});
//}
//
//void imdraw::set_model(glm::mat4 model_matrix) {
//	imdraw::set_uniforms(imdraw::program(), {
//		{"model", model_matrix}
//		});
//}

static glm::mat4 projection_matrix;
void imdraw::set_projection(glm::mat4 M) {
	imdraw::set_uniforms(imdraw::program(), {
		{uniform_locations["projection"], M}
	});
	projection_matrix = M;
}
static glm::mat4 view_matrix;
void imdraw::set_view(glm::mat4 M) {
	imdraw::set_uniforms(imdraw::program(), {
		{uniform_locations["view"], M}
	});
	view_matrix = M;
}

void imdraw::triangle() {
	// create vbo
	static float vertices[] = {
		-0.5f, -0.5f, 0.0f,
		0.5f, -0.5f, 0.0f,
		0.0f,  0.5f, 0.0f
	};

	static GLuint vao = []() {
		unsigned int VBO;
		glGenBuffers(1, &VBO);
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

		unsigned int VAO;
		glGenVertexArrays(1, &VAO);
		glBindVertexArray(VAO);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);
		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		return VAO;
	}();

	// draw
	push_program(program());
	glBindVertexArray(vao);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	glBindVertexArray(0);
	pop_program();
}

void imdraw::quad(GLuint texture, glm::vec2 min_rect, glm::vec2 max_rect, glm::vec2 uv_tiling, glm::vec2 uv_offset, float opacity) {
	static auto geo = imgeo::quad();

	// init vao
	static auto vao = imdraw::make_vao(program(), {
		{"aPos", {make_vbo(geo.positions), 3}},
		{"aUV", {make_vbo(geo.uvs.value()), 2}},
		{"aNormal", {make_vbo(geo.normals.value()), 3}}
	});
	static auto ebo = imdraw::make_ebo(geo.indices);

	// get indices count
	static auto indices_count = (GLuint)geo.indices.size();

	// draw
	auto M = glm::mat4(1);
	auto pos = (max_rect + min_rect) / 2.0f;
	auto size = (max_rect - min_rect)/2.0f;
	M = glm::translate(M, glm::vec3(pos, 0));
	M = glm::scale(M, glm::vec3(size, 1));
	
	push_program(program());
	imdraw::reset_uniforms();
	imdraw::set_uniforms(program(), {
		{uniform_locations["useTextureMap"], true },
		{uniform_locations["model"], M},
		{uniform_locations["uv_tiling"], uv_tiling},
		{uniform_locations["uv_offset"], uv_offset},
		{uniform_locations["opacity"], opacity}
	});
	glBindTexture(GL_TEXTURE_2D, texture);
	glBindVertexArray(vao);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glDrawElements(GL_TRIANGLES, geo.indices.size(), GL_UNSIGNED_INT, NULL);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	glBindTexture(GL_TEXTURE_2D, 0);
	pop_program();
}

void imdraw::grid() {
	// create geo
	static auto geo = imgeo::grid();

	// init vao
	static auto vao = make_vao(program(), {
		{"aPos", {make_vbo(geo.positions), 3}}
		});

	// make ebo
	static auto ebo = make_ebo(geo.indices);

	// get indices count
	static auto indices_count = (GLuint)geo.indices.size();

	// draw
	
	push_program(program());
	imdraw::reset_uniforms();
	imdraw::set_uniforms(program(), {
		{uniform_locations["model"], glm::mat4(1)},
		{uniform_locations["color"], glm::vec3(0.5)},
		{uniform_locations["useTextureMap"], false }
	});
	glBindVertexArray(vao);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glDrawElements(GL_LINES, indices_count, GL_UNSIGNED_INT, NULL);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	
	pop_program();
}

void imdraw::disc(glm::vec3 center, float diameter, glm::vec3 color) {
	static auto geo = imgeo::disc();
	static auto vao = make_vao(program(), {
		{"aPos",    {make_vbo(geo.positions),      3}},
		{"aUV",     {make_vbo(geo.uvs.value()),    2}},
		{"aNormal", {make_vbo(geo.normals.value()),3}}
		});
	static auto ebo = make_ebo(geo.indices);
	static auto indices_count = (GLuint)geo.indices.size();

	//draw
	push_program(program());
	imdraw::reset_uniforms();
	set_uniforms(program(), {
		{uniform_locations["color"], color},
		{uniform_locations["model"], glm::scale(glm::translate(glm::mat4(1), center), glm::vec3(diameter/2))},
		{uniform_locations["useTextureMap"], false}
	});
	glBindVertexArray(vao);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glDrawElements(geo.mode, indices_count, GL_UNSIGNED_INT, NULL);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	pop_program();
}

void imdraw::disc(std::vector<glm::vec3> centers, float diameter, glm::vec3 color) {
	static auto geo = imgeo::disc();
	static auto vao = make_vao(program(), {
		{"aPos", {make_vbo(geo.positions), 3}}
		});
	static auto ebo = make_ebo(geo.indices);
	static auto indices_count = (GLuint)geo.indices.size();

	std::vector < glm::mat4> instances;
	for (glm::vec3 P : centers) {
		glm::mat4 M = glm::mat4(1);
		M = glm::translate(M, P);
		M = glm::scale(M, glm::vec3(diameter / 2));
		instances.push_back(M);
	}
	auto VBO = imdraw::make_vbo(instances);

	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBindVertexArray(vao);
	std::size_t vec4Size = sizeof(glm::vec4);
	
	GLint location = glGetAttribLocation(program(), "instanceMatrix");
	glEnableVertexAttribArray(location+0);
	glVertexAttribPointer(location + 0, 4, GL_FLOAT, GL_FALSE, 4 * vec4Size, (void*)0);
	
	glEnableVertexAttribArray(location + 1);
	glVertexAttribPointer(location + 1, 4, GL_FLOAT, GL_FALSE, 4 * vec4Size, (void*)(1 * vec4Size));
	
	glEnableVertexAttribArray(location + 2);
	glVertexAttribPointer(location + 2, 4, GL_FLOAT, GL_FALSE, 4 * vec4Size, (void*)(2 * vec4Size));
	
	glEnableVertexAttribArray(location + 3);
	glVertexAttribPointer(location + 3, 4, GL_FLOAT, GL_FALSE, 4 * vec4Size, (void*)(3 * vec4Size));
	
	glVertexAttribDivisor(location+0, 1);
	glVertexAttribDivisor(location+1, 1);
	glVertexAttribDivisor(location+2, 1);
	glVertexAttribDivisor(location+3, 1);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	//draw
	push_program(program());
	imdraw::reset_uniforms();
	set_uniforms(program(), {
		{uniform_locations["color"], color},
		{uniform_locations["model"], glm::mat4(1)},
		{uniform_locations["useTextureMap"], false},
		{uniform_locations["useInstanceMatrix"], true}
	});

	glBindVertexArray(vao);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glDrawElementsInstanced(geo.mode, indices_count, GL_UNSIGNED_INT, NULL, instances.size());
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	pop_program();

	glDeleteBuffers(1, &VBO);
}

void imdraw::rect(glm::vec2 rect_min, glm::vec2 rect_max) {
	std::vector<glm::vec3> data{
		{rect_min.x, rect_min.y, 0},
		{rect_max.x, rect_min.y, 0},
		{rect_max.x, rect_max.y, 0},
		{rect_min.x, rect_max.y, 0}
	};

	auto vbo = make_vbo(data);
	auto vao = make_vao(program(), {
		{"aPos", {vbo, 3}},
	});

	// draw
	push_program(program());
	imdraw::reset_uniforms();
	glBindVertexArray(vao);
	glDrawArrays(GL_LINE_LOOP, 0, data.size());
	glBindVertexArray(0);
	pop_program();

	glDeleteBuffers(1, &vbo);
	glDeleteVertexArrays(1, &vao);
}

void imdraw::cross(glm::vec3 center, float size) {
	// gepmetry
	static std::vector<glm::vec3> positions{
		{-1,0,0}, {1,0,0},
		{0,-1,0}, {0,1,0},
		{0,0,-1}, {0,0,1}
	};

	static std::vector<unsigned int> indices{
		0,1, 2,3, 4, 5
	};

	// buffers
	static auto vao = make_vao(program(), {
		{"aPos", {make_vbo(positions), 3}}
		});
	static auto ebo = make_ebo(indices);

	// draw
	push_program(program());
	imdraw::reset_uniforms();
	set_uniforms(program(), {
		{uniform_locations["model"], glm::translate(glm::mat4(1), center) * glm::scale(glm::mat4(1), glm::vec3(size))},
		{uniform_locations["useTextureMap"], false}
	});
	glBindVertexArray(vao);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glDrawElements(GL_LINES, indices.size(), GL_UNSIGNED_INT, NULL);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	pop_program();
}

void imdraw::cylinder(glm::vec3 center, float size) {
	// geometry
	auto geo = imgeo::cylinder();

	// buffers
	static auto vao = make_vao(program(), {
		{"aPos", {make_vbo(geo.positions), 3}},
		{"aNormal", {make_vbo(geo.normals.value()), 3}}
		});
	static auto ebo = make_ebo(geo.indices);

	// draw
	push_program(program());
	imdraw::reset_uniforms();
	set_uniforms(program(), {
		{uniform_locations["model"], glm::translate(glm::mat4(1), center)*glm::scale(glm::mat4(1), glm::vec3(size))},
		{uniform_locations["useTextureMap"], false}
	});
	glBindVertexArray(vao);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glDrawElements(geo.mode, geo.indices.size(), GL_UNSIGNED_INT, NULL);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	pop_program();
}

void imdraw::sphere(glm::vec3 center, float diameter) {
	// geometry
	static auto geo = imgeo::sphere(32, 32);

	// buffers
	static auto vao = make_vao(program(), {
		{"aPos", {make_vbo(geo.positions), 3}},
		{"aNormal", {make_vbo(geo.normals.value()), 3}}
		});
	static auto ebo = make_ebo(geo.indices);

	// draw
	push_program(program());
	set_uniforms(program(), {
		{uniform_locations["model"], glm::translate(glm::mat4(1), center) * glm::scale(glm::mat4(1), glm::vec3(diameter/2))},
		{uniform_locations["useTextureMap"], false}
	});
	glBindVertexArray(vao);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glDrawElements(geo.mode, geo.indices.size(), GL_UNSIGNED_INT, NULL);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	pop_program();
}

void imdraw::cube(glm::vec3 center, float size, glm::vec3 color) {
	// geometry
	static auto geo = imgeo::cube();

	// buffers
	static auto vao = make_vao(program(), {
		{"aPos", {make_vbo(geo.positions), 3}},
		{"aNormal", {make_vbo(geo.normals.value()), 3}}
		});
	static auto ebo = make_ebo(geo.indices);

	// draw
	push_program(program());
	imdraw::reset_uniforms();
	set_uniforms(program(), {
		{uniform_locations["model"], glm::translate(glm::mat4(1), center) * glm::scale(glm::mat4(1), glm::vec3(size))},
		{uniform_locations["useTextureMap"], false},
		{uniform_locations["color"], color}
	});
	glBindVertexArray(vao);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glDrawElements(geo.mode, geo.indices.size(), GL_UNSIGNED_INT, NULL);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	pop_program();
}

void imdraw::sharp_cube(glm::vec3 center, float size) {
	// geometry
	auto geo = imgeo::sharp_cube();

	// buffers
	auto pos_buffer = make_vbo(geo.positions);
	auto normals_buffer = make_vbo(geo.normals.value());
	auto vao = make_vao(program(), {
		{"aPos", {pos_buffer, 3}},
		{"aNormal", {normals_buffer, 3}}
		});
	auto ebo = make_ebo(geo.indices);

	// draw
	push_program(program());
	imdraw::reset_uniforms();
	set_uniforms(program(), {
		{uniform_locations["model"], glm::translate(glm::mat4(1), center) * glm::scale(glm::mat4(1), glm::vec3(size))},
		{uniform_locations["useTextureMap"], false}
		});
	glBindVertexArray(vao);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glDrawElements(geo.mode, geo.indices.size(), GL_UNSIGNED_INT, NULL);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	pop_program();

	glDeleteVertexArrays(1, &vao);
	glDeleteBuffers(1, &pos_buffer);
	glDeleteBuffers(1, &normals_buffer);
	glDeleteBuffers(1, &ebo);
}

void imdraw::lines(std::vector<glm::vec3> P, std::vector<glm::vec3> Q) {
	assert(P.size() == Q.size());
	std::vector<glm::vec3> data;
	for (auto i = 0; i < P.size(); i++) {
		data.push_back(P[i]);
		data.push_back(Q[i]);
	}
	auto vbo = make_vbo(data);
	auto vao = make_vao(program(), {
		{"aPos", {vbo, 3}},
	});

	// draw
	push_program(program());
	imdraw::reset_uniforms();
	glBindVertexArray(vao);
	glDrawArrays(GL_LINES, 0, data.size());
	glBindVertexArray(0);
	pop_program();

	glDeleteBuffers(1, &vbo);
	glDeleteVertexArrays(1, &vao);
}

void imdraw::arrow(glm::vec3 A, glm::vec3 B, glm::vec3 color) {
	auto forward = glm::vec3(view_matrix[0][2], view_matrix[1][2], view_matrix[2][2]);
	auto left = glm::cross(forward, A - B);
	auto right = glm::cross(forward, B - A);
	auto dir = B - A;
	std::vector<glm::vec3> positions{A, B, B+(left-dir)*0.1f, B+(right-dir)*0.1f};

	auto pos_vbo = make_vbo(positions);
	auto vao = make_vao(program(), {
		{"aPos", {pos_vbo, 3}},
	});
	std::vector<unsigned int> indices{ 0,1,1,2,1,3 };
	auto ebo = make_ebo(indices);

	// draw
	push_program(program());
	imdraw::reset_uniforms();
	imdraw::set_uniforms(program(), { { "color", color } });
	glBindVertexArray(vao);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glDrawElements(GL_LINES, indices.size(), GL_UNSIGNED_INT, NULL);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	pop_program();

	glDeleteBuffers(1, &pos_vbo);
	glDeleteBuffers(1, &ebo);
	glDeleteVertexArrays(1, &vao);
}

void imdraw::axis() {
	imdraw::arrow({ 0,0,0 }, { 1,0,0 }, { 1,0,0 });
	imdraw::arrow({ 0,0,0 }, { 0,1,0 }, { 0,1,0 });
	imdraw::arrow({ 0,0,0 }, { 0,0,1 }, { 0,0,1 });
}

