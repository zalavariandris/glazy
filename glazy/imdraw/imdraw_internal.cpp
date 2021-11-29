#include "imdraw_internal.h"

#include <glm/gtc/type_ptr.hpp>

#include "../imgeo/imgeo.h"

#include <iostream>

/* file utils */
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <fstream>
#include <sstream>

template<typename T>
size_t container_size(const T& val) {
	return sizeof(val) / sizeof(val[0]);
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

inline unsigned char* read_image(std::string path, int* width, int* height, int* channels) {
	unsigned char* data = stbi_load(path.c_str(), width, height, channels, 0);
	return data;
}

inline void free_image(void* data) {
	stbi_image_free(data);
}

/*********
* IMDRAW *
**********/
/* Frame Buffer Object */
GLuint imdraw::make_fbo(GLuint color_attachment) {
	GLuint fbo;
	glGenFramebuffers(1, &fbo);

	// attach
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_attachment, 0);


	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		std::cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	return fbo;
}

/* Vertex Buffer Objects*/
GLuint imdraw::make_vbo(const std::vector<glm::vec3>& data, GLenum usage) {
	GLuint vbo;
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(glm::vec3), data.data(), usage);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	return vbo;
}

GLuint imdraw::make_vbo(const std::vector<glm::vec2>& data, GLenum usage) {
	GLuint vbo;
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(glm::vec2), data.data(), usage);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	return vbo;
}

GLuint imdraw::make_vbo(const std::vector<glm::mat4>& data, GLenum usage) {
	GLuint vbo;
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(glm::mat4), data.data(), usage);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	return vbo;
}

GLuint imdraw::make_ebo(const std::vector<unsigned int>& data) {
	GLuint ebo;
	glGenBuffers(1, &ebo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, data.size() * sizeof(unsigned int), data.data(), GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	return ebo;
}

GLuint imdraw::make_ebo(const std::vector<glm::uvec3>& data) {
	GLuint ebo;
	glGenBuffers(1, &ebo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, data.size() * sizeof(glm::uvec3), data.data(), GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	return ebo;
}



/* Vertex Array Object */
GLuint imdraw::make_vao(std::map <GLuint, std::tuple<GLuint, GLsizei>> attributes) {
	GLuint vao;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	for (const auto& [location, attribute] : attributes) {
		const auto& [vbo, size] = attribute;
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glEnableVertexAttribArray(location);
		glVertexAttribPointer(location, size, GL_FLOAT, GL_FALSE, 0, nullptr);
	}
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	return vao;
}

GLuint imdraw::make_vao(GLuint program, std::map<std::string, std::tuple<GLuint, GLsizei>> attributes) {
	GLuint vao;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	for (const auto& [name, attribute] : attributes) {
		auto location = glGetAttribLocation(program, name.c_str());
		if (location < 0) {
			std::cout << "WARNING::GL: attribute '" << name << "' is not active!" << std::endl;
		}

		const auto& [vbo, size] = attribute;
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glEnableVertexAttribArray(location);
		glVertexAttribPointer(location, size, GL_FLOAT, GL_FALSE, 0, nullptr);
	}
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	return vao;
}

/*
GLuint imdraw::make_vao_from_trimesh(imgeo::Trimesh const& mesh, std::map<std::string, GLint> locations)
{
	GLuint vao;
	GLuint pos_vbo;
	GLuint uv_vbo;
	GLuint normal_vbo;
	GLuint color_vbo;

	// Create Vertex Array
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	// Create and bind vbos
	pos_vbo = make_vbo(mesh.positions);
	glBindBuffer(GL_ARRAY_BUFFER, pos_vbo);
	glEnableVertexAttribArray(locations["aPos"]);
	glVertexAttribPointer(locations["aPos"], 3, GL_FLOAT, GL_FALSE, 0, nullptr);

	if (mesh.uvs.has_value()) {
		uv_vbo = make_vbo(mesh.positions);
		glBindBuffer(GL_ARRAY_BUFFER, uv_vbo);
		glEnableVertexAttribArray(locations["aUV"]);
		glVertexAttribPointer(locations["aPUV"], 2, GL_FLOAT, GL_FALSE, 0, nullptr);
	}
	if (mesh.normals.has_value()) {
		normal_vbo = make_vbo(mesh.positions);
		glBindBuffer(GL_ARRAY_BUFFER, normal_vbo);
		glEnableVertexAttribArray(locations["aNormal"]);
		glVertexAttribPointer(locations["aNormal"], 3, GL_FLOAT, GL_FALSE, 0, nullptr);
	}
	if (mesh.colors.has_value()) {
		color_vbo = make_vbo(mesh.positions);
		glBindBuffer(GL_ARRAY_BUFFER, color_vbo);
		glEnableVertexAttribArray(locations["aColor"]);
		glVertexAttribPointer(locations["aColor"], 3, GL_FLOAT, GL_FALSE, 0, nullptr);
	}

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	return vao;
}
*/


/* Textures */
GLuint imdraw::make_texture(
	GLsizei width,
	GLsizei height,
	const unsigned char* data,
	GLint internalformat,
	GLenum format,
	GLint type,
	GLint min_filter,
	GLint mag_filter,
	GLint wrap_s,
	GLint wrap_t
)
{
	unsigned int texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_s);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_t);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);

	glTexImage2D(GL_TEXTURE_2D, 0, internalformat, width, height, 0, format, type, data);
	//glGenerateMipmap(GL_TEXTURE_2D);

	glBindTexture(GL_TEXTURE_2D, 0);
	return texture;
}

