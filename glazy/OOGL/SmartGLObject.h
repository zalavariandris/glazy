#pragma once
#include <glad/glad.h>
#include <map>
#include <iostream>
#define OOGL_PLACEHOLDER

namespace OOGL {
	class SmartGLObject {

	protected:
		GLuint _id = -1;
		static std::map<GLuint, int> refs;
		mutable int* refs_ptr = NULL;
	public:
		// Constructors
		//GLObject(OOGL_PLACEHOLDER) :_id(-1) {};

		SmartGLObject();

		// getters and setters
		GLuint id() const;

		operator GLuint() const {
			return _id;
		}

		// reference counter
		void ref_inc() const;

		void ref_dec() const;

		unsigned int ref_count() const;

		// overide
		virtual void GLCreate() {
			std::cout << "Error:GLCreate Base method called;" << std::endl;
		};
		virtual void GLDestroy() {
			std::cout << "Error:GLDestroy Base method called;" << std::endl;
		};

		virtual bool HasGLObject() const {
			std::cout << "Error:HasGLObject Base method called;" << std::endl;
			return false;
		};

		// copy constructor
		SmartGLObject(const SmartGLObject& other);

		// override equal operator
		SmartGLObject& operator=(const SmartGLObject& other);

		// destructor
		~SmartGLObject();
	};
}