#include "glitter.h"
#include "utils.h"
#include "animation.h"

#include "bone.h"
#include "texture.h"
#include "vao.h"
#include "keyframe.h"
#include "shaders/shader.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
    glm::ivec4 boneIds = glm::ivec4(0);
    glm::vec4 boneWeights = glm::vec4(0.0f);
};

bool readSkeleton(Bone& boneOutput, aiNode* node, std::unordered_map<std::string, std::pair<int, glm::mat4>>& boneInfoTable) {

    if (boneInfoTable.find(node->mName.C_Str()) != boneInfoTable.end()) {
        boneOutput.setName(node->mName.C_Str());
        boneOutput.setId(boneInfoTable[boneOutput.name()].first);
        boneOutput.setOffset(boneInfoTable[boneOutput.name()].second);

        for (int i = 0; i < node->mNumChildren; i++) {
            Bone child;
            readSkeleton(child, node->mChildren[i], boneInfoTable);
            boneOutput.addChild(child);
        }
        return true;
    }

    for (int i = 0; i < node->mNumChildren; i++) {
        if (readSkeleton(boneOutput, node->mChildren[i], boneInfoTable)) {
            return true;
        }
    }

    return false;
}

void loadModel(const aiScene* scene, aiMesh* mesh, std::vector<Vertex>& verticesOutput, std::vector<GLuint>& indicesOutput, Bone& skeletonOutput, GLuint&nBoneCount) {
    verticesOutput = {};
    indicesOutput = {};

    for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
        //process position 
        Vertex vertex;
        glm::vec3 vector;
        vector.x = mesh->mVertices[i].x;
        vector.y = mesh->mVertices[i].y;
        vector.z = mesh->mVertices[i].z;
        vertex.position = vector;
        //process normal
        vector.x = mesh->mNormals[i].x;
        vector.y = mesh->mNormals[i].y;
        vector.z = mesh->mNormals[i].z;
        vertex.normal = vector;
        //process uv
        glm::vec2 vec;
        vec.x = mesh->mTextureCoords[0][i].x;
        vec.y = mesh->mTextureCoords[0][i].y;
        vertex.uv = vec;

        vertex.boneIds = glm::ivec4(0);
        vertex.boneWeights = glm::vec4(0.0f);

        verticesOutput.push_back(vertex);
    }

    std::unordered_map<std::string, std::pair<int, glm::mat4>> boneInfo = {};
    std::vector<GLuint> boneCounts;
    boneCounts.resize(verticesOutput.size(), 0);
    nBoneCount = mesh->mNumBones;

    for (int i = 0; i < nBoneCount; i++) {
        aiBone* bone = mesh->mBones[i];
        glm::mat4 m = assimpToGlmMatrix(bone->mOffsetMatrix);
        boneInfo[bone->mName.C_Str()] = { i, m };

        for (int j = 0; j < bone->mNumWeights; j++) {
            GLuint id = bone->mWeights[j].mVertexId;
            float weight = bone->mWeights[j].mWeight;
            boneCounts[id]++;
            switch (boneCounts[id]) {
            case 1:
                verticesOutput[id].boneIds.x = i;
                verticesOutput[id].boneWeights.x = weight;
                break;
            case 2:
                verticesOutput[id].boneIds.y = i;
                verticesOutput[id].boneWeights.y = weight;
                break;
            case 3:
                verticesOutput[id].boneIds.z = i;
                verticesOutput[id].boneWeights.z = weight;
                break;
            case 4:
                verticesOutput[id].boneIds.w = i;
                verticesOutput[id].boneWeights.w = weight;
                break;
            default:
                //std::cout << "err: unable to allocate bone to vertex" << std::endl;
                break;

            }
        }
    }

    for (int i = 0; i < verticesOutput.size(); i++) {
        glm::vec4 & boneWeights = verticesOutput[i].boneWeights;
        float totalWeight = boneWeights.x + boneWeights.y + boneWeights.z + boneWeights.w;
        if (totalWeight > 0.0f) {
            verticesOutput[i].boneWeights = glm::vec4(
                boneWeights.x / totalWeight,
                boneWeights.y / totalWeight,
                boneWeights.z / totalWeight,
                boneWeights.w / totalWeight
            );
        }
    }

    for (int i = 0; i < mesh->mNumFaces; i++) {
        aiFace& face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++)
            indicesOutput.push_back(face.mIndices[j]);
    }

    readSkeleton(skeletonOutput, scene->mRootNode, boneInfo);
}

