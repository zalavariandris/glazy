#include "glazy.h"
#include "imdraw/imdraw.h"
#include "imdraw/imdraw_internal.h"

void run_draw_demo() {
	glazy::init();

	//// borderless window
	//GLFWmonitor* monitor = glfwGetPrimaryMonitor();
	//const GLFWvidmode* mode = glfwGetVideoMode(monitor);
	//glfwSetWindowMonitor(glazy::window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);

	//
	auto canvas_resolution = ImVec2(2048 * 2, 2048 * 2);
	auto nozzle_texture = imdraw::make_texture_from_file("C:/Users/andris/Desktop/nozzle.png");
	auto canvas_texture = imdraw::make_texture(canvas_resolution.x, canvas_resolution.y, NULL, GL_RGB, GL_RGB, GL_FLOAT);
	auto canvas_fbo = imdraw::make_fbo(canvas_texture);

	glBindFramebuffer(GL_FRAMEBUFFER, canvas_fbo);
	glClearColor(1.0, 1.0, 1.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glm::vec3 mouse_point;
	ImVec2 mouse;

	while (glazy::is_running()) {
		// Draw
		glazy::new_frame();
		auto viewport = ImGui::GetMainViewport();
		auto display_size = ImGui::GetIO().DisplaySize;

		ImGui::Text("%.1f fps", ImGui::GetIO().Framerate);

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

		// brush UI
		static float brush_color[4]{0,0,0,1};
		ImGui::ColorEdit4("color", brush_color);
		//ImGui::ColorButton("color", brush_color);
		static float brush_size{ 0.01 };
		ImGui::SliderFloat("brush size", &brush_size, 0.001, 0.1);

		// Update
		mouse = ImGui::GetMousePos();
		mouse_point = glazy::camera.screen_to_planeXY(mouse.x, mouse.y, display_size.x, display_size.y);
		static auto last_point = mouse_point;

		// Draw on canvas
		if (!ImGui::IsAnyItemActive()) {
			glBindFramebuffer(GL_FRAMEBUFFER, canvas_fbo);
			{
				glViewport(0, 0, canvas_resolution.x, canvas_resolution.y);
				
				imdraw::set_projection(glm::ortho(-1, 1, -1, 1, 0, 1));
				imdraw::set_view( glm::mat4(1) );

				//if (ImGui::IsMouseDown(0)) {

				float pressure = Stylus::GetPenPressure();
				pressure = 1.0;
				brush_size *= pressure;
				float stepping = std::min(1.0 / 2048, 0.18 * brush_size);

				if (ImGui::IsMouseClicked(0) && !ImGui::GetIO().KeyMods) {
					last_point = mouse_point;

					imdraw::disc(last_point, brush_size, { brush_color[0],brush_color[1],brush_color[2] });
				}

				if (ImGui::IsMouseDragging(0) && !ImGui::GetIO().KeyMods) {

					auto dir = mouse_point - last_point;
					auto length = glm::length(dir);
					auto step_vector = dir / length * stepping;
					// DRAW this in one call, by implementing instancing to imdraw::disc
					while (glm::distance(last_point, mouse_point) > stepping) {
						last_point += step_vector;

						imdraw::disc(last_point, brush_size, { brush_color[0],brush_color[1],brush_color[2] });
					}
				}
			}
		}

		// Draw to viewport
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		glViewport(0, 0, display_size.x, display_size.y);

		imdraw::set_projection(glazy::camera.getProjection());
		imdraw::set_view(glazy::camera.getView());

		imdraw::grid();
		imdraw::quad(canvas_texture);

		auto centers = std::vector<glm::vec3>();
		centers.push_back(mouse_point);
		imdraw::disc(centers, brush_size, { brush_color[0],brush_color[1],brush_color[2] });

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

			//imdraw::set_model(glm::translate(glm::mat4(1), { 0,0,0 }));
			//imdraw::set_color({ 1,1,1 });
			//imdraw::cube();

			//imdraw::set_color({ 1,1,1 }, 0.0);
			//imdraw::lines(
			//	{ {2,2,2}, {0,0,0} },
			//	{ {3,3,3}, {5,5,0} }
			//);

			//imdraw::plate()
			auto texture = imdraw::make_texture_from_file("C:/Users/andris/Pictures/50285493_806103886402081_6381557048001167360_n.jpg");
			imdraw::quad(texture);

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
