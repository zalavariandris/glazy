#pragma once

// Logging
#include <iostream>

// FileSystem
#include <fstream>
#include <sstream>
#include <filesystem>

// collections
#include <vector>
#include <tuple>
#include <map>
#include <array>

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
#include "imgui_internal.h"

// glm
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

//
#include "Camera.h"
#include "Stylus.h"

// imgeo
#include "imgeo/imgeo.h"

// imdraw
#include "imdraw/imdraw.h"
#include "imdraw/imdraw_internal.h"
#include "imdraw/imdraw_geo.h"

// OOGL
#include "OOGL/Shader.h"
#include "OOGL/Program.h"
#include "OOGL/ArrayBuffer.h"
#include "OOGL/ElementBuffer.h"
#include "OOGL/VertexArray.h"
#include "OOGL/Texture.h"

// Icon Fonts
#include "IconsFontAwesome5.h"

#include "../tracy/Tracy.hpp"

void glPrintErrors()
{
	GLenum err(glGetError());

	while (err != GL_NO_ERROR) {
		std::string error;

		switch (err) {
		case GL_INVALID_OPERATION:      error = "INVALID_OPERATION";      break;
		case GL_INVALID_ENUM:           error = "INVALID_ENUM";           break;
		case GL_INVALID_VALUE:          error = "INVALID_VALUE";          break;
		case GL_OUT_OF_MEMORY:          error = "OUT_OF_MEMORY";          break;
		case GL_INVALID_FRAMEBUFFER_OPERATION:  error = "INVALID_FRAMEBUFFER_OPERATION";  break;
		}

		std::cerr << "GL_" << error.c_str() << "\n";
		err = glGetError();
	}
}

namespace ImGui
{
	static auto vector_getter = [](void* vec, int idx, const char** out_text)
	{
		auto& vector = *static_cast<std::vector<std::string>*>(vec);
		if (idx < 0 || idx >= static_cast<int>(vector.size())) { return false; }
		*out_text = vector.at(idx).c_str();
		return true;
	};

	bool Combo(const char* label, int* currIndex, std::vector<std::string>& values)
	{
		if (values.empty()) { return false; }
		return Combo(label, currIndex, vector_getter,
			static_cast<void*>(&values), values.size());
	}

	bool ListBox(const char* label, int* currIndex, std::vector<std::string>& values)
	{
		if (values.empty()) { return false; }
		return ListBox(label, currIndex, vector_getter,
			static_cast<void*>(&values), values.size());
	}
}

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
#include <chrono>


namespace glazy {
	GLFWwindow* window;
	Camera camera;
	GLuint checkerboard_tex;

	std::chrono::steady_clock::time_point g_Time;
	float DeltaTime; // in seconds
	float Framerate; // frames per sec


	// GLuint default_program;

	inline void glfw_error_callback(int error, const char* description) {
		std::cout << "GLFW error" << error << description << std::endl;
	}

	void ApplyMyThemed();

	/*
	https://github.com/ocornut/imgui/issues/707#issuecomment-463758243
	*/
	void ApplyPhotoshopTheme();

	void ApplyGameEngineTheme();

	void ApplyVisualStudioTheme();

	void ApplyEmbraceTheDarknessTheme();

	void set_vsync(bool enable) {
		glfwSwapInterval(enable ? 1 : 0);
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
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
		//glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
		glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_FALSE);

		

		glfwSetErrorCallback(glfw_error_callback);

		// create main window
		//glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);

		window = glfwCreateWindow(1024, 768, "MiniViewer", nullptr, nullptr);

		auto hwd = glfwGetWin32Window(glazy::window);
		
		if (!window) {
			std::cout << "Window or context creation failed" << std::endl;
			EXIT_FAILURE;
		}

