#pragma once

#include <variant>
#include <tuple>
#include <map>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include <glad/glad.h>

namespace imdraw {
	using UniformVariant = std::variant<bool, int, float, glm::vec3, glm::vec2, glm::mat4, GLuint>;
	//using Texture = std::tuple<GLenum, GLuint>;
	//using Attribute = std::tuple<GLuint, GLint, GLenum, GLsizei>;
	//using Elements = std::tuple<GLuint, GLsizei, GLenum>;

	/* Textures */
	GLuint make_texture(
		GLsizei width = -1,
		GLsizei height = -1,
		const unsigned char* data = NULL,
		GLint internalformat = GL_RGB,
		GLenum format = GL_RGB,
		GLint type = GL_UNSIGNED_BYTE,
		GLint min_filter = GL_LINEAR,
		GLint mag_filter = GL_NEAREST,
		GLint wrap_s = GL_REPEAT,
		GLint wrap_t = GL_REPEAT
	);

	GLuint make_texture_float(
		GLsizei width = -1,
		GLsizei height = -1,
		float* data = NULL,
		GLint internalformat = GL_RGB,
		GLenum format = GL_RGB,
		GLint type = GL_FLOAT,
		GLint min_filter = GL_LINEAR,
		GLint mag_filter = GL_NEAREST,
		GLint wrap_s = GL_REPEAT,
		GLint wrap_t = GL_REPEAT
	);

	GLuint make_texture_from_file(std::string path);

	/* Frame Buffer Object */
	GLuint make_fbo(GLuint color_attachment);
	GLuint make_fbo(GLuint color_attachment, GLuint depth_attachment);

	/* Vertex Buffer Objects */
	GLuint make_vbo(const std::vector<glm::vec3>& data, GLenum usage = GL_STATIC_DRAW);
	GLuint make_vbo(const std::vector<glm::vec2>& data, GLenum usage = GL_STATIC_DRAW);
	GLuint make_vbo(const std::vector<glm::mat4>& data, GLenum usage = GL_STATIC_DRAW);
	GLuint make_ebo(const std::vector<unsigned int>& data);
	GLuint make_ebo(const std::vector<glm::uvec3>& data);

	/* Vertex Array Objects */
	GLuint make_vao(std::map<GLuint, std::tuple<GLuint, GLsizei>> attributes);
	GLuint make_vao(GLuint program, std::map<std::string, std::tuple<GLuint, GLsizei>> attributes);

	/* Shader */
	GLuint make_shader(GLenum type, const char* shaderSource);

	/* Program */
	GLuint make_program_from_shaders(GLuint vertex_shader, GLuint fragment_shader);
	GLuint make_program_from_source(const char* vertexShaderSource, const char* fragmentShaderSource);
	GLuint make_program_from_files(const char* vertexSourcePath, const char* fragmentSourcePath);
	void set_uniforms(GLuint program, std::map<GLint, UniformVariant> uniforms);
	void set_uniforms(GLuint program, std::map<std::string, UniformVariant> uniforms);
	void push_program(GLuint program);
	GLuint pop_program();

	/* Trasform helpers */
	glm::mat4 orbit(glm::mat4 m, float yaw, float pitch);

	/* DRAW */
	void draw(GLenum mode, GLuint vao, GLsizei count); // draw array
	void draw(GLenum mode, GLuint vao, GLuint ebo, GLsizei count); // draw elements

	/* RENDER */
	void render(
		GLuint program,
		std::map<std::string, UniformVariant> uniforms,
		std::map<GLenum, std::tuple<GLenum, GLuint>> textures,
		GLuint vao,
		GLenum mode,
		GLuint ebo,
		size_t data_length
	);
}	