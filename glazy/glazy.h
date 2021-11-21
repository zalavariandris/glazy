#pragma once

// Logging
#include <iostream>

// FileSystem
#include <fstream>
#include <sstream>

// collections
#include <vector>
#include <tuple>
#include <map>

// OpenGL
#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

// imgui
#include "imgui_impl_opengl3.h"
#include "imgui_impl_glfw.h"

// glm
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

//
#include "Camera.h"
#include "Stylus.h"

#include "imdraw/imdraw.h"
#include "imdraw/imdraw_internal.h"

void control_camera(Camera& camera, int scree_width, int screen_height) {
	auto io = ImGui::GetIO();
	// control camera
	if (!ImGui::IsAnyItemHovered()) {

		if (ImGui::IsMouseDragging(0) && io.KeyAlt) {
			camera.pan(-io.MouseDelta.x / scree_width, -io.MouseDelta.y / screen_height);
		}

		if (ImGui::IsMouseDragging(0) && io.KeyAlt && io.KeyCtrl) {
			camera.orbit(-io.MouseDelta.x * 0.006, -io.MouseDelta.y * 0.006);
		}

		if (io.MouseWheel != 0 && !io.KeyMods) {
			camera.dolly(-io.MouseWheel);
		}

		bool UseTouchForNavigation = true;
		if (UseTouchForNavigation) {
			if (Stylus::IsAnyTouchDown()) {

			}

			if (Stylus::TouchDown(2)) {
			}

			if (Stylus::TouchDragging(2)) {

				camera.pan(-Stylus::GetTouchDeltaX() / scree_width, -Stylus::GetTouchDeltaY() / screen_height);
			}

			if (Stylus::TouchUp(2)) {
			}

			if (Stylus::GetTouchZoomDelta() != 1.0) {
				auto d = glm::distance(camera.eye, camera.target);
				camera.dolly((Stylus::GetTouchZoomDelta() - 1.0) * -1.5);
			}
		}
	}
}

namespace glazy {
	GLFWwindow* window;
	Camera camera;
	GLuint default_program;

	//inline void gizmo(const glm::vec3 * center, float size=0.3) {
	//	static std::vector<glm::vec3> vertices{
	//			{-1,0,0}, {1,0,0},
	//			{0, -1, 0}, {0,1,0}
	//	};
	//	static std::vector<glm::uvec2> indices{
	//			{0,1},
	//			{2,3}
	//	};

	//	// create vao
	//	static auto vao = []() {
	//		GLuint vao;
	//		glGenVertexArrays(1, &vao);
	//		glBindVertexArray(vao);

	//		GLuint vbo;
	//		glGenBuffers(1, &vbo);
	//		glBindBuffer(GL_ARRAY_BUFFER, vbo);
	//		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), glm::value_ptr(vertices[0]), GL_STATIC_DRAW);

	//		glEnableVertexAttribArray(0);
	//		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

	//		GLuint ebo;
	//		glGenBuffers(1, &ebo);
	//		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	//		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(glm::uvec4), glm::value_ptr(indices[0]), GL_STATIC_DRAW);

	//		glBindVertexArray(0);
	//		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	//		glBindBuffer(GL_ARRAY_BUFFER, 0);

	//		return vao;
	//	}();

	//	auto M = glm::mat4(1);
	//	M = glm::translate(M, { *center });
	//	M = glm::scale(M, glm::vec3(size));
	//	imdraw::set_program_uniform(default_program, "model", M);

	//	auto data_length = indices.size() * 3;
	//	glBindVertexArray(vao);
	//	glDrawElements(GL_LINES, data_length, GL_UNSIGNED_INT, NULL);
	//	glBindVertexArray(0);
	//}

	//inline void draw_cross(glm::vec3 center, float size) {
	//	static std::vector<glm::vec3> vertices{
	//			{-1,0,0}, {1,0,0},
	//			{0, -1, 0}, {0,1,0}
	//		};
	//	static std::vector<glm::uvec2> indices{
	//			{0,1},
	//			{2,3}
	//		};

	//	// create vao
	//	static auto vao = [](){
	//		GLuint vao;
	//		glGenVertexArrays(1, &vao);
	//		glBindVertexArray(vao);

	//		GLuint vbo;
	//		glGenBuffers(1, &vbo);
	//		glBindBuffer(GL_ARRAY_BUFFER, vbo);
	//		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), glm::value_ptr(vertices[0]), GL_STATIC_DRAW);

	//		glEnableVertexAttribArray(0);
	//		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

	//		GLuint ebo;
	//		glGenBuffers(1, &ebo);
	//		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	//		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(glm::uvec4), glm::value_ptr(indices[0]), GL_STATIC_DRAW);

	//		glBindVertexArray(0);
	//		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	//		glBindBuffer(GL_ARRAY_BUFFER, 0);

	//		return vao;
	//	}();

	//	auto M = glm::mat4(1);
	//	M = glm::translate(M, { center });
	//	M = glm::scale(M, glm::vec3(size));
	//	imdraw::set_program_uniform(default_program, "model", M);

	//	auto data_length = indices.size() * 3;
	//	glBindVertexArray(vao);
	//	glDrawElements(GL_LINES, data_length, GL_UNSIGNED_INT, NULL);
	//	glBindVertexArray(0);
	//}

	//inline void draw_grid() {
	//	// Create GEO
	//	static auto geo = []() {
	//		// create geo
	//		std::vector<glm::vec3> vertices;
	//		std::vector<glm::uvec4> indices;

	//		static int slices{ 10 };

