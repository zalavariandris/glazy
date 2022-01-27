#pragma once
#include <vector>
#include <optional>
#include <glm/glm.hpp>

// opengl
#include <glad/glad.h>

namespace imgeo {
	struct Trimesh {
		GLenum mode = GL_TRIANGLES;
		std::vector<glm::vec3> positions;
		std::vector<unsigned int> indices;
		std::optional<std::vector<glm::vec2>> uvs = std::nullopt;
		std::optional<std::vector<glm::vec3>> normals = std::nullopt;
		std::optional<std::vector<glm::vec3>> colors = std::nullopt;
	};

	Trimesh grid(unsigned int slices = 10);

	/// Unit quad, positions from -1 to 1
	/// UV from 0 to 1
	/// and -z facing normals
	Trimesh quad();
	Trimesh disc(unsigned int segmets = 32);
	Trimesh cylinder();
	Trimesh sphere(size_t segs = 8, size_t stacks = 8);
	Trimesh cube();
	Trimesh sharp_cube();
}