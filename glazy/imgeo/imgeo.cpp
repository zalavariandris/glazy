#include "imgeo.h"
#include <glm/ext.hpp> //glm::pi<double>()

imgeo::Trimesh imgeo::grid(unsigned int slices) {
	// create geo
	std::vector<glm::vec3> positions;
	std::vector<glm::vec2> uvs;
	std::vector<glm::vec3> normals;
	std::vector<unsigned int> indices;

	for (int j = 0; j <= slices; ++j) {
		for (int i = 0; i <= slices; ++i) {
			float x = (float)i / (float)slices - 0.5;
			float z = 0;
			float y = (float)j / (float)slices - 0.5;
			x *= slices;
			y *= slices;
			positions.push_back(glm::vec3(x, y, z));
		}
	}

	for (int j = 0; j < slices; ++j) {
		for (int i = 0; i < slices; ++i) {

			int row1 = j * (slices + 1);
			int row2 = (j + 1) * (slices + 1);

			//indices.push_back(glm::uvec4(
			indices.push_back(row1 + i);
			indices.push_back(row1 + i + 1);
			indices.push_back(row1 + i + 1);
			indices.push_back(row2 + i + 1);

			//indices.push_back(glm::uvec4(
			indices.push_back(row2 + i + 1);
			indices.push_back(row2 + i);
			indices.push_back(row2 + i);
			indices.push_back(row1 + i);
		}
	}

	return { GL_LINES, positions, indices };
}

imgeo::Trimesh imgeo::quad() {
	// create geometry
	static std::vector<glm::vec3> positions{
		{-1,-1,0}, {1,-1,0}, {1, 1, 0}, {-1, 1, 0}
	};

	static std::vector<glm::vec2> uvs{
		{0,0}, {1,0}, {1, 1}, {0, 1}
	};

	static std::vector<glm::vec3> normals{
		{0,0,-1}, {0,0,-1}, {0,0,-1}, {0,0,-1}
	};

	static std::vector<unsigned int> indices{
		0,1,2, 0,2,3
	};

	return Trimesh(GL_TRIANGLES, positions, indices, uvs, normals);
}

imgeo::Trimesh imgeo::disc(unsigned int segments) {
	auto positions = std::vector<glm::vec3>(segments);
	auto uvs = std::vector<glm::vec2>(segments);
	auto normals = std::vector<glm::vec3>(segments);
	auto indices = std::vector<unsigned int>((segments - 1) * 3);

	// create vertices
	for (auto i = 0; i < segments; i++) {
		auto angle = (double)i / segments;

		auto PI = glm::pi<double>();
		auto P = glm::vec3(glm::cos(angle * PI * 2), glm::sin(angle * PI * 2), 0);
		positions[i] = P;
		uvs[i] = glm::vec2(P.x, P.y);
		normals[i] = glm::vec3(0, 0, 1);
	}

	// create indices
	for (auto i = 0; i < segments - 1; i++) {
		//indices[i] = glm::uvec3(0, i, i + 1);
		indices[i * 3 + 0] = 0;
		indices[i * 3 + 1] = i;
		indices[i * 3 + 2] = i + 1;
	}

	return { GL_TRIANGLES, positions, indices, uvs, normals };
}

