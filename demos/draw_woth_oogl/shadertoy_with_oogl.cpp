// draw_woth_oogl.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include "glazy.h"
#include <glm/gtx/matrix_decompose.hpp>
#include "widgets/Viewport.h"
#include <filesystem>
#include "imgui_stdlib.h"

// assimp: model loading
#include "assimp/Importer.hpp"
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <deque>

glm::mat4 assimp_to_glm(const aiMatrix4x4& aiMat)
{
	return {
		aiMat.a1, aiMat.b1, aiMat.c1, aiMat.d1,
		aiMat.a2, aiMat.b2, aiMat.c2, aiMat.d2,
		aiMat.a3, aiMat.b3, aiMat.c3, aiMat.d3,
		aiMat.a4, aiMat.b4, aiMat.c4, aiMat.d4
	};
}

namespace imgeo {
	struct Node {
		std::vector<Node*> children;
		glm::mat4 transform=glm::mat4(1);
		std::vector<Trimesh> geos;
		std::string name = "";
	};
}

bool load_model(const std::filesystem::path & path) {
	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(path.string(), aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenNormals);

	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
	{
		std::cout << "ERROR::ASSIMP::" << importer.GetErrorString() << std::endl;
		return false;
	}


	/**
	Collect all nodes
	*/
	std::vector<std::pair<aiNode*, imgeo::Node>> node_pairs;

	// depth first search
	std::cout << "DFS: " << path << " model: " << std::endl;
	std::deque<aiNode*> nodes_to_visit{scene->mRootNode};
	std::vector<aiNode* , imgeo::Node> node_pairs;
	while (nodes_to_visit.size() > 0) {
		aiNode* ainode = nodes_to_visit.back();
		nodes_to_visit.pop_back();
		// prepend children
		for (unsigned int i = 0; i < ainode->mNumChildren; i++)
		{
			nodes_to_visit.push_front(ainode->mChildren[i]);
		}

		/*
		do something with current node
		*/
		imgeo::Node mynode;
		for (auto i = 0; ainode->mNumChildren; i++) {
			mynode.children.push_back()
		}
		node_pairs.push_back({ ainode, mynode });
	}
	for(auto & node_pair : node_pairs) {
		auto current_node = std::get<0>(node_pair);
		auto my_node = std::get<0>(node_pair);
			mynode.name = current_node->mName.C_Str();
		mynode.transform = assimp_to_glm(current_node->mTransformation);
		//TODO: mynode.parent = ?

		for (auto i = 0; i < current_node->mNumMeshes; i++) {
			aiMesh* mesh = scene->mMeshes[current_node->mMeshes[i]];

			// process mesh
			std::vector<glm::vec3> positions;
			if (mesh->HasPositions())
			{
				for (auto i = 0; i < mesh->mNumVertices; i++) {
					auto P = mesh->mVertices[i];
					glm::vec3 pos{ P.x, P.y, P.z };
					positions.push_back(pos);
				}
			}
			std::vector<glm::vec3> normals;

			if (mesh->HasNormals())
			{
				for (auto i = 0; i < mesh->mNumVertices; i++) {
					auto N = mesh->mNormals[i];
					glm::vec3 normal{ N.x, N.y, N.z };
					normals.push_back(normal);
				}
			}

			std::vector<glm::vec2> uvs;
			std::vector<glm::vec3> tangents;
			std::vector<glm::vec3> bitangents;
			if (mesh->mTextureCoords[0]) // has UV
			{
				for (auto i = 0; i < mesh->mNumVertices; i++) {
					auto texture_coords = mesh->mTextureCoords[0][i]; // TODO: multiple texture coordinates
					glm::vec2 uv{ texture_coords.x, texture_coords.y };
					uvs.push_back(uv);

					auto T = mesh->mTangents[i];
					glm::vec3 tangent{ T.x, T.y, T.z };
					tangents.push_back(tangent);

					auto bT = mesh->mBitangents[i];
					glm::vec3 biTangent{ bT.x, bT.y, bT.z };
					tangents.push_back(biTangent);
				}
			}

			// indices
			std::vector<unsigned int> indices;
			for (auto i = 0; i < mesh->mNumFaces; i++) {
				auto face = mesh->mFaces[i];
				for (auto j = 0; j < face.mNumIndices; j++) {
					auto vertex_index = face.mIndices[j];
					indices.push_back(vertex_index);
				}
			}

			auto geo = imgeo::Trimesh(GL_TRIANGLES,
				positions,
				indices,
				uvs.size() > 0 ? std::optional<std::vector<glm::vec2>>(uvs) : std::nullopt,
				normals.size() > 0 ? std::optional<std::vector<glm::vec3>>(normals) : std::nullopt,
				std::nullopt
			);

			geometries.push_back(geo);
		}
	}

	return true;
}

