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

struct VertexCPU {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
    glm::vec4 boneTr0 = glm::vec4(1, 0, 0, 0);
    glm::vec4 boneTr1 = glm::vec4(0, 1, 0, 0);
    glm::vec4 boneTr2 = glm::vec4(0, 0, 1, 0);
    glm::vec4 boneTr3 = glm::vec4(0, 0, 0, 1);
};

static Vao createVertexArrayCPU(std::vector<VertexCPU>& vertices, std::vector<GLuint> indices) {
    Vao vao(true);

    GLuint& vboId = vao.vboId();
    GLuint& eboId = vao.eboId();

    glGenBuffers(1, &vboId);
    glBindBuffer(GL_ARRAY_BUFFER, vboId);

    glBufferData(GL_ARRAY_BUFFER, sizeof(VertexCPU) * vertices.size(), &vertices[0], GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexCPU), (GLvoid*)offsetof(VertexCPU, position));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(VertexCPU), (GLvoid*)offsetof(VertexCPU, normal));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(VertexCPU), (GLvoid*)offsetof(VertexCPU, uv));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(VertexCPU), (GLvoid*)offsetof(VertexCPU, boneTr0));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(VertexCPU), (GLvoid*)offsetof(VertexCPU, boneTr1));
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(VertexCPU), (GLvoid*)offsetof(VertexCPU, boneTr2));
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(VertexCPU), (GLvoid*)offsetof(VertexCPU, boneTr3));

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, eboId);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), &indices[0], GL_STATIC_DRAW);

    vao.setVertexCount(indices.size());
    vao.unbind();

    return vao;
}

static void getPoseCPU(Animation& animation, Bone& bone, float animationTime, std::vector<glm::mat4>& output, glm::mat4& parentTransform, glm::mat4& globalInverseTransform) {
    animationTime = fmod(animationTime, animation.duration());
    std::vector<KeyFrame> keyFrames = animation.getBoneKeyFrames(bone.name());
    glm::mat4 globalTransform = parentTransform;
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

        globalTransform = parentTransform * newTransform.toTransformMatrix();
    }
    output[bone.id()] = globalInverseTransform * globalTransform * bone.offset();

    for (Bone& child : bone.children()) {
        getPoseCPU(animation, child, animationTime, output, globalTransform, globalInverseTransform);
    }
}

static void getBoneTransform(const std::vector<glm::mat4>& currentPose, const std::vector<Vertex>& vertices, std::vector<VertexCPU>& verticesCPU)
{
    for (unsigned int idx = 0; idx < vertices.size(); idx++)
    {
        Vertex vertex = vertices[idx];
        VertexCPU vertexCPU = verticesCPU[idx];
        glm::mat4 boneTransform(0.0f);
        boneTransform += currentPose[vertex.boneIds.x] * vertex.boneWeights.x;
        boneTransform += currentPose[vertex.boneIds.y] * vertex.boneWeights.y;
        boneTransform += currentPose[vertex.boneIds.z] * vertex.boneWeights.z;
        boneTransform += currentPose[vertex.boneIds.w] * vertex.boneWeights.w;
        vertexCPU.boneTr0 = boneTransform[0];
        vertexCPU.boneTr1 = boneTransform[1];
        vertexCPU.boneTr2 = boneTransform[2];
        vertexCPU.boneTr3 = boneTransform[3];

        verticesCPU[idx] = vertexCPU;
    }
}

static std::vector<Vertex> vertices;
static std::vector<VertexCPU> verticesCPU;

static void CPULoop(float time, FreeCamera camera, AnimPackage& anim)
{
    glm::mat4 identity(1.0);

    std::vector<glm::mat4> currentPose;
    currentPose.resize(anim.boneCount, identity);

    getPoseCPU(anim.animation, anim.skeleton, time, currentPose, identity, anim.globalInvTr);

    getBoneTransform(currentPose, vertices, verticesCPU);

    glm::mat4 modelMatrix = glm::mat4(1.0f);
    modelMatrix = glm::translate(modelMatrix, glm::vec3(0.0f, 1.0f, 0.0f));
    //modelMatrix = glm::scale(modelMatrix, glm::vec3(.2f, .2f, .2f));
    glm::mat4 projectionMatrix = glm::perspective(glm::radians(camera._zoom), ((float) WIDTH / HEIGHT), 0.01f, 100.0f);
    glm::mat4 viewMatrix = camera.view_matrix();
    glm::mat4 viewProjectionMatrix = projectionMatrix * viewMatrix;

    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    anim.shader.start();

    anim.shader.loadMatrix4("view_projection_matrix", glm::value_ptr(viewProjectionMatrix));
    anim.shader.loadMatrix4("model_matrix", glm::value_ptr(modelMatrix));

    anim.vao.bind();

    glBufferData(GL_ARRAY_BUFFER, sizeof(VertexCPU) * verticesCPU.size(), &verticesCPU[0], GL_STATIC_DRAW);

    glActiveTexture(GL_TEXTURE0);
    anim.texture.load();

    glDrawElements(GL_TRIANGLES, anim.vao.getVertexCount(), GL_UNSIGNED_INT, 0);
    anim.vao.unbind();
    anim.shader.stop();
}

static AnimPackage initCPU(const aiScene* scene, aiMesh* mesh)
{
    std::cout << "Init anim on CPU" << std::endl;

    std::vector<GLuint> indices = {};
    GLuint boneCount = 0;
    Animation animation;
    Bone skeleton;

    glm::mat4 globalInverseTransform = assimpToGlmMatrix(scene->mRootNode->mTransformation);
    globalInverseTransform = glm::inverse(globalInverseTransform);
    
    loadModel(scene, mesh, vertices, indices, skeleton, boneCount);
    verticesCPU = std::vector<VertexCPU>(vertices.size());
    for (unsigned int idx = 0; idx < vertices.size(); idx++)
    {
        Vertex v = vertices[idx];
        VertexCPU vCPU;
        vCPU.uv = v.uv;
        vCPU.position = v.position;
        vCPU.normal = v.normal;
        vCPU.boneTr0 = glm::vec4(1, 0, 0, 0);
        vCPU.boneTr1 = glm::vec4(0, 1, 0, 0);
        vCPU.boneTr2 = glm::vec4(0, 0, 1, 0);
        vCPU.boneTr3 = glm::vec4(0, 0, 0, 1);

        verticesCPU[idx] = vCPU;
    }
    loadAnimation(scene, skeleton, animation);

    Vao vao = createVertexArrayCPU(verticesCPU, indices);

    Shader shader("V_cpu_shader.glsl", "F_shader.glsl");
    
    shader.start();
    shader.loadInt("diff_texture", 0);
    shader.stop();

    return AnimPackage(shader, vao, animation, skeleton, boneCount, globalInverseTransform);
}