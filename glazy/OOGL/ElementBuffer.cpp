#include "ElementBuffer.h"

namespace OOGL {
	ElementBuffer ElementBuffer::from_data(const std::vector<unsigned int>& data, GLenum usage) {
		ElementBuffer buffer;
		auto buffer_id = (GLuint)buffer;
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer_id);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, data.size() * sizeof(unsigned int), data.data(), GL_STATIC_DRAW);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		return buffer;
	}
}