void run_tests() {
	std::cout << "running tests..." << std::endl;
	
	auto current_dir = std::filesystem::current_path();
	auto glazy_dir = current_dir.parent_path().parent_path();

	auto vertex_path = glazy_dir / "glazy" / "shaders" / "default.vert";
	auto fragment_path = glazy_dir / "glazy" / "shaders" / "default.frag";
	auto vertex_shader_code = glazy::read_text(vertex_path.string().c_str());
	auto fragment_shader_code = glazy::read_text(fragment_path.string().c_str());

	glazy::init();
	// smartg gl tests
	{
		auto tex = OOGL::Texture();
		std::cout << "is texture: " << (bool)glIsTexture(tex) << std::endl;
	}
	{
		auto vert = OOGL::Shader(GL_VERTEX_SHADER);
		std::cout << "is vert shader: " << (bool)glIsShader(vert) << std::endl;
		auto frag = OOGL::Shader(GL_FRAGMENT_SHADER);
		std::cout << "is frag shader: " << (bool)glIsShader(vert) << std::endl;
	}
	{
		auto prog = OOGL::Program();
		std::cout << "is program: " << (bool)glIsProgram(prog) << std::endl;
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

		std::cout << "is vert shader: " << (bool)glIsShader(vert) << std::endl;
		std::cout << "is frag shader: " << (bool)glIsShader(vert) << std::endl;
		std::cout << "is program: " << (bool)glIsProgram(prog) << std::endl;
	}

	{
		for (auto i = 0; i < 10; i++) {
			auto vert = OOGL::Shader::from_source(GL_VERTEX_SHADER, vertex_shader_code.c_str());
			auto frag = OOGL::Shader::from_source(GL_FRAGMENT_SHADER, fragment_shader_code.c_str());
			auto prog = OOGL::Program::from_shaders(vert, frag);
		}
	}
	glazy::destroy();
	std::cout << "tests done!" << std::endl;
}


int main()
{
	auto current_dir = std::filesystem::current_path();
	auto glazy_dir = current_dir.parent_path().parent_path();
	auto assets_dir = glazy_dir / "assets";
	std::cout << "current directory:" << current_dir << std::endl;
	std::cout << "glazy directory:" << glazy_dir << std::endl;
	std::cout << "assets directory: " << assets_dir << std::endl;
	//run_tests();

		// load model
	std::filesystem::path model_path = glazy_dir / "assets" / "backpack/backpack.obj";
	auto model = load_model(model_path);

    glazy::init();

	//glazy::destroy();
	//return 0;

	auto vertex_path = glazy_dir / "glazy" / "shaders" / "matcap.vert";
	auto fragment_path = glazy_dir / "glazy" / "shaders" / "matcap.frag";
	auto vertex_shader_code = glazy::read_text(vertex_path.string().c_str());
	auto fragment_shader_code = glazy::read_text(fragment_path.string().c_str());

	auto program = imdraw::make_program_from_shaders(
		imdraw::make_shader(GL_VERTEX_SHADER ,vertex_shader_code.c_str()),
		imdraw::make_shader(GL_FRAGMENT_SHADER, fragment_shader_code.c_str())
	);
	//auto program = OOGL::Program::from_shaders(
	//	OOGL::Shader::from_source(GL_VERTEX_SHADER, vertex_shader_code.c_str()), 
	//	OOGL::Shader::from_source(GL_FRAGMENT_SHADER, fragment_shader_code.c_str())
	//);

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

		
		ImGui::Begin("Style");
		ImGui::ShowStyleEditor();
		ImGui::End();

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

		ImGui::Begin("Code");
		
		if (ImGui::InputTextMultiline("code", &vertex_shader_code, { -1,-1 })) {
			std::cout << "input changed" << std::endl;
		}
		ImGui::End();

		// Background window
		auto viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos({ 0,0 });
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
			ImGui::InvisibleButton("BACKGROUND_CONTROL_BUTTON", ImVec2(-1, -1));
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
		ImGui::End(); // end background

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

		if (glazy::IsFileChanged(vertex_path.string())) {
			std::cout << "vertex changed" << std::endl;
		}

		//auto vert = OOGL::Shader::from_source(GL_VERTEX_SHADER, vertex_shader_code.c_str());
		//auto frag = OOGL::Shader::from_source(GL_FRAGMENT_SHADER, fragment_shader_code.c_str());
		//auto prog = OOGL::Program::from_shaders(vert, frag);
		
		// set program
		static auto matcap_tex = imdraw::make_texture_from_file((assets_dir / "jeepster_skinmat2.jpg").string());
		//imdraw::quad(matcap_tex);

		imdraw::set_uniforms(program, {
			{"projection", glazy::camera.getProjection()},
			{"view", glazy::camera.getView()},
			{"model", glm::mat4(1)},
			{"matCap", 0}
		});

		// create geometry
		//static auto geo = imgeo::cube();
		//static auto vbo = OOGL::ArrayBuffer::from_data(geo.positions);
		//static auto normal_vbo = OOGL::ArrayBuffer::from_data(geo.normals.value());
		//static auto vao = OOGL::VertexArray::from_attributes(program, {
		//	{"position",{vbo, 3}},
		//	{"normal", {normal_vbo, 3}}
		//});
		//static auto ebo = OOGL::ElementBuffer::from_data(geo.indices);

		// draw
		glEnable(GL_DEPTH_TEST);
		glUseProgram(program);
		
		glClearColor(.5, .5, 1, 1);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glBindTexture(GL_TEXTURE_2D, matcap_tex);

		static auto geo_buffer = imdraw::make_geo(imgeo::sphere());
		imdraw::draw(geo_buffer);
		//glBindVertexArray(vao);
		//glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
		//glDrawElements(geo.mode, geo.indices.size(), GL_UNSIGNED_INT, nullptr);
		//glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		//glBindVertexArray(0);
		glBindTexture(GL_TEXTURE_2D, 0);

		// 
        glazy::end_frame();
    }
    glazy::destroy();
    std::cout << "Hello World!\n";
}