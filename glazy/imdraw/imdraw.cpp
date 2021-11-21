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
	GLuint program() {

		// init program
		static auto prog = []() {
			// make program
			GLuint p = imdraw::make_program_from_source(R"(#version 330 core
				layout (location = 0) in vec3 aPos;
				layout (location = 1) in vec2 aUV;
				layout (location = 2) in vec3 aNormal;
				layout (location = 3) in vec3 aColor;
				layout (location = 4) in mat4 instanceMatrix;

				uniform mat4 projection;
				uniform mat4 view;
				uniform mat4 model;
				uniform bool useInstanceMatrix;

				out vec2 vUV;
				out vec3 vNormal;
				out vec3 vColor;
				out vec3 FragPos; 

				void main()
				{
					vUV = aUV;
					vNormal = aNormal;
					vColor = aColor;

					mat4 viewModel = view*model;
					if(useInstanceMatrix){
						viewModel*=instanceMatrix;
					}

					FragPos = vec3(model * vec4(aPos, 1.0));

					gl_Position = projection * viewModel * vec4(aPos, 1.0);
				};
			)", R"(#version 330 core
				out vec4 FragColor;

				uniform vec3 color;
				uniform bool useTextureMap;
				uniform sampler2D textureMap;

				uniform bool useVertexColor;

				in vec3 FragPos; 
				in vec3 vNormal;
				in vec3 vColor;
				in vec2 vUV;

				uniform float opacity;

				struct Light
				{
					vec3 dir;
					vec3 color;
				};

				void main()
				{
					// color
					vec3 col = color;
					float alpha = 1.0;
					if(useTextureMap){
						vec4 tex = texture(textureMap, vUV);
						col *= tex.rgb;
						alpha *= tex.a;
					}

					if(useVertexColor){
						col*=vColor;
					}

					// Lighting
					vec3 norm = normalize(vNormal);
	
					// ambient
					vec3 diff = vec3(0.03);

					//keylight
					Light keylight = Light(
						normalize(vec3(0.7,1,-0.5)), 
						vec3(1,1,1)
					);
					diff += max(dot(norm, keylight.dir), 0.0)*keylight.color;

					Light filllight = Light(
						normalize(vec3(-0.7,0.5,-0.7)), 
						vec3(0.5,0.4,0.3)*0.3
					);
					diff += max(dot(norm, filllight.dir), 0.0)*filllight.color;

					Light rimlight = Light(
						normalize(vec3(0.6,0.1,0.9)), 
						vec3(0.5,0.5,0.7)*0.3
					);
					diff += max(dot(norm, rimlight.dir), 0.0)*rimlight.color;

					// calc frag color
					FragColor = vec4(col, alpha*(1.0-opacity));
				};
			)");

			// set default uniforms
			imdraw::set_uniforms(p, {
				{"model", glm::mat4(1)},
				{"view", glm::mat4(1)},
				{"projection", glm::mat4(1)},
				{"useInstanceMatrix", false},
				{"color", glm::vec3(1,0,0)},
				{"useTextureMap", false},
				{"textureMap", 0}
				});
			return p;
		}();

		return prog;
	}
}

void imdraw::set_color(glm::vec3 rgb, float opacity) {
	imdraw::set_uniforms(imdraw::program(), {
		{"color", rgb},
		{"opacity", opacity}
		});
}

void imdraw::set_model(glm::mat4 model_matrix) {
	imdraw::set_uniforms(imdraw::program(), {
		{"model", model_matrix}
		});
}

void imdraw::set_projection(glm::mat4 projection_matrix) {
	imdraw::set_uniforms(imdraw::program(), {
		{"projection", projection_matrix}
		});
}

void imdraw::set_view(glm::mat4 view_matrix) {
	imdraw::set_uniforms(imdraw::program(), {
		{"view", view_matrix}
		});
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

void imdraw::quad() {
	static auto geo = imgeo::quad();

	// init vao
	static GLuint vao = []() {
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);
		auto pos_vbo = imdraw::make_vbo(geo.positions);
		glBindBuffer(GL_ARRAY_BUFFER, pos_vbo);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

		auto uv_vbo = imdraw::make_vbo(geo.uvs.value());
		glBindBuffer(GL_ARRAY_BUFFER, pos_vbo);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

		glBindBuffer(GL_ARRAY_BUFFER, 0);

		return vao;
	}();

	// create ebo
	static GLuint ebo = []() {
		GLuint ebo;
		glGenBuffers(1, &ebo);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, geo.indices.size() * sizeof(unsigned int), geo.indices.data(), GL_STATIC_DRAW);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		return ebo;
	}();

	// get indices count
	static auto indices_count = (GLuint)geo.indices.size();

	// draw
	push_program(program());
	glBindVertexArray(vao);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glDrawElements(GL_TRIANGLES, indices_count, GL_UNSIGNED_INT, NULL);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
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
	glBindVertexArray(vao);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glDrawElements(GL_LINES, indices_count, GL_UNSIGNED_INT, NULL);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	pop_program();
}

void imdraw::disc() {
	static auto geo = imgeo::disc();
	static auto vao = make_vao(program(), {
		{"aPos", {make_vbo(geo.positions), 3}}
		});
	static auto ebo = make_ebo(geo.indices);
	static auto indices_count = (GLuint)geo.indices.size();

	//draw
	push_program(program());
	glBindVertexArray(vao);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glDrawElements(geo.mode, indices_count, GL_UNSIGNED_INT, NULL);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	pop_program();
}

void imdraw::cross() {
	// goemetry
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
	glBindVertexArray(vao);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glDrawElements(GL_LINES, indices.size(), GL_UNSIGNED_INT, NULL);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	pop_program();
}

void imdraw::cylinder() {
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
	glBindVertexArray(vao);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glDrawElements(geo.mode, geo.indices.size(), GL_UNSIGNED_INT, NULL);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	pop_program();
}

void imdraw::sphere() {
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
	glBindVertexArray(vao);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glDrawElements(geo.mode, geo.indices.size(), GL_UNSIGNED_INT, NULL);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	pop_program();
}

void imdraw::cube() {
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
	glBindVertexArray(vao);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glDrawElements(geo.mode, geo.indices.size(), GL_UNSIGNED_INT, NULL);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	pop_program();
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
	glBindVertexArray(vao);
	glDrawArrays(GL_LINES, 0, data.size());
	glBindVertexArray(0);
	pop_program();

	glDeleteBuffers(1, &vbo);
	glDeleteVertexArrays(1, &vao);
}

