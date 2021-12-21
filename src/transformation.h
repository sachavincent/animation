#ifndef TRANSFORMATION_H
#define TRANSFORMATION_H

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/dual_quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

class Transformation
{
public:
    Transformation(glm::vec3 position = { 0, 0, 0 }, glm::quat rotation = { 0, 0, 0, 0 }, glm::vec3 scale = { 1, 1, 1 });

    inline const glm::vec3& position() const { return _position; }
    inline glm::vec3& position() { return _position; }

    inline const glm::quat& rotation() const { return _rotation; }
    inline glm::quat& rotation() { return _rotation; }

    inline const glm::vec3& scale() const { return _scale; }
    inline glm::vec3& scale() { return _scale; }

    glm::mat4 toTransformMatrix();

    glm::fdualquat toDualQuat();

    /*
    * Interpolate 2 transformations based on the progression value (between 0 and 1)
    */
    static Transformation interpolate(Transformation transformA, Transformation transformB, float progression)
    {
        assert(progression >= 0 && progression <= 1);

        glm::vec3 newPosition = glm::mix(transformA.position(), transformB.position(), progression);
        glm::quat newRotation = glm::slerp(transformA.rotation(), transformB.rotation(), progression);
        glm::vec3 newScale = glm::mix(transformA.scale(), transformB.scale(), progression);

        return Transformation(newPosition, newRotation, newScale);
    }
private:
    glm::vec3 _position;

    glm::quat _rotation;

    glm::vec3 _scale;
};
#endif // TRANSFORMATION_H