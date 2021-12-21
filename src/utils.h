#pragma once
#include <glad/glad.h>
#include <iostream>
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <assimp/scene.h>

#include "texture.h"
#include "shader.h"
#include "vao.h"
#include "animation.h"
#include "bone.h"

inline glm::mat4 assimpToGlmMatrix(aiMatrix4x4 mat) {
	glm::mat4 m;
	for (int y = 0; y < 4; y++)
	{
		for (int x = 0; x < 4; x++)
		{
			m[x][y] = mat[y][x];
		}
	}
	return m;
}

inline glm::vec3 assimpToGlmVec3(aiVector3D vec) {
	return glm::vec3(vec.x, vec.y, vec.z);
}

inline glm::quat assimpToGlmQuat(aiQuaternion quat) {
	glm::quat q;
	q.x = quat.x;
	q.y = quat.y;
	q.z = quat.z;
	q.w = quat.w;

	return q;
}

struct FreeCamera
{
	FreeCamera(glm::vec3 pos) : _position(pos) {}

	glm::vec3 _position = glm::vec3(0.f);
	glm::vec3 _front = glm::vec3(0.f, 0.f, 1.f);
	glm::vec3 _up = glm::vec3(0.f, 1.f, 0.f);
	glm::vec3 _right = glm::vec3(1.f, 0.f, 0.f);
	glm::vec3 _world_up = glm::vec3(0.f, 1.f, 0.f);
	float _yaw = -90.f;
	float _pitch = 0.f;
	float _movement_speed = 0.5f;
	float _mouse_sensitivity = 0.1f;
	float _zoom = -65.f;

	float _mouse_last_x = NAN;
	float _mouse_last_y = NAN;

	glm::mat4 view_matrix() const
	{
		return glm::lookAt(_position, _position + _front, _up);
	}

	void on_keyboard_move(glm::vec3 delta)
	{
		float velocity = _movement_speed;
		_position += delta * velocity;
	}

	void on_mouse_scroll(float yoffset)
	{
		_zoom -= yoffset;
	}

	void on_mouse_move(float x, float y)
	{
		if (std::isnan(_mouse_last_x) || std::isnan(_mouse_last_y))
		{
			_mouse_last_x = x;
			_mouse_last_y = y;
		}
		const float xoffset = (x - _mouse_last_x) * _mouse_sensitivity;
		const float yoffset = (_mouse_last_y - y) * _mouse_sensitivity;
		_mouse_last_x = x;
		_mouse_last_y = y;
		_yaw += xoffset;
		_pitch += yoffset;
		force_refresh();
	}

	void force_refresh()
	{
		_pitch = std::min(std::max(_pitch, -89.0f), 89.0f);

		_front.x = cos(glm::radians(_yaw)) * cos(glm::radians(_pitch));
		_front.y = sin(glm::radians(_pitch));
		_front.z = sin(glm::radians(_yaw)) * cos(glm::radians(_pitch));
		_front = glm::normalize(_front);
		_right = glm::normalize(glm::cross(_front, _world_up));
		_up = glm::normalize(glm::cross(_right, _front));
	}
};

int WIDTH = 800;
int HEIGHT = 600;

enum class Mode
{
	CPU,
	GPU,
	GPU_DUAL
};

struct AppState
{
	FreeCamera camera = FreeCamera(glm::vec3(0.0f, -5.0f, 7.0f));
	Mode mode = Mode::CPU;
	Mode previous_mode = Mode::CPU;
	bool paused = false;
	bool reset = false;
	float paused_time;
	float pause_delay = 0;
};


struct AnimPackage
{
	AnimPackage(Shader s, Vao v, Texture t, Animation a, Bone b, int c, glm::mat4 g = glm::mat4(1)) :
		shader(s), vao(v), texture(t), animation(a), skeleton(b), boneCount(c), globalInvTr(g)
	{}

	Shader shader;
	Vao vao;
	Texture texture;
	Animation animation;
	Bone skeleton;
	GLuint boneCount;
	glm::mat4 globalInvTr;
};

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

void loadModel(const aiScene* scene, aiMesh* mesh, std::vector<Vertex>& verticesOutput, std::vector<GLuint>& indicesOutput, Bone& skeletonOutput, GLuint& nBoneCount) {
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
		glm::vec4& boneWeights = verticesOutput[i].boneWeights;
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