#pragma once
#include "SmartGLObject.h"
#include <string>
#include <assert.h>

namespace OOGL {
	class Framebuffer : public SmartGLObject
	{
	public:
		Framebuffer() : SmartGLObject(
			[]() {
				GLuint framebuffer_id;
				glGenFramebuffers(1, &framebuffer_id);
				return framebuffer_id;
			},
			[](GLuint framebuffer_id) {
				assert(glIsFramebuffer(framebuffer_id));
				glDeleteFramebuffers(1, &framebuffer_id);
			}
		) {}

		static Framebuffer with_attachments(GLuint color_attachment);
		static Framebuffer with_attachments(GLuint color_attachment, GLuint depth_attachment);
	};
}