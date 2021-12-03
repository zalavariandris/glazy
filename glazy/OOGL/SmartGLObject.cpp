#include "SmartGLObject.h"
#include <stdlib.h>
#include <iostream>

// Constructors
OOGL::SmartGLObject::SmartGLObject(
	std::function<GLuint()> c, 
	std::function<void(GLuint)> d, 
	std::function<bool(GLuint)> e)
{
	_id = c();
	std::cout << "smartgl constructor " << _id << std::endl;
	_deleteFunc = d;
	_existFunc = e;
	ref_inc();
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
	std::cout << "smartgl copy constructor: " << other._id << " refs: " << other.ref_count() << std::endl;

	if (other._existFunc(other._id)) {
		other.ref_inc();
	}

	this->_id = other._id;
	this->refs_ptr = other.refs_ptr;
}

// override equal operator
OOGL::SmartGLObject& OOGL::SmartGLObject::operator=(const OOGL::SmartGLObject& other)
{
	
	if (this->_existFunc(this->_id)) {
		this->ref_dec();
	}
	if (other._existFunc(other._id)) {
		other.ref_inc();
	}

	this->_id = other._id;
	this->refs_ptr = other.refs_ptr;

	return *this;
}

// destructor
OOGL::SmartGLObject::~SmartGLObject() {
	std::cout << "smargl destructor: " << this->_id << " refs: " << this->ref_count() << std::endl;

	this->ref_dec();

	// clenup gl if no references left
	if (this->ref_count() > 0) {
		return;
	}

	if (glIsProgram(this->_id)) {
		std::cout << "delete gl object program: " << this->_id << std::endl;
		this->_deleteFunc(this->_id);
		free(this->refs_ptr);
	}
}