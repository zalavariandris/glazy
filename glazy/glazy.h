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
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_glfw.h"
#include "implot.h"

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
	// GLuint default_program;

	inline void glfw_error_callback(int error, const char* description) {
		std::cout << "GLFW error" << error << description << std::endl;
	}

	inline int init() {
		/***********
		  Init GLFW
		************/
		if (!glfwInit()) {
			std::cout << "Failed on initializing GLFW" << std::endl;
			return EXIT_FAILURE;
		}

		// configure
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
		//glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

		glfwSetErrorCallback(glfw_error_callback);

		// create main window
		window = glfwCreateWindow(1024, 768, "Demo: OpenGL Only", nullptr, nullptr);

		if (!window) {
			std::cout << "Window or context creation failed" << std::endl;
			EXIT_FAILURE;
		}

		glfwMakeContextCurrent(window);

		/***************************
		  GLAD GL extension library
		****************************/
		if (!gladLoadGL()) {
			std::cout << "GL init failed" << std::endl;
		};

		/********
		  STYLUS
		*********/ 
		Stylus::Init();

		/************
		  Init ImGui
		*************/
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		ImFontConfig cfg;
		

		ImGui_ImplGlfw_InitForOpenGL(window, true);
		ImGui_ImplOpenGL3_Init();

		cfg.SizePixels = 13;
		auto font_default = io.Fonts->AddFontDefault(&cfg);

		ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable; // enable docking
		
		ImPlot::CreateContext();

		/*************************
		  OPENGL RENDER SETTINGS
		**************************/
		static bool vsync=true;
		glfwSwapInterval(vsync ? 1 : 0);     // 0: vsync-off, 1: vsync-on
		static bool blend = true;
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		static bool blend_test = true;
		glEnable(GL_DEPTH_TEST);
		static bool culling = true;
		glEnable(GL_CULL_FACE);
		glCullFace(GL_FRONT);

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

	void new_frame(bool wait_events=false) {
		// poll events
		if (wait_events) {
			glfwWaitEvents();
		}
		else {
			glfwPollEvents();
		}

		/********
		  IMGUI
		*********/
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		
		ImGui::NewFrame();
	

		/********
		  STYLUS
		*********/
		Stylus::NewFrame();


		/*******************
		  CLEAR FRAMEBUFFER
		********************/
		int display_w, display_h;
		glfwGetFramebufferSize(glazy::window, &display_w, &display_h);

		glViewport(0, 0, display_w, display_h);
		glClearColor(0.1, 0.1, 0.1, 1.0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		/************ 
		  CREATE GUI 
		*************/
		ImGui::DockSpaceOverViewport();

		camera.aspect = (float)display_w / display_h;

		ImGui::Begin("camera");
		ImGui::Checkbox("ortho", &camera.ortho);
		ImGui::InputFloat("eye", &camera.eye.z);
		ImGui::End();
		//control_camera(camera, display_w, display_h);
	}

	void end_frame() {
		/* IMGUI */
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		ImGui::Render(); // render imgui
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData()); // draw imgui to screen

		/* SWAP BUFFERS */
		glfwSwapBuffers(window);
	}

	void destroy() {
		// - cleanup implot
		ImPlot::DestroyContext();

		// - cleanup imgui
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
		

		// - cleanup flfw
		glfwDestroyWindow(window);
		glfwTerminate();
	}
}