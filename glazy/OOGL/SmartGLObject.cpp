#include "SmartGLObject.h"
#include <stdlib.h>
#include <iostream>

// Constructors
OOGL::SmartGLObject::SmartGLObject() {
	GLCreate();
	ref_inc();
}
// getters and setters
GLuint OOGL::SmartGLObject::id() const {
	return _id;
}

// reference counter
void OOGL::SmartGLObject::ref_inc() const {
	if (refs_ptr == NULL) {
		refs_ptr = (int*)malloc(sizeof(int));
		(*refs_ptr) = 0;
	}

	(*refs_ptr) += 1;
}

void OOGL::SmartGLObject::ref_dec() const {
	(*refs_ptr) -= 1;
}

unsigned int OOGL::SmartGLObject::ref_count() const {
	if (refs_ptr == NULL) {
		return -1;
	}
	return *refs_ptr;
}

// copy constructor
OOGL::SmartGLObject::SmartGLObject(const OOGL::SmartGLObject& other) {
	std::cout << "copy program: " << other._id << " refs: " << other.ref_count() << std::endl;


	if (other.HasGLObject()) {
		other.ref_inc();
	}

	this->_id = other._id;
	this->refs_ptr = other.refs_ptr;
}

// override equal operator
OOGL::SmartGLObject& OOGL::SmartGLObject::operator=(const OOGL::SmartGLObject& other)
{
	if (this->HasGLObject()) {
		this->ref_dec();
	}
	if (other.HasGLObject()) {
		other.ref_inc();
	}

	this->_id = other._id;
	this->refs_ptr = other.refs_ptr;

	return *this;
}

// destructor
OOGL::SmartGLObject::~SmartGLObject() {
	std::cout << "delete program: " << this->_id << " refs: " << this->ref_count() << std::endl;

	this->ref_dec();

	// clenup gl if no references left
	if (this->ref_count() > 0) {
		return;
	}

	if (glIsProgram(this->_id)) {
		std::cout << "GC program: " << this->_id << std::endl;
		GLDestroy();
		free(this->refs_ptr);
	}
}