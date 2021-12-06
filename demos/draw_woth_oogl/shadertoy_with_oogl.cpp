// draw_woth_oogl.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include "glazy.h"
#include <glm/gtx/matrix_decompose.hpp>
#include "widgets/Viewport.h"



void run_tests() {
	std::cout << "running tests..." << std::endl;
	auto vertex_shader_code = glazy::read_text("C:/Users/andris/source/repos/glazy/glazy/shaders/default.vert");
	auto fragment_shader_code = glazy::read_text("C:/Users/andris/source/repos/glazy/glazy/shaders/default.frag");

	glazy::init();
	// smartg gl tests
	{
		auto tex = OOGL::Texture();
	}
	{
		auto vert = OOGL::Shader(GL_VERTEX_SHADER);
		std::cout << "is vert shader: " << (bool)glIsShader(vert) << std::endl;
		auto frag = OOGL::Shader(GL_FRAGMENT_SHADER);
		std::cout << "is frag shader: " << (bool)glIsShader(vert) << std::endl;
	}
	{
		auto progr = OOGL::Program();
	}

	{
		auto vert = OOGL::Shader(GL_VERTEX_SHADER);
		auto frag = OOGL::Shader(GL_FRAGMENT_SHADER);
		auto prog = OOGL::Program::from_shaders(vert, frag);
	}

	{
		auto vert = OOGL::Shader::from_source(GL_VERTEX_SHADER, vertex_shader_code.c_str());
		auto frag = OOGL::Shader::from_source(GL_FRAGMENT_SHADER, fragment_shader_code.c_str());
		auto prog = OOGL::Program::from_shaders(vert, frag);
	}

	{
		for (auto i = 0; i < 10; i++) {
			auto vert = OOGL::Shader::from_source(GL_VERTEX_SHADER, vertex_shader_code.c_str());
			auto frag = OOGL::Shader::from_source(GL_FRAGMENT_SHADER, fragment_shader_code.c_str());
			auto prog = OOGL::Program::from_shaders(vert, frag);
		}
	}
	glazy::destroy();
	std::cout << "done!" << std::endl;
}