void loadAnimation(const aiScene* scene, Bone bone, Animation& animation) {
    aiAnimation* anim = scene->mAnimations[0];

    if (anim->mTicksPerSecond != 0.0f)
        animation.setTPS(anim->mTicksPerSecond);
    else
        animation.setTPS(1);

    animation.setDuration(anim->mDuration * anim->mTicksPerSecond);

    for (int i = 0; i < anim->mNumChannels; i++) {
        std::vector<KeyFrame> keyFrames;
        aiNodeAnim* channel = anim->mChannels[i];

        unsigned int numPositions = channel->mNumPositionKeys;
        unsigned int numRotations = channel->mNumRotationKeys;
        unsigned int numScales = channel->mNumScalingKeys;

        unsigned int nbKeyFrames = std::max(std::max(numPositions, numRotations), numScales);
        for (unsigned int i = 0; i < nbKeyFrames; i++)
        {
            unsigned int idxPos = std::min(i, numPositions - 1);
            unsigned int idxRot = std::min(i, numRotations - 1);
            unsigned int idxScale = std::min(i, numScales - 1);

            KeyFrame keyFrame;
            keyFrame.transform = Transformation(
                assimpToGlmVec3(channel->mPositionKeys[idxPos].mValue),
                assimpToGlmQuat(channel->mRotationKeys[idxRot].mValue),
                assimpToGlmVec3(channel->mScalingKeys[idxScale].mValue));

            keyFrame.timeStamp = channel->mPositionKeys[i].mTime;
            keyFrames.push_back(keyFrame);
        }
        for (const KeyFrame& k : keyFrames)
            animation.addBoneKeyFrame(channel->mNodeName.C_Str(), k);
    }
}

Vao createVertexArray(std::vector<Vertex>& vertices, std::vector<GLuint> indices) {
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

void getPose(Animation& animation, Bone& bone, float animationTime, std::vector<glm::mat4>& output, glm::mat4& parentTransform, glm::mat4& globalInverseTransform) {
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

        Transformation newTransform = Transformation::interpolate(currentKeyFrame.transform, nextKeyFrame.transform, progression);

        globalTransform = parentTransform * newTransform.toTransformMatrix();
    }
    output[bone.id()] = globalInverseTransform * globalTransform * bone.offset();

    for (Bone& child : bone.children()) {
        getPose(animation, child, animationTime, output, globalTransform, globalInverseTransform);
    }
}

static void OnWindowResize(GLFWwindow* window, int width, int height)
{
    AppState* app = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    assert(app);
    app->screen_width = width;
    app->screen_height = height;

    glViewport(0, 0, width, height);
}

static void OnMouseMove(GLFWwindow* window, double xpos, double ypos)
{
    AppState* app = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    assert(app);
    app->camera.on_mouse_move(float(xpos), float(ypos));
}

static void OnMouseScroll(GLFWwindow* window, double xoffset, double yoffset)
{
    (void)xoffset;
    AppState* app = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    assert(app);
    app->camera.on_mouse_scroll(float(yoffset));
}

static void HandleInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, true);
        return;
    }

    AppState* app = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    assert(app);
    FreeCamera& camera = app->camera;

    const struct KeyDelta
    {
        int key;
        glm::vec3 delta;
    };
    KeyDelta key_delta[] =
    {
        {GLFW_KEY_W, +camera._front},
        {GLFW_KEY_S, -camera._front},
        {GLFW_KEY_D, +camera._right},
        {GLFW_KEY_A, -camera._right},
        {GLFW_KEY_E, +camera._up},
        {GLFW_KEY_Q, -camera._up},
    };
    for (KeyDelta keyDelta : key_delta)
    {
        if (glfwGetKey(window, keyDelta.key) == GLFW_PRESS)
        {
            camera.on_keyboard_move(keyDelta.delta, app->_dt);
            break;
        }
    }
}

