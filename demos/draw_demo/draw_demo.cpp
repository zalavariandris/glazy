#include "glazy.h"

int main(int argc, char* argv[]) {
	glazy::init();

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

			imdraw::set_model(glm::translate(glm::mat4(1), { 0,0,0 }));
			imdraw::set_color({ 1,1,1 });
			imdraw::cube();

			imdraw::set_model(glm::translate(glm::mat4(1), { 0,0,0 }));
			imdraw::set_color({ 1,0,1 }, 0.5);
			imdraw::grid();

			imdraw::set_color({ 1,1,1 }, 0.0);
			imdraw::lines(
				{ {2,2,2}, {0,0,0} },
				{ {3,3,3}, {5,5,0} }
			);
		}
		glazy::end_frame();
	}
	glazy::destroy();
}