GLuint imdraw::make_texture_from_file(std::string path) {
	int width, height, nrChannels;
	unsigned char* data = read_image(path.c_str(), &width, &height, &nrChannels);

	if (!data)
		std::cout << "Failed to load texture from file: " << path << std::endl;

	GLuint texture = imdraw::make_texture(width, height, data);

	free_image(data);
	return texture;
}

/* Shader */
GLuint imdraw::make_shader(GLenum type, const char* shaderSource) {
	// create shader
	GLuint shader;
	shader = glCreateShader(type);
	glShaderSource(shader, 1, &shaderSource, NULL);
	glCompileShader(shader);

	// error check
	int success;
	char infoLog[512];
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(shader, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::COMPILATION_FAILED\n" << type << infoLog << std::endl;
	};
	return shader;
}

/* Program */
GLuint imdraw::make_program_from_source(const char* vertexShaderSource, const char* fragmentShaderSource) {
	std::cout << "create program" << std::endl;
	// create vertex shader
	auto vertexShader = make_shader(GL_VERTEX_SHADER, vertexShaderSource);

	// create fragment shader
	auto fragmentShader = make_shader(GL_FRAGMENT_SHADER, fragmentShaderSource);

	// compile program
	GLuint shaderProgram;
	shaderProgram = glCreateProgram();

	glAttachShader(shaderProgram, vertexShader);
	glAttachShader(shaderProgram, fragmentShader);
	glLinkProgram(shaderProgram);

	int success;
	char infoLog[512];
	glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
	if (!success)
	{
		glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
	}

	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	return shaderProgram;
}

GLuint imdraw::make_program_from_files(const char* vertexSourcePath, const char* fragmentSourcePath) {
	auto vertexSource = read_text(vertexSourcePath);
	auto fragmentSource = read_text(fragmentSourcePath);

	return make_program_from_source(vertexSource.c_str(), fragmentSource.c_str());
}

void imdraw::set_uniforms(GLuint program, std::map<GLint, UniformVariant> uniforms) {
	push_program(program);
	for (auto& [location, data] : uniforms)
	{
		if (auto value = std::get_if<bool>(&data)) {
			glUniform1i(location, *value ? 1 : 0);
		}
		else if (auto value = std::get_if<int>(&data)) {
			glUniform1i(location, *value);
		}
		else if (auto value = std::get_if<float>(&data)) {
			glUniform1f(location, *value);
		}
		else if (auto value = std::get_if<GLuint>(&data)) {
			glUniform1i(location, *value);
		}
		else if (auto value = std::get_if<glm::vec3>(&data)) {
			glUniform3f(location, value->x, value->y, value->z);
		}
		else if (auto value = std::get_if<glm::mat4>(&data)) {
			glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(*value));
		}
		else {
			std::cout << "WARNING:GLAZY: '" << location << "' type is not handled" << std::endl;
		}
	}
	pop_program();
}

