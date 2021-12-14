#include "animation.h"

Animation::Animation() : _duration(0.0f), _ticksPerSecond(20.0f), _boneKeyFrames({})
{

}

void Animation::addBoneKeyFrame(std::string boneName, KeyFrame keyFrame)
{
	_boneKeyFrames[boneName].push_back(keyFrame);
}

std::vector<KeyFrame> Animation::getBoneKeyFrames(std::string boneName)
{
	return _boneKeyFrames[boneName];
}