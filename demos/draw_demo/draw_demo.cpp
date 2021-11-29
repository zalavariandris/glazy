#include "glazy.h"
#include <deque>
#include "imdraw/imdraw.h"
#include "imdraw/imdraw_internal.h"

void ImGuiStats() {
	std::vector<float> x_data;
	static std::vector<float> fps_history;
	fps_history.push_back(ImGui::GetIO().Framerate);
	if (fps_history.size() >= 100) {
		fps_history.erase(fps_history.begin());
	}
	for (auto i = 0; i < fps_history.size(); i++) {
		x_data.push_back(i);
	}


	ImGui::Begin("Stats");
	ImGui::Text("%.1f fps", fps_history[0]);
	ImPlot::SetNextPlotLimits(0, 100, 0, 350);
	if (ImPlot::BeginPlot("fps history", "", "", ImVec2(-1, -1),
		ImPlotFlags_CanvasOnly, ImPlotAxisFlags_NoDecorations)) {
		ImPlot::PlotShaded("fps", x_data.data(), fps_history.data(), fps_history.size());
		ImPlot::EndPlot();
	}
	ImGui::End();
}

void ImGuiStylus() {
	ImGui::Begin("Stylus");
	{
		ImGui::Text("pen in range: %i", Stylus::IsPenInRange());
		ImGui::Text("pen is down: %i", Stylus::IsPenDown());
		ImGui::Text("pressure: %f", Stylus::GetPenPressure());

		for (const auto& [key, touch] : Stylus::GetTouches()) {
			ImGui::Text("touch: %i, (%.1f,%.1f) d(%.1f,%.1f)", key, touch.x, touch.y, touch.deltax, touch.deltay);
		}
	}
	ImGui::End();
}

std::vector<GLuint> framebuffer_stack;

void push_framebuffer(GLuint fbo) {
	GLint current_fbo = 0;
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &current_fbo);

	framebuffer_stack.push_back(current_fbo);

	if (fbo != current_fbo) {
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	}
}

GLuint pop_framebuffer() {
	assert(framebuffer_stack.size() > 0);

	GLint current_fbo = 0;
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &current_fbo);

	GLuint program = framebuffer_stack.back();
	if (program != current_fbo) {
		glBindFramebuffer(GL_FRAMEBUFFER, program);
	}
	framebuffer_stack.pop_back();
	return program;
}