void imdraw::set_uniforms(GLuint program, std::map<std::string, UniformVariant> uniforms)
{
	push_program(program);
	for (auto& [name, data] : uniforms)
	{
		GLint location = glGetUniformLocation(program, name.c_str());
		if (location < 0) {
			std::cout << "WARNING:GLAZY: '" << name << "' uniform is not used!" << std::endl;
		}

		if (auto value = std::get_if<bool>(&data)) {
			glUniform1i(location, *value ? 1 : 0);
		}
		else if (auto value = std::get_if<int>(&data)) {
			glUniform1i(location, *value);
		}
		else if (auto value = std::get_if<float>(&data)) {
			glUniform1f(location, *value);
		}
		else if (auto value = std::get_if<GLuint>(&data)) {
			glUniform1i(location, *value);
		}
		else if (auto value = std::get_if<glm::vec3>(&data)) {
			glUniform3f(location, value->x, value->y, value->z);
		}
		else if (auto value = std::get_if<glm::mat4>(&data)) {
			glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(*value));
		}
		else {
			std::cout << "WARNING:GLAZY: '" << name << "' type is not handled" << std::endl;
		}
	}
	pop_program();
}

std::vector<GLint> program_stack;

void imdraw::push_program(GLuint program) {
	GLint current_prog = 0;
	glGetIntegerv(GL_CURRENT_PROGRAM, &current_prog);

	program_stack.push_back(current_prog);

	if (program != current_prog) {
		glUseProgram(program);
	}
}

GLuint imdraw::pop_program() {
	assert(program_stack.size() > 0);

	GLint current_prog = 0;
	glGetIntegerv(GL_CURRENT_PROGRAM, &current_prog);

	GLuint program = program_stack.back();
	if (program != current_prog) {
		glUseProgram(program);
	}
	program_stack.pop_back();
	return program;
}

/* Trasform helpers */
glm::mat4 imdraw::orbit(glm::mat4 m, float yaw, float pitch) {
	const glm::vec3 horizontal_axis{ m[0][0], m[1][0], m[2][0] };
	const glm::vec3 vertical_axis{ 0,1,0 };

	auto mat = glm::mat4(1);
	m *= glm::rotate(mat, yaw, horizontal_axis);
	m *= glm::rotate(mat, pitch, vertical_axis);

	return m;
}

/* DRAW */
void imdraw::draw(GLuint vao, GLenum mode, GLuint ebo, GLsizei count) {
	// draw VAO
	glBindVertexArray(vao);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glDrawElements(mode, count, GL_UNSIGNED_INT, nullptr);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}

void imdraw::render(GLuint program,
	std::map<std::string, std::variant<bool, int, float, glm::vec3, glm::mat4, GLuint>> uniforms,
	std::map<GLenum, std::tuple<GLenum, GLuint>> textures,
	GLuint vao,
	GLenum mode,
	GLuint ebo,
	size_t data_length)
{
	// bind program
	GLint current_program;
	glGetIntegerv(GL_CURRENT_PROGRAM, &current_program);
	if (current_program != program) {
		glUseProgram(program);
	}

	// bind textures
	for (auto& [slot, texture] : textures) {
		glActiveTexture(GL_TEXTURE0 + slot);
		auto& [target, tex] = texture;
		glBindTexture(target, tex);
	}

	// set uniforms
	imdraw::set_uniforms(program, uniforms);

	// set atributes
	//auto vao = make_vao(program, attributes);

	// draw primitive
	imdraw::draw(vao, mode, ebo, data_length);

	// restore state
	//if (current_program != program) {
	//	glUseProgram(current_program);
	//}
}