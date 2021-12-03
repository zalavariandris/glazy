#pragma once
#include <glad/glad.h>
#include <map>
#include <iostream>
#include <functional>
#include <variant>
#define OOGL_PLACEHOLDER

namespace OOGL {
	class SmartGLObject {

	protected:
		GLuint _id = -1;
		static std::map<GLuint, int> refs;
		mutable int* refs_ptr = NULL;

		std::function<GLuint()> _createFunc;
		std::function<void(GLuint)> _deleteFunc;
		std::function<bool(GLuint)> _existFunc;
	public:
		// Constructors
		SmartGLObject(
			std::function<GLuint()> c, 
			std::function<void(GLuint)> d, 
			std::function<bool(GLuint)> e
		);

		// cast to GLuint
		GLuint id() const {
			return _id;
		}
		operator GLuint() const {
			return _id;
		}

		// reference counter
		void ref_inc() const;
		void ref_dec() const;
		unsigned int ref_count() const;

		// copy constructor
		SmartGLObject(const SmartGLObject& other);

		// override equal operator
		SmartGLObject& operator=(const SmartGLObject& other);

		// destructor
		~SmartGLObject();
	};
}