#include "bone.h"

Bone::Bone(): _id(0), _name(""), _offset(glm::mat4(1.0f)), _children({})
{

}

void Bone::addChild(const Bone& bone)
{
	this->_children.push_back(bone);
}