		// center window
		auto monitor = glfwGetPrimaryMonitor();
		float xscale; float yscale;
		glfwGetMonitorContentScale(monitor, &xscale, &yscale);
		const GLFWvidmode* mode = glfwGetVideoMode(monitor);
		auto width = mode->width * 3 / 4;
		auto height = mode->height * 3 / 4;
		glfwSetWindowSize(window, width, height);
		glfwSetWindowPos(window, (mode->width-width)/2, (mode->height-height)/2);

		glfwMakeContextCurrent(window);

		/***************************
		  GLAD GL extension library
		****************************/
		if (!gladLoadGL()) {
			std::cout << "GL init failed" << std::endl;
		};

		std::cout << "using opengl " << glGetString(GL_VERSION) << "\n";



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
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // enable docking
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // enable multiviewports
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // enable keyboard navigation
		
		
		ImGuiStyle& style = ImGui::GetStyle();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
			style.WindowRounding = 0.0f;
			style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		}

		ImGui_ImplGlfw_InitForOpenGL(window, true);
		ImGui_ImplOpenGL3_Init();

		/// Setup fonts
		ImFontConfig config;
		config.OversampleV = 8;
		config.OversampleH = 8;
		config.SizePixels = 16*xscale;
		//auto font_default = io.Fonts->AddFontDefault(&cfg);
		//io.Fonts->AddFontFromFileTTF("C:/WINDOWS/FONTS/ARIAL.ttf", 13*xscale);
		std::cout << "dpi scale: " << xscale << std::endl;
		io.Fonts->AddFontFromFileTTF("C:/WINDOWS/FONTS/SEGUIVAR.ttf", 16.0f * xscale, &config);
		// the first loaded font gets used by default

		// Merge icon font
		config.MergeMode = true;
		config.GlyphMinAdvanceX = 16.0f * xscale; // Use if you want to make the icon monospaced
		config.PixelSnapH = true;
		static const ImWchar icon_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
		io.Fonts->AddFontFromFileTTF("../../glazy/" FONT_ICON_FILE_NAME_FAS, 12.0f * xscale, &config, icon_ranges); // Merge icon font
		//io.Fonts->AddFontFromFileTTF("C:/Users/andris/source/repos/glazy/glazy/Font Awesome 5 Free-Regular-400.otf", 16.0f * xscale, &config, icon_ranges); // Merge icon font

		ImGui::GetStyle().ScaleAllSizes(xscale);

		//io.Fonts->GetTexDataAsRGBA32();// or GetTexDataAsAlpha8()

		
		
		ImPlot::CreateContext();

		/*************************
		  OPENGL RENDER SETTINGS
		**************************/
		static bool vsync=true;
		glfwSwapInterval(vsync ? 1 : 0);     // 0: vsync-off, 1: vsync-on
		//static bool blend = true;
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		//static bool blend_test = true;
		//glEnable(GL_DEPTH_TEST);
		//static bool culling = true;
		//glEnable(GL_CULL_FACE);
		//glCullFace(GL_FRONT);

		// camera defaults
		camera.ortho = false;
		camera.eye = glm::vec3(0, 1, -5);
		camera.target = glm::vec3(0, 0, 0);

		/// Generate builtin textures
		
		checkerboard_tex = imdraw::make_texture_float(64, 64, NULL, GL_RGBA);
		GLuint fbo = imdraw::make_fbo(checkerboard_tex);
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
		glClearColor(1.0,1.0,1.0, 1);
		glClear(GL_COLOR_BUFFER_BIT);
		glViewport(0, 0, 64, 64);
		imdraw::set_projection(glm::ortho(-1, 1, -1, 1));
		imdraw::set_view(glm::mat4(1));
		imdraw::quad(0, { -1,-1 }, { 0,0 });
		imdraw::quad(0, { 0,0 }, { 1,1 });
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		// set dark style
		//ApplyPhotoshopTheme();
		//ApplyVisualStudioTheme();
		//ApplyGameEngineTheme();
		//ApplyEmbraceTheDarknessTheme();

		ApplyEmbraceTheDarknessTheme();

		std::cout << "<glazy init errors> " << "\n";
		glPrintErrors();
		std::cout << "</end of glazy errors>" << "\n";
		return EXIT_SUCCESS;
	}

	/**
	* return empty string if cancelled
	*/
	std::filesystem::path open_file_dialog(const char* filter) {
		OPENFILENAMEA ofn;
		CHAR szFile[260] = { 0 };
		ZeroMemory(&ofn, sizeof(OPENFILENAME));
		ofn.lStructSize = sizeof(OPENFILENAME);
		ofn.hwndOwner = glfwGetWin32Window((GLFWwindow*)glazy::window);
		ofn.lpstrFile = szFile;
		ofn.nMaxFile = sizeof(szFile);
		ofn.lpstrFilter = filter;
		ofn.nFilterIndex = 1;
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
		if (GetOpenFileNameA(&ofn) == true)
		{
			return ofn.lpstrFile;
		}
		return std::string();
	}

	/// <summary>
	/// Returns true when the file has changed since last frame
	/// changes are kept for mutliple calls. And renew new frame
	/// </summary>
	std::map<std::filesystem::path, std::filesystem::file_time_type> mod_times;
	bool is_file_modified(const std::filesystem::path& path) {
		if (!mod_times.contains(path)) {
			mod_times[path] = std::filesystem::last_write_time(path);
			return true;
		}
		if (mod_times[path] != std::filesystem::last_write_time(path)) {
			return true;
		}
		return false;
	}

	/// <summary>
	/// private function. clear mod changes
	/// </summary>
	void _clear_mod_flags() {
		for (const auto& [path, timestamp] : mod_times) {
			mod_times[path] = std::filesystem::last_write_time(path);
		}
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

	void quit() {
		glfwSetWindowShouldClose(window, 1);
	}

	void new_frame(bool wait_events = false)
	{
		ZoneScoped;
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


		// calc framerate
		auto current_time = std::chrono::steady_clock::now();
		auto dt = current_time - g_Time;
		DeltaTime = std::chrono::duration_cast<std::chrono::milliseconds>(dt).count() / 1000.0;
		Framerate = 1.0 / DeltaTime;
		g_Time = current_time;

		//
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
		ImGui::DockSpaceOverViewport(NULL, ImGuiDockNodeFlags_PassthruCentralNode);

		camera.aspect = (float)display_w / display_h;

		//control_camera(camera, display_w, display_h);
	}

	void end_frame()
	{
		ZoneScoped;
		// glazy windows
		{
			static bool themes;
			static bool stats;
			static bool imgui_demo;
			static bool imgui_style;
			static bool fullscreen;
			if (ImGui::BeginMainMenuBar()) {
				auto left_cursor = ImGui::GetCursorPos(); // store cursor
				ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x-(ImGui::CalcTextSize("glazy").x + 60 + ImGui::CalcTextSize("99.00fps").x + ImGui::CalcTextSize(ICON_FA_EXPAND).x+2*ImGui::GetStyle().ItemSpacing.x*4));

				if (ImGui::BeginMenu("glazy"))
				{
					ImGui::Separator();
					ImGui::Text("Windows");
					ImGui::Separator();
					ImGui::MenuItem("themes", "", &themes);
					ImGui::MenuItem("stats", "", &stats);
					ImGui::MenuItem("imgui demo", "", &imgui_demo);
					ImGui::MenuItem("imgui style", "", &imgui_style);
					if (ImGui::MenuItem("quit")) {
						quit();
					}
					ImGui::EndMenu();
				}

				// full screen toggle
				if (ImGui::MenuItem(fullscreen ? ICON_FA_COMPRESS_ALT : ICON_FA_EXPAND_ALT, "", &fullscreen)) {
					std::cout << "toggle fullscreen" << "\n";
					static std::array< int, 2 > _wndPos{ 0, 0 };
					static std::array< int, 2 > _wndSize{ 0, 0 };
					if (fullscreen) {
						auto monitor = glfwGetPrimaryMonitor();
						const GLFWvidmode* mode = glfwGetVideoMode(monitor);
						glfwGetWindowPos(window, &_wndPos[0], &_wndPos[1]);
						glfwGetWindowSize(window, &_wndSize[0], &_wndSize[1]);
						glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
					}
					else {
						glfwSetWindowMonitor(window, nullptr, _wndPos[0], _wndPos[1], _wndSize[0], _wndSize[1], 0);
					}
				}

				// Show fps in the right side
				{
					auto ctx = ImGui::GetCurrentContext();
					float* dts = ctx->FramerateSecPerFrame;
					auto offset = ctx->FramerateSecPerFrameIdx;

					float fpss[120];
					for (auto i = 0; i < 120; i++) fpss[i] = 1.0 / dts[i];
					auto avg_fps = ImGui::GetIO().Framerate;
					ImGui::PlotHistogram("fps", fpss, 120, offset,std::to_string((int)avg_fps).c_str(), 0, 120, {60,0});

					std::stringstream ss; // compose text to calculate size
					//ss << std::fixed << std::setprecision(0) << 1.0/ImGui::GetIO().DeltaTime << "fps";
					ss << std::fixed << std::setprecision(0) << Framerate << "fps";
					ImGui::Text(ss.str().c_str());
				}

				// restore cursor to the left side
				ImGui::SetCursorPos(left_cursor); 

				// end menubar
				ImGui::EndMainMenuBar();
			}

			// Show stats
			if (stats && ImGui::Begin("stats", &stats)) {
				std::vector<float> x_data;
				static std::vector<float> fps_history;

				float framerate = ImGui::GetIO().Framerate;

				fps_history.push_back(framerate);
				if (fps_history.size() >= 300) {
					fps_history.erase(fps_history.begin());
				}
				for (auto i = 0; i < fps_history.size(); i++) {
					x_data.push_back(i);
				}

				static float low_limit = 0;
				static float high_limit = 200;
				high_limit = std::max(high_limit, framerate);

				ImGui::Text("%.1f fps", fps_history[0]);
				ImPlot::SetNextPlotLimits(0, fps_history.size(), low_limit, high_limit, ImGuiCond_Always);

				ImGui::PlotLines("fps", fps_history.data(), fps_history.size(), 0, "", 0, 120);

				ImGui::End();


			}

			// Show themes window
			if (themes && ImGui::Begin("themes", &themes)) {
				static int current_item = 0;
				if (ImGui::Combo("themes", &current_item, "photoshop\0VS\0game\0darkness")) {
					switch (current_item) {
					case 0:
						ApplyPhotoshopTheme();
						break;

					case 1:
						ApplyVisualStudioTheme();
						break;

					case 2:
						ApplyGameEngineTheme();
						break;

					case 3:
						ApplyEmbraceTheDarknessTheme();
						break;
					}
				}
				ImGui::End();
			}

			if (imgui_demo) {
				ImGui::ShowDemoWindow(&imgui_demo);
			}
			if (imgui_style) {
				ImGui::Begin("StyleEditor", &imgui_style);
				ImGui::ShowStyleEditor();
				ImGui::End();
			}
		}

		/* IMGUI */
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		ImGui::Render(); // render imgui
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData()); // draw imgui to screen

		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
			GLFWwindow* backup_current_context = glfwGetCurrentContext();
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
			glfwMakeContextCurrent(backup_current_context);
		}

		glazy::_clear_mod_flags(); // this must be before swap buffers? I dont know why... TODO: FigureOut

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

	inline std::string read_text(const char* path) {
		std::string content;
		std::cout << "read: " << path << std::endl;
		std::string code;
		std::ifstream file(path, std::ios::in);
		std::stringstream stream;

		if (!file.is_open()) {
			std::cerr << "Could not read file " << path << ". File does not exist." << std::endl;
		}

		std::string line = "";
		while (!file.eof()) {
			std::getline(file, line);
			content.append(line + "\n");
		}
		file.close();
		return content;
	}
}