void run_draw_demo() {
	glazy::init();

	//// borderless window
	//GLFWmonitor* monitor = glfwGetPrimaryMonitor();
	//const GLFWvidmode* mode = glfwGetVideoMode(monitor);
	//glfwSetWindowMonitor(glazy::window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);

	//
	
	auto nozzle_texture = imdraw::make_texture_from_file("C:/Users/andris/Desktop/nozzle.png");
	
	/* create canvas FBO */
	auto canvas_resolution = ImVec2(2048 * 2, 2048 * 2);
	auto canvas_texture = imdraw::make_texture(canvas_resolution.x, canvas_resolution.y, NULL, GL_RGB, GL_RGB, GL_FLOAT);
	auto canvas_fbo = imdraw::make_fbo(canvas_texture);

	// clear canvas to white
	glBindFramebuffer(GL_FRAMEBUFFER, canvas_fbo);
	glClearColor(1.0, 1.0, 1.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	/* create main FBO */
	auto main_resolution = ImVec2(1024, 768);
	auto main_texture = imdraw::make_texture(main_resolution.x, main_resolution.y, NULL, GL_RGB, GL_RGB, GL_FLOAT);
	auto main_fbo = imdraw::make_fbo(main_texture);

	// store mouse
	glm::vec3 mouse_point;
	ImVec2 mouse;

	while (glazy::is_running()) {
		// Draw
		glazy::new_frame();
		ImGuiStats();
		ImGuiStylus();

		// brush UI
		static float brush_size{ .11 };
		static float brush_color[4]{ 0,0,0,1 };
		ImGui::Begin("Brush");
		ImGui::SliderFloat("brush size", &brush_size, 0.001, 0.1);
		ImGui::ColorEdit4("color", brush_color);		
		ImGui::End();

		// Draw to main fbo
		ImGui::Begin("viewport");
		{
			// get mouse on plane
			mouse = ImGui::GetMousePos();

			ImGui::Button("screen", ImVec2(-1, -1));
			auto widget_size = ImGui::GetItemRectSize();
			glazy::camera.aspect = widget_size.x / widget_size.y;
			auto rect_min = ImGui::GetItemRectMin();
			auto rect_max = ImGui::GetItemRectMax();
			mouse_point = glazy::camera.screen_to_planeXY(mouse.x, mouse.y,
				{ rect_min.x, rect_max.y, widget_size.x, -widget_size.y });

			auto io = ImGui::GetIO();
			Camera& camera = glazy::camera;
			if (ImGui::IsItemActive()) {
				if (ImGui::IsMouseDragging(0) && io.KeyAlt) {
					camera.pan(-io.MouseDelta.x / widget_size.x, -io.MouseDelta.y / widget_size.y);
				}

				if (ImGui::IsMouseDragging(0) && io.KeyAlt && io.KeyCtrl) {
					camera.orbit(-io.MouseDelta.x * 0.006, -io.MouseDelta.y * 0.006);
				}
			}

			if (ImGui::IsItemHovered()) {
				if (io.MouseWheel != 0 && !io.KeyMods) {
					camera.dolly(-io.MouseWheel);
				}
			}

			// Draw on canvas
			if (ImGui::IsItemActive()) {
				// bind render target
				glBindFramebuffer(GL_FRAMEBUFFER, canvas_fbo);
				glViewport(0, 0, canvas_resolution.x, canvas_resolution.y);
				{
					
					imdraw::set_projection(glm::ortho(-1, 1, -1, 1, 0, 1));
					imdraw::set_view(glm::mat4(1));
					//---------------------------------------------------------

					float pressure = Stylus::GetPenPressure();
					//brush_size *= pressure;
					float stepping = std::min(1.0 / 2048, 0.18 * brush_size);
					static auto last_point = mouse_point;

					if (ImGui::IsMouseClicked(0) && !ImGui::GetIO().KeyMods) {
						last_point = mouse_point;
						imdraw::disc(last_point, brush_size, { brush_color[0],brush_color[1],brush_color[2] });
					}

					if (ImGui::IsMouseDragging(0) && !ImGui::GetIO().KeyMods) {
						auto dir = mouse_point - last_point;
						auto length = glm::length(dir);
						auto step_vector = dir / length * stepping;

						std::vector<glm::vec3> centers;
						while (glm::distance(last_point, mouse_point) > stepping) {
							last_point += step_vector;
							centers.push_back(last_point);
						}

						imdraw::disc(centers, brush_size, { brush_color[0],brush_color[1],brush_color[2] });
					}
				}
				// restore render target
				glBindFramebuffer(GL_FRAMEBUFFER, main_fbo);
				glViewport(0, 0, main_resolution.x, main_resolution.y);
			}

			// resize fbo
			static auto initial_window_size = ImGui::GetWindowSize();
			if (initial_window_size.x != ImGui::GetWindowSize().x || initial_window_size.y!=ImGui::GetWindowSize().y) {
				initial_window_size = ImGui::GetWindowSize();
				main_resolution = initial_window_size;

				// update fbo and attachments
				glDeleteTextures(1, &main_texture);
				glDeleteFramebuffers(1, &main_fbo);
				main_texture = imdraw::make_texture(main_resolution.x, main_resolution.y, NULL, GL_RGB, GL_RGB, GL_FLOAT);
				main_fbo = imdraw::make_fbo(main_texture);
			}

			//
			glClearColor(1.0, 1.0, 1.0, 1.0);
			glClear(GL_COLOR_BUFFER_BIT);
			glazy::camera.aspect = main_resolution.x / main_resolution.y;
			imdraw::set_projection(glazy::camera.getProjection());
			imdraw::set_view(glazy::camera.getView());
			// ---------------------------------------------------

			for (auto i = 0; i < 1; i++) {
				imdraw::grid();
			}
			imdraw::quad(canvas_texture);
			imdraw::disc(mouse_point, brush_size, { brush_color[0],brush_color[1],brush_color[2] });


			// display fbo in window and handle input
			ImGui::SetCursorPos(ImGui::GetWindowContentRegionMin());
			ImGui::Image((ImTextureID)main_texture, widget_size, ImVec2(0, 1), ImVec2(1, 0));
			
		}
		ImGui::End();

		// draw to screen
		//glBindFramebuffer(GL_FRAMEBUFFER, 0);
		//glViewport(0, 0, display_size.x, display_size.y);
		//imdraw::set_projection(glm::ortho(-1, 1, -1, 1, 0, 1));
		//imdraw::set_view(glm::mat4(1));
		//imdraw::quad(main_texture);

		glazy::end_frame();
	}
	glazy::destroy();
	//return SketchGL();
}

void run_imdraw_demo() {
	glazy::init(true, true);

	while (glazy::is_running()) {
		glazy::new_frame();
		{

			imdraw::set_projection(glazy::camera.getProjection());
			imdraw::set_view(glazy::camera.getView());
			/*
			imdraw::set_model(glm::translate(glm::mat4(1), { 0,0,0 }));
			imdraw::set_color({ 0,0,1 }, 0.0);
			imdraw::quad();

			imdraw::set_color({ 1,0,0 }, 0.5);
			imdraw::set_model(glm::translate(glm::mat4(1), { 0,1,0.0 }));
			imdraw::triangle();

			imdraw::set_model(glm::translate(glm::mat4(1), { 0,0,-1 }));
			imdraw::set_color({ 0,1,0 });
			imdraw::disc();

			imdraw::set_model(glm::translate(glm::mat4(1), { 0,2,0 }));
			imdraw::set_color({ 1,1,1 });
			imdraw::cross();
			*/

			glPointSize(4);
			{// set drawing mode
				const char* polygon_mode_items[] = { "TRIANGLE", "LINE", "POINT" };
				static int polygon_mode_current = 0;
				ImGui::ListBox("polygon mode", &polygon_mode_current, polygon_mode_items, IM_ARRAYSIZE(polygon_mode_items), 3);
				std::vector<GLenum> polygon_modes{ GL_FILL, GL_LINE, GL_POINT };
				glPolygonMode(GL_FRONT_AND_BACK, polygon_modes[polygon_mode_current]);
			}

			//auto texture = imdraw::make_texture_from_file("C:/Users/andris/Pictures/50285493_806103886402081_6381557048001167360_n.jpg");
			//imdraw::quad(texture);

			imdraw::grid();
		}
		glazy::end_frame();
	}
	glazy::destroy();
}

int main(int argc, char* argv[]) {
	run_draw_demo();
	//run_imdraw_demo();
}
