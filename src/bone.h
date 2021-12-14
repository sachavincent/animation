#ifndef BONE_H
#define BONE_H

#include <string>
#include <vector>
#include <glm/glm.hpp>

class Bone
{
public:
    Bone();

    inline const int& id() const { return this->_id; }
    inline void setId(const int& id) { this->_id = id; }

    inline const std::string& name() const { return this->_name; }
    inline void setName(const std::string& name) { this->_name = name; }

    inline const glm::mat4& offset() const { return this->_offset; }
    inline void setOffset(const glm::mat4& offset) { this->_offset = offset; }

    void addChild(const Bone& bone);

    inline std::vector<Bone>& children() { return this->_children; }
private:
    int _id;
    std::string _name;

    glm::mat4 _offset;

    std::vector<Bone> _children;
};
#endif // BONE_H