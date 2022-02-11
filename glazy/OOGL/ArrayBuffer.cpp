#include "ArrayBuffer.h"


OOGL::ArrayBuffer OOGL::ArrayBuffer::from_data(const std::vector<glm::vec2>& data, GLenum usage) {
	OOGL::ArrayBuffer buffer;
	auto buffer_id = (GLuint)buffer;
	glBindBuffer(GL_ARRAY_BUFFER, buffer_id);
	glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(glm::vec2), data.data(), usage);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	return buffer;
}

OOGL::ArrayBuffer OOGL::ArrayBuffer::from_data(const std::vector<glm::vec3>& data, GLenum usage) {
	OOGL::ArrayBuffer buffer;
	auto buffer_id = (GLuint)buffer;
	glBindBuffer(GL_ARRAY_BUFFER, buffer_id);
	glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(glm::vec3), data.data(), usage);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	return buffer;
}


OOGL::ArrayBuffer OOGL::ArrayBuffer::from_data(const std::vector<glm::mat4>& data, GLenum usage) {
	OOGL::ArrayBuffer buffer;
	auto buffer_id = (GLuint)buffer;
	glBindBuffer(GL_ARRAY_BUFFER, buffer_id);
	glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(glm::mat4), data.data(), usage);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	return buffer;
}
