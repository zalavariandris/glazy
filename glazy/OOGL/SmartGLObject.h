#pragma once
#include <glad/glad.h>
#include <functional>

namespace OOGL {
	class SmartGLObject {

	protected:
		GLuint _id = -1;
		mutable int* refs_ptr = NULL; // TODO: use shared_ptr!!! std::make_shared<int>

		std::function<GLuint()> _createFunc;
		std::function<void(GLuint)> _deleteFunc;
	public:
		// Constructors
		SmartGLObject(
			std::function<GLuint()> c, 
			std::function<void(GLuint)> d
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