imgeo::Trimesh imgeo::cylinder() {
	const auto PI_2 = glm::pi<float>() * 2;


	std::vector<glm::vec3> positions;
	std::vector<glm::vec3> normals;
	std::vector<unsigned int> indices;
	size_t index_offset = 0;
	// create side
	index_offset = positions.size();
	for (auto j = 0; j < 8; j++) {
		for (auto i = 0; i < 8; i++) {
			// calc position
			auto P = glm::vec3(
				cosf((float)i / 8.0 * PI_2) * 0.5,
				sinf((float)i / 8.0 * PI_2) * 0.5,
				(float)j / 7 - 0.5
			);
			positions.push_back(P);

			// calc normal
			auto N = glm::normalize(glm::vec3(P.x, P.y, 0));
			normals.push_back(N);
		}
	}

	// create indices
	for (auto j = 0; j < 7; j++) {
		for (auto i = 0; i < 8; i++) {
			indices.push_back(i % 8 + j * 8 + index_offset);
			indices.push_back((i + 1) % 8 + j * 8 + index_offset);
			indices.push_back(i % 8 + (j + 1) * 8 + index_offset);

			indices.push_back((i + 1) % 8 + j * 8 + index_offset);
			indices.push_back((i + 1) % 8 + (j + 1) * 8 + index_offset);
			indices.push_back(i % 8 + (j + 1) * 8 + index_offset);
		}
	}

	// cap
	index_offset = positions.size();
	positions.push_back({ 0,0,-0.5 }); //north pole
	normals.push_back(glm::vec3(0, 0, -1.0));
	for (auto i = 0; i < 8; i++) {
		// calc position
		auto P = glm::vec3(
			cosf((float)i / 8.0 * PI_2) * 0.5,
			sinf((float)i / 8.0 * PI_2) * 0.5,
			-0.5
		);
		positions.push_back(P);

		// calc normal
		normals.push_back(glm::vec3(0, 0, -1.0));
	}

	for (auto i = 0; i < 8; i++) {
		indices.push_back(index_offset);
		indices.push_back((i + 1) % 8 + index_offset + 1);
		indices.push_back(i % 8 + index_offset + 1);
	}

	// cap top
	index_offset = positions.size();
	positions.push_back({ 0,0,0.5 }); //south pole
	normals.push_back(glm::vec3(0, 0, 1.0));
	for (auto i = 0; i < 8; i++) {
		// calc position
		auto P = glm::vec3(
			cosf((float)i / 8.0 * PI_2) * 0.5,
			sinf((float)i / 8.0 * PI_2) * 0.5,
			0.5
		);
		positions.push_back(P);

		// calc normal
		normals.push_back(glm::vec3(0, 0, 1.0));
	}

	for (auto i = 0; i < 8; i++) {
		indices.push_back(index_offset);
		indices.push_back(i % 8 + index_offset + 1);
		indices.push_back((i + 1) % 8 + index_offset + 1);
	}

	return { GL_TRIANGLES, positions, indices, std::nullopt, normals };
}


imgeo::Trimesh imgeo::sphere(size_t segs, size_t stacks) {
	const auto PI = glm::pi<float>();

	std::vector<glm::vec3> positions;
	std::vector<glm::vec3> normals;
	std::vector<unsigned int> indices;

	// create side
	for (auto j = 1; j <= stacks - 1; j++) {
		for (auto i = 0; i < segs; i++) {
			// calc position
			auto P = glm::vec3(
				cos((double)i / segs * PI * 2) * 0.5 * sin((double)j / stacks * PI),
				sin((double)i / segs * PI * 2) * 0.5 * sin((double)j / stacks * PI),
				cos((double)j / stacks * PI) * 0.5
			);
			positions.push_back(P);

			// calc normal
			auto N = glm::normalize(glm::vec3(P.x, P.y, P.z));
			normals.push_back(N);
		}
	}

	// create indices
	for (auto j = 0; j < stacks - 2; j++) {
		for (auto i = 0; i < segs; i++) {
			indices.push_back(i % segs + j * segs);
			indices.push_back(i % segs + (j + 1) * segs);
			indices.push_back((i + 1) % segs + j * segs);

			indices.push_back((i + 1) % segs + j * segs);
			indices.push_back(i % segs + (j + 1) * segs);
			indices.push_back((i + 1) % segs + (j + 1) * segs);
		}
	}

	// north pole
	auto north_pole_idx = positions.size();
	positions.push_back(glm::vec3(0, 0, 0.5));
	normals.push_back(glm::vec3(0, 0, 1));
	for (auto i = 0; i < segs; i++) {
		indices.push_back(i);
		indices.push_back((i + 1) % segs);
		indices.push_back(north_pole_idx);
	}

	// south pole
	auto south_pole_idx = positions.size();
	positions.push_back(glm::vec3(0, 0, -0.5));
	normals.push_back(glm::vec3(0, 0, -1));
	for (auto i = 0; i < segs; i++) {
		indices.push_back(i + north_pole_idx - segs);
		indices.push_back(south_pole_idx);
		indices.push_back((i + 1) % segs + north_pole_idx - segs);
	}

	return { GL_TRIANGLES, positions, indices, std::nullopt, normals };
}