int main()
{
	//run_tests();

    glazy::init();

	//glazy::destroy();
	//return 0;

	auto vertex_path = "C:/Users/andris/source/repos/glazy/glazy/shaders/default.vert";
	auto fragment_path = "C:/Users/andris/source/repos/glazy/glazy/shaders/default.frag";
	auto vertex_shader_code = glazy::read_text(vertex_path);
	auto fragment_shader_code = glazy::read_text(fragment_path);

	auto vertex_shader = OOGL::Shader::from_source(GL_VERTEX_SHADER, vertex_shader_code.c_str());
	auto fragment_shader = OOGL::Shader::from_source(GL_FRAGMENT_SHADER, fragment_shader_code.c_str());
	auto program = OOGL::Program::from_shaders(vertex_shader, fragment_shader);

	glazy::camera.eye = glm::vec3(-4, 4, -6);

	//auto main_resolution = ImVec2(1024, 768);
	GLuint main_color;
	GLuint main_depth;
	GLuint main_fbo=0;

	Camera main_camera;
	main_camera.ortho = false;
	main_camera.eye = glm::vec3(0, 1, -5);
	main_camera.target = glm::vec3(0, 0, 0);

    while (glazy::is_running()) {
        glazy::new_frame();

		ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove
			| ImGuiWindowFlags_NoDecoration
			| ImGuiWindowFlags_AlwaysAutoResize
			| ImGuiWindowFlags_NoSavedSettings
			| ImGuiWindowFlags_NoBackground
			| ImGuiWindowFlags_NoDocking;

		ImGui::SetNextWindowPos({ 0, 0 });
		ImGui::Begin("Stats", 0, flags);
		ImGui::Text("%f fps", ImGui::GetIO().Framerate);
		ImGui::End();

		ImGui::SetNextWindowPos({ 0,0 });
		auto viewport = ImGui::GetMainViewport();
		
		ImGui::SetNextWindowSize(viewport->Size);
		ImGui::Begin("background", 0, 
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoNav |
			ImGuiWindowFlags_NoBringToFrontOnFocus | 
			ImGuiWindowFlags_NoBackground | 
			ImGuiWindowFlags_NoDocking |
			ImGuiWindowFlags_NoDecoration |
			ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoMove
		);
		{
			ImGui::InvisibleButton("BACKGROUDN", ImVec2(-1, -1));
			auto size = ImGui::GetItemRectSize();
			auto camera = &glazy::camera;
			auto io = ImGui::GetIO();
			if (ImGui::IsItemActive()) {
				if (ImGui::IsItemActive()) {
					if (ImGui::IsMouseDragging(0) && (io.KeyMods == ImGuiKeyModFlags_Alt)) {
						camera->pan(-io.MouseDelta.x / size.x, -io.MouseDelta.y / size.y);
					}
					if (ImGui::IsMouseDragging(0) && (io.KeyMods == (ImGuiKeyModFlags_Ctrl | ImGuiKeyModFlags_Alt))) {
						camera->orbit(-io.MouseDelta.x * 0.006, -io.MouseDelta.y * 0.006);
					}
					if (ImGui::IsMouseDragging(0) && (io.KeyMods == ImGuiKeyModFlags_Ctrl)) {
						auto target_distance = camera->get_target_distance();
						camera->dolly(-io.MouseDelta.y * target_distance * 0.005);
					}
				}
			}

			if (ImGui::IsItemHovered()) {
				if (io.MouseWheel != 0 && !io.KeyMods) {
					auto target_distance = camera->get_target_distance();
					camera->dolly(-io.MouseWheel * target_distance * 0.2);
				}
			}
		}
		ImGui::End();

		ImGui::Begin("Viewport3D");
		{
			ViewportBegin(&main_fbo, &main_color, &main_depth, &main_camera);
			{
				glClearColor(0.3, .3, .3, 1);
				glClear(GL_COLOR_BUFFER_BIT);
				imdraw::set_projection(main_camera.getProjection());
				imdraw::set_view(main_camera.getView());
				imdraw::cube(glm::vec3(0), 1.0, glm::vec3(.8, .8, .8));


			}
			ViewportEnd();
		}
		ImGui::End();

		//
		//auto vert = imdraw::make_shader(GL_VERTEX_SHADER, vertex_shader_code.c_str());
		//auto frag = imdraw::make_shader(GL_FRAGMENT_SHADER, fragment_shader_code.c_str());
		//auto prog = imdraw::make_program_from_shaders(vert, frag);
		//glDeleteShader(vert);
		//glDeleteShader(frag);
		//glDeleteProgram(prog);

		//static auto init_ftime = std::filesystem::last_write_time(vertex_path);
		//auto ftime = std::filesystem::last_write_time(vertex_path);
		//if (ftime != init_ftime) {
		//	std::cout << "file changed" << std::endl;
		//	init_ftime = ftime;
		//}

		if (glazy::IsFileChanged(vertex_path)) {
			std::cout << "vertex changed" << std::endl;
		}

		//auto vert = OOGL::Shader::from_source(GL_VERTEX_SHADER, vertex_shader_code.c_str());
		//auto frag = OOGL::Shader::from_source(GL_FRAGMENT_SHADER, fragment_shader_code.c_str());
		//auto prog = OOGL::Program::from_shaders(vert, frag);
		
		// set program
		program.set_uniforms({
			{"projection", glazy::camera.getProjection()},
			{"view", glazy::camera.getView()},
			{"model", glm::mat4(1)},
			{"useInstanceMatrix", false},
			{"color", glm::vec3(1,1,0)},
			{"useTextureMap", false},
			{"textureMap", 0}
		});

		// create geometry
		static auto geo = imgeo::cube();
		static auto vbo = OOGL::ArrayBuffer::from_data(geo.positions);
		static auto vao = OOGL::VertexArray::from_attributes(program, {
			{"aPos",{vbo, 3}}
		});
		static auto ebo = OOGL::ElementBuffer::from_data(geo.indices);

		// draw
		glUseProgram(program);
		glClearColor(.5, .5, 1, 1);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glBindVertexArray(vao);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
		glDrawElements(geo.mode, geo.indices.size(), GL_UNSIGNED_INT, nullptr);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		glBindVertexArray(0);

		// 
		ImGui::Text("program ref count: %i", program.ref_count());
        glazy::end_frame();
    }
    glazy::destroy();
    std::cout << "Hello World!\n";
}