	//		for (int j = 0; j <= slices; ++j) {
	//			for (int i = 0; i <= slices; ++i) {
	//				float x = (float)i / (float)slices - 0.5;
	//				float z = 0;
	//				float y = (float)j / (float)slices - 0.5;
	//				x *= slices;
	//				y *= slices;
	//				vertices.push_back(glm::vec3(x, y, z));
	//			}
	//		}

	//		for (int j = 0; j < slices; ++j) {
	//			for (int i = 0; i < slices; ++i) {

	//				int row1 = j * (slices + 1);
	//				int row2 = (j + 1) * (slices + 1);

	//				indices.push_back(glm::uvec4(row1 + i, row1 + i + 1, row1 + i + 1, row2 + i + 1));
	//				indices.push_back(glm::uvec4(row2 + i + 1, row2 + i, row2 + i, row1 + i));
	//			}
	//		}

	//		return std::tuple<std::vector<glm::vec3>, std::vector<glm::uvec4>>( vertices, indices );
	//	}();

	//	// Create VAO
	//	static unsigned int vao = [](std::vector<glm::vec3> vertices, std::vector<glm::uvec4> indices) {
	//		unsigned int vao;
	//		glGenVertexArrays(1, &vao);
	//		glBindVertexArray(vao);

	//		GLuint vbo;
	//		glGenBuffers(1, &vbo);
	//		glBindBuffer(GL_ARRAY_BUFFER, vbo);
	//		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), glm::value_ptr(vertices[0]), GL_STATIC_DRAW);
	//		glEnableVertexAttribArray(0);
	//		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

	//		GLuint ibo;
	//		glGenBuffers(1, &ibo);
	//		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
	//		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(glm::uvec4), glm::value_ptr(indices[0]), GL_STATIC_DRAW);

	//		glBindVertexArray(0);
	//		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	//		glBindBuffer(GL_ARRAY_BUFFER, 0);

	//		return vao;
	//	}(std::get<0>(geo), std::get<1>(geo));

	//	// shader program
	//	// use default shder program
	//	imdraw::set_program_uniform(default_program, "model", glm::mat4(1));

	//	// draw grid
	//	auto data_length = std::get<1>(geo).size() * 4;
	//	glBindVertexArray(vao);
	//	glDrawElements(GL_LINES, data_length, GL_UNSIGNED_INT, NULL);
	//	glBindVertexArray(0);
	//}

	//inline void draw_line() {

	//}

	inline void glfw_error_callback(int error, const char* description) {
		std::cout << "GLFW error" << error << description << std::endl;
	}

	inline int init() {
		/*********************************
		INIT glfw and GL extension library
		**********************************/

		// init GLFW
		if (!glfwInit()) {
			std::cout << "Failed on initializing GLFW" << std::endl;
			return EXIT_FAILURE;
		}

		// configure GLFW
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
		//glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

		// log errors
		glfwSetErrorCallback(glfw_error_callback);

		// create main window
		window = glfwCreateWindow(1024, 768, "Demo: OpenGL Only", nullptr, nullptr);

		if (!window) {
			std::cout << "Window or context creation failed" << std::endl;
			EXIT_FAILURE;
		}

		glfwMakeContextCurrent(window);

		glfwPollEvents();
		//glfwWaitEvents();

		// load extension library
		if (!gladLoadGL()) {
			std::cout << "GL init failed" << std::endl;
		};

		// STYLUS
		Stylus::Init();

		/*****************
		* Configure OpenGL
		****************/
		glfwSwapInterval(0);     // 0: vsync-off, 1: vsync-on
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_DEPTH_TEST);

		/************
		  Init ImGui
		*************/
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui_ImplGlfw_InitForOpenGL(window, true);
		ImGui_ImplOpenGL3_Init();
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		auto font_default = io.Fonts->AddFontDefault();

		/*****************
		* Shader Program *
		******************/
		default_program = imdraw::make_program_from_source(
			R"(#version 330 core
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

			)", 
			R"(#version 330 core
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

		//std::cout << "new program id: " << new_program.id() << " is valid: " << (glIsProgram(new_program.id()) ? "true" : "false") << std::endl;
		//
		//default_program = new_program;

		return EXIT_SUCCESS;
	}

	/**
	* Check windows
	*
	* @deprecated since 1.0
	* @see is_running()
	*/
	bool window_should_close() {
		return glfwWindowShouldClose(window);
	}

	bool is_running() {
		return !glfwWindowShouldClose(window);
	}

	void new_frame() {
		// poll events
		glfwPollEvents();
		//glfwWaitEvents();

		// Start the ImGui frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		Stylus::NewFrame();

		// setup viewport
		int display_w, display_h;
		glfwGetFramebufferSize(glazy::window, &display_w, &display_h);

		glViewport(0, 0, display_w, display_h);
		glClearColor(0.1, 0.1, 0.1, 1.0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// update camera
		camera.aspect = (float)display_w / display_h;

		ImGui::Begin("camera");
		ImGui::Checkbox("ortho", &camera.ortho);
		ImGui::InputFloat("eye", &camera.eye.z);
		ImGui::End();
		control_camera(camera, display_w, display_h);

		/* Shader Progam */
		glUseProgram(default_program);

		imdraw::set_uniforms(default_program, {
			{"projection", camera.getProjection()},
			{ "view", camera.getView()},
			{ "model", glm::mat4(1) },
			{ "useInstanceMatrix", false },
			{ "useTextureMap", false },
			{ "color", glm::vec3(0.9, 0.2, 0.9) }
			});

	}

	void end_frame() {
		ImGui::Render(); // render imgui
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData()); // draw imgui to screen

		glfwSwapBuffers(window);
	}

	void destroy() {
		// - cleanup imgui
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();

		// - cleanup flfw
		glfwDestroyWindow(window);
		glfwTerminate();
	}
}