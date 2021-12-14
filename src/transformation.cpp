#include "transformation.h"

Transformation::Transformation(glm::vec3 position, glm::quat rotation, glm::vec3 scale):
    _position(position), _rotation(rotation), _scale(scale)
{}

glm::mat4 Transformation::toTransformMatrix()
{
    glm::mat4 translation = glm::translate(glm::mat4(1.0f), _position);
    glm::mat4 rotation = glm::toMat4(_rotation);
    glm::mat4 scale = glm::scale(glm::mat4(1.0f), _scale);

    return translation * rotation * scale;
}