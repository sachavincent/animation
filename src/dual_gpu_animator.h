#pragma once

#include <assimp/Importer.hpp>
#include <assimp/matrix4x4.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/vector3.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "utils.h"
#include "animation.h"

#include "bone.h"
#include "vao.h"
#include "keyframe.h"
#include "shader.h"

static Vao createVertexArrayDual(std::vector<Vertex>& vertices, std::vector<GLuint> indices) {
    Vao vao(true);

    GLuint& vboId = vao.vboId();
    GLuint& eboId = vao.eboId();
    glGenBuffers(1, &vboId);

    glBindBuffer(GL_ARRAY_BUFFER, vboId);

    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * vertices.size(), &vertices[0], GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)offsetof(Vertex, position));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)offsetof(Vertex, uv));
    glEnableVertexAttribArray(3);
    glVertexAttribIPointer(3, 4, GL_INT, sizeof(Vertex), (GLvoid*)offsetof(Vertex, boneIds));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)offsetof(Vertex, boneWeights));

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, eboId);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), &indices[0], GL_STATIC_DRAW);
    
    vao.setVertexCount(indices.size());
    vao.unbind();

    return vao;
}

static void getPoseDual(Animation& animation, Bone& bone, float animationTime, std::vector<glm::fdualquat>& output,
    glm::fdualquat& parentTransform) {
    animationTime = fmod(animationTime, animation.duration());
    glm::fdualquat identityQuat = glm::fdualquat(glm::quat(1.f, 0.f, 0.f, 0.f), glm::quat(0.f, 0.f, 0.f, 0.f));
    std::vector<KeyFrame> keyFrames = animation.getBoneKeyFrames(bone.name());
    glm::mat4 globalTransform;
    glm::fdualquat globalTransformQuat = identityQuat;
    if (!keyFrames.empty())
    {
        unsigned int currentKeyFrameIdx;
        for (unsigned int index = 0; index < keyFrames.size() - 1; ++index)
        {
            if (animationTime < keyFrames[index + 1].timeStamp)
            {
                currentKeyFrameIdx = index;
                break;
            }
        }
        KeyFrame currentKeyFrame = keyFrames[currentKeyFrameIdx];
        KeyFrame nextKeyFrame = keyFrames[(currentKeyFrameIdx + 1) % keyFrames.size()];
        float timeStamp1 = currentKeyFrame.timeStamp;
        float timeStamp2 = nextKeyFrame.timeStamp;
        float progression = (animationTime - timeStamp1) / (timeStamp2 - timeStamp1);
        if (timeStamp1 > animationTime)
        {
            progression = 0; // Happens when first timeStamp > 0
        }
        Transformation newTransform = Transformation::interpolate(currentKeyFrame.transform, nextKeyFrame.transform, progression);

        globalTransformQuat = glm::normalize(parentTransform * newTransform.toDualQuat());
        if (globalTransformQuat.dual.w == -0)
            globalTransformQuat.dual.w = 0;
    }
    glm::mat4 offset = bone.offset();
    glm::fdualquat offsetQuat = glm::normalize(glm::fdualquat(glm::normalize(glm::quat_cast(offset)), glm::vec3(offset[3][0], offset[3][1], offset[3][2])));
    if (offsetQuat.dual.w == -0)
        offsetQuat.dual.w = 0;

    glm::fdualquat res = glm::normalize(identityQuat * globalTransformQuat * offsetQuat);
    if (res.dual.w == -0)
        res.dual.w = 0;

    output[bone.id()] = res;

    for (Bone& child : bone.children()) {
        getPoseDual(animation, child, animationTime, output, globalTransformQuat);
    }
}

static void DualGPULoop(float time, FreeCamera camera, AnimPackage& anim)
{
    glm::fdualquat identityQuat = glm::fdualquat(glm::quat(1.f, 0.f, 0.f, 0.f), glm::quat(0.f, 0.f, 0.f, 0.f));

    std::vector<glm::fdualquat> currentPose;

    currentPose.resize(anim.boneCount, identityQuat);
    getPoseDual(anim.animation, anim.skeleton, time, currentPose, identityQuat);

    glm::mat4 projectionMatrix = glm::perspective(glm::radians(camera._zoom), ((float) WIDTH / HEIGHT), 0.01f, 100.0f);
    glm::mat4 viewMatrix = camera.view_matrix();
    glm::mat4 viewProjectionMatrix = projectionMatrix * viewMatrix;

    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    anim.shader.start();
    glm::mat4 modelMatrix = glm::mat4(1.0f);
    glm::vec3 rotation = { 90, 0, 0 };
    modelMatrix = glm::translate(modelMatrix, glm::vec3(0.0f, 1.0f, 0.0f));
    modelMatrix = glm::rotate(modelMatrix, glm::radians(rotation.x), glm::vec3(1, 0, 0));
    modelMatrix = glm::rotate(modelMatrix, glm::radians(rotation.y), glm::vec3(0, 1, 0));
    modelMatrix = glm::rotate(modelMatrix, glm::radians(rotation.z), glm::vec3(0, 0, 1));
    //modelMatrix = glm::scale(modelMatrix, glm::vec3(.2f, .2f, .2f));

    anim.shader.loadMatrix4("view_projection_matrix", glm::value_ptr(viewProjectionMatrix));
    anim.shader.loadMatrix4("model_matrix", glm::value_ptr(modelMatrix));

    for (unsigned int i = 0; i < currentPose.size(); i++) {
        glm::mat2x4 boneTr = glm::mat2x4_cast(currentPose[i]);
        anim.shader.loadMatrix2x4("bone_transforms[" + std::to_string(i) + "]", boneTr);
    }

    glActiveTexture(GL_TEXTURE0);
    anim.texture.load();

    anim.vao.bind();
    glDrawElements(GL_TRIANGLES, anim.vao.getVertexCount(), GL_UNSIGNED_INT, 0);
    anim.vao.unbind();
    anim.shader.stop();
}

static AnimPackage initDualGPU(const aiScene* scene, aiMesh* mesh)
{
    std::cout << "Init dual anim on GPU" << std::endl;

    std::vector<Vertex> vertices = {};
    std::vector<GLuint> indices = {};
    GLuint boneCount = 0;
    Animation animation;
    Bone skeleton;

    loadModel(scene, mesh, vertices, indices, skeleton, boneCount);
    loadAnimation(scene, skeleton, animation);

    Vao vao = createVertexArrayDual(vertices, indices);
    
    Shader shader("V_dual_shader.glsl", "F_shader.glsl");

    shader.start();
    shader.loadInt("diff_texture", 0);
    shader.stop();

    return AnimPackage(shader, vao, animation, skeleton, boneCount);
}