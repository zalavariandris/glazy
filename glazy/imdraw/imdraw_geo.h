#include "glm/glm.hpp"
#include <glad/glad.h>
#include <vector>

#include "imdraw_internal.h"
#include "../imgeo/imgeo.h"

namespace imdraw {
	struct GeometryBuffer {
		GLenum mode;
		GLuint vao;
		GLuint ebo;
		GLsizei count;
	};

	GeometryBuffer make_geo(imgeo::Trimesh geo, std::map<std::string, GLuint> locations = std::map<std::string, GLuint>({ 
		{"position", 0},
		{"uv", 1},
		{"normal", 2}
	})) 
	{
		std::map<GLuint, std::tuple<GLuint, GLsizei>> attributes;
		auto pos_vbo = imdraw::make_vbo(geo.positions);
		attributes[locations["position"]] = { pos_vbo, 3 };

		if (geo.uvs) {
			auto uv_vbo = imdraw::make_vbo(geo.uvs.value());
			attributes[locations["uv"]] = { uv_vbo,2 };
		}

		if (geo.normals) {
			auto normal_vbo = imdraw::make_vbo(geo.normals.value());
			attributes[locations["normal"]] = { normal_vbo,3 };
		}

		if (geo.colors) {
			auto color_vbo = imdraw::make_vbo(geo.colors.value());
			attributes[locations["color"]] = { color_vbo,3 };
		}

		auto vao = imdraw::make_vao(attributes);

		auto ebo = imdraw::make_ebo(geo.indices);

		return { geo.mode, vao, ebo, (GLsizei)geo.indices.size() };
	}

	void draw(GeometryBuffer geo) {
		// draw VAO
		glBindVertexArray(geo.vao);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, geo.ebo);
		glDrawElements(geo.mode, geo.count, GL_UNSIGNED_INT, nullptr);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
}

