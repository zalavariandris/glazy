#include "SmartGLObject.h"
#include <stdlib.h>
#include <iostream>



// Constructors
OOGL::SmartGLObject::SmartGLObject(
	std::function<GLuint()> create_func, 
	std::function<void(GLuint)> delete_func
)
{
	_id = create_func();
	std::cout << "smartgl constructor " << _id << std::endl;
	_deleteFunc = delete_func;

	refs_ptr = (int*)malloc(sizeof(int));
	(*refs_ptr) = 1;
}

// reference counter
void OOGL::SmartGLObject::ref_inc() const {
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
	std::cout << "smartgl: copy: " << this->_id << " refs: " << this->ref_count() << std::endl;

	other.ref_inc();

	this->_id = other._id;
	this->refs_ptr = other.refs_ptr;
}

// override equal operator
OOGL::SmartGLObject& OOGL::SmartGLObject::operator=(const OOGL::SmartGLObject& other)
{
	std::cout << "smartgl: equal: " << this->_id << " refs: " << this->ref_count() << std::endl;
	this->ref_dec();
	other.ref_inc();

	this->_id = other._id;
	this->refs_ptr = other.refs_ptr;

	return *this;
}

// destructor
OOGL::SmartGLObject::~SmartGLObject() {
	std::cout << "smartgl: destructor: " << this->_id << " refs: " << this->ref_count() << std::endl;

	this->ref_dec();

	// keep alive?
	if (this->ref_count() > 0) {
		return;
	}

	// when no references left:
	// delete from GPU
	std::cout << "smartgl: delete object: " << this->_id << " refs: " << this->ref_count() << std::endl;
	free(this->refs_ptr);
	this->_deleteFunc(this->_id);
}