imgeo::Trimesh imgeo::cube() {
	std::vector<glm::vec3> positions;
	std::vector<glm::vec3> normals;
	std::vector<unsigned int> indices;

	// top
	positions = std::vector<glm::vec3>{
		// {-1,-1}, {+1, -1}, {+1,+1}, {-1,+1},
		{-1,+1,-1}, {+1,+1,-1}, {+1,+1,+1}, {-1,+1,+1}, //top
		{-1,-1,-1}, {+1,-1,-1}, {+1,-1,+1}, {-1,-1,+1}, //bottom
		{-1,-1,-1}, {-1,+1,-1}, {-1,+1,+1}, {-1,-1,+1}, // right
		{+1,-1,-1}, {+1,+1,-1}, {+1,+1,+1}, {+1,-1,+1}, // left
		{-1,-1,+1}, {+1,-1,+1}, {+1,+1,+1}, {-1,+1,+1}, // front
		{-1,-1,-1}, {+1,-1,-1}, {+1,+1,-1}, {-1,+1,-1}  // back

	};

	normals = std::vector<glm::vec3>{
		{0,+1,0},{0,+1,0},{0,+1,0},{0,+1,0},
		{0,-1,0},{0,-1,0},{0,-1,0},{0,-1,0},
		{-1,0,0},{-1,0,0},{-1,0,0},{-1,0,0},
		{+1,0,0},{+1,0,0},{+1,0,0},{+1,0,0},
		{0,0,+1},{0,0,+1},{0,0,+1},{0,0,+1},
		{0,0,-1},{0,0,-1},{0,0,-1},{0,0,-1}
	};

	indices = std::vector<unsigned int>{
		 0, 2, 1,  0, 3, 2, //top
		 4, 5, 6,  4, 6, 7, // bottom
		 8,10, 9,  8,11,10, // right
		12,13,14, 12,14,15, // left
		16,17,18, 16,18,19, // back
		20,22,21, 20,23,22  // front
	};

	return { GL_TRIANGLES, positions, indices, std::nullopt, normals };
}

imgeo::Trimesh imgeo::sharp_cube() {
	std::vector<glm::vec3> positions;
	std::vector<glm::vec3> normals;
	std::vector<unsigned int> indices;

	// top
	positions = std::vector<glm::vec3>{
		// {-1,-1}, {+1, -1}, {+1,+1}, {-1,+1},
		{-1,+1,-1}, {+1,+1,-1}, {+1,+1,+1}, {-1,+1,+1}, //top
		{-1,-1,-1}, {+1,-1,-1}, {+1,-1,+1}, {-1,-1,+1}, //bottom
		{-1,-1,-1}, {-1,+1,-1}, {-1,+1,+1}, {-1,-1,+1}, // right
		{+1,-1,-1}, {+1,+1,-1}, {+1,+1,+1}, {+1,-1,+1}, // left
		{-1,-1,+1}, {+1,-1,+1}, {+1,+1,+1}, {-1,+1,+1}, // front
		{-1,-1,-1}, {+1,-1,-1}, {+1,+1,-1}, {-1,+1,-1}  // back
	};

	normals = std::vector<glm::vec3>{
		{0,+1,0},{0,+1,0},{0,+1,0},{0,+1,0},
		{0,-1,0},{0,-1,0},{0,-1,0},{0,-1,0},
		{-1,0,0},{-1,0,0},{-1,0,0},{-1,0,0},
		{+1,0,0},{+1,0,0},{+1,0,0},{+1,0,0},
		{0,0,+1},{0,0,+1},{0,0,+1},{0,0,+1},
		{0,0,-1},{0,0,-1},{0,0,-1},{0,0,-1}
	};

	indices = std::vector<unsigned int>{
		 2, 0, 1,  3, 0, 2, // top
		 5, 4, 6,  6, 4, 7, // bottom
		 10,8, 9,  11,8,10, // right
		13,12,14, 14,12,15, // left
		17,16,18, 18,16,19, // back
		22,20,21, 23,20,22  // front
	};

	return { GL_TRIANGLES, positions, indices, std::nullopt, normals };
}