#ifndef KEYFRAME_H
#define KEYFRAME_H

#include "transformation.h"

struct KeyFrame
{
    Transformation transform;
    float timeStamp;
};

#endif // KEYFRAME_H