int main(int argc, char ** argv) {

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    AppState app;
    GLFWwindow* window = glfwCreateWindow(app.screen_width, app.screen_height, "App", nullptr, nullptr);
    assert(window && "Failed to create window.");
    glfwSetWindowUserPointer(window, &app);
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, OnWindowResize);
    glfwSetCursorPosCallback(window, OnMouseMove);
    glfwSetScrollCallback(window, OnMouseScroll);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    const int glad_ok = gladLoadGL();
    assert(glad_ok > 0);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);

    app.camera.force_refresh();

    Assimp::Importer importer;
    const char* filePath = "model.dae";
    const aiScene* scene = importer.ReadFile(Texture::DIR_PATH + filePath, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenSmoothNormals);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cout << "ERROR::ASSIMP::" << importer.GetErrorString() << std::endl;
        exit(1);
    }

    aiMesh* mesh = scene->mMeshes[0];

    std::vector<Vertex> vertices = {};
    std::vector<GLuint> indices = {};
    GLuint boneCount = 0;
    Animation animation;
    Bone skeleton;

    glm::mat4 globalInverseTransform = assimpToGlmMatrix(scene->mRootNode->mTransformation);
    globalInverseTransform = glm::inverse(globalInverseTransform);
    

    loadModel(scene, mesh, vertices, indices, skeleton, boneCount);
    loadAnimation(scene, skeleton, animation);

    Vao vao = createVertexArray(vertices, indices);
    Texture diffuseTexture = Texture("diffuse.png");

    glm::mat4 identity(1.0);

    std::vector<glm::mat4> currentPose;
    currentPose.resize(boneCount, identity);

    Shader shader("V_shader.glsl", "F_shader.glsl");

    //GLuint viewProjectionMatrixLocation = glGetUniformLocation(shader, "view_projection_matrix");
    //GLuint modelMatrixLocation = glGetUniformLocation(shader, "model_matrix");
    //GLuint boneMatricesLocation = glGetUniformLocation(shader, "bone_transforms");
    //GLuint textureLocation = glGetUniformLocation(shader, "diff_texture");

    glm::mat4 modelMatrix = glm::mat4(1.0f);
    modelMatrix = glm::translate(modelMatrix, glm::vec3(0.0f, 1.0f, 0.0f));
    modelMatrix = glm::scale(modelMatrix, glm::vec3(.2f, .2f, .2f));

    shader.start();
    shader.loadInt("diff_texture", 0);
    shader.stop();

    while (!glfwWindowShouldClose(window))
    {
        const float current_time = float(glfwGetTime());
        app._dt = current_time - app._last_frame_time;
        //app._last_frame_time = current_time;
        HandleInput(window);
        getPose(animation, skeleton, app._dt, currentPose, identity, globalInverseTransform);

        glm::mat4 projectionMatrix = glm::perspective(glm::radians(app.camera._zoom), ((float) app.screen_width / app.screen_height), 0.01f, 100.0f);
        glm::mat4 viewMatrix = app.camera.view_matrix();
        glm::mat4 viewProjectionMatrix = projectionMatrix * viewMatrix;

        glClearColor(0.f, 0.f, 0.f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        shader.start();

        shader.loadMatrix4("view_projection_matrix", viewProjectionMatrix);
        shader.loadMatrix4("model_matrix", modelMatrix);
        shader.loadMatrix4("bone_transforms", currentPose[0], GLsizei(boneCount));
        //glUniformMatrix4fv(viewProjectionMatrixLocation, 1, GL_FALSE, glm::value_ptr(viewProjectionMatrix));
        //glUniformMatrix4fv(modelMatrixLocation, 1, GL_FALSE, glm::value_ptr(modelMatrix));
        //glUniformMatrix4fv(boneMatricesLocation, GLsizei(boneCount), GL_FALSE, glm::value_ptr(currentPose[0]));

        vao.bind();
        glActiveTexture(GL_TEXTURE0);
        diffuseTexture.load();

        glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);

        vao.unbind();
        shader.stop();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();

    return 0;
}