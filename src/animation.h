#ifndef ANIMATION_H
#define ANIMATION_H

#include <string>
#include <unordered_map>

#include "bone.h"
#include "keyframe.h"

class Animation {
public:
    Animation();

    inline const float& duration() const { return this->_duration; }
    inline void setDuration(const float& duration) { this->_duration = duration; }
    inline const float& TPS() const { return this->_ticksPerSecond; }
    inline void setTPS(const float& ticksPerSecond) { this->_ticksPerSecond = ticksPerSecond; }

    void addBoneKeyFrame(std::string, KeyFrame);

    std::vector<KeyFrame> getBoneKeyFrames(std::string);
private:
    float _duration;
    float _ticksPerSecond;

    std::unordered_map<std::string, std::vector<KeyFrame>> _boneKeyFrames;
};

#endif // ANIMATION_H