#ifndef SCENE_OBJECT_SHADER_H
#define SCENE_OBJECT_SHADER_H

#include "shader.h"

class SceneObjectShader: public Shader
{
private:
   SceneObjectShader(): Shader("V_sceneobj.glsl", "F_sceneobj.glsl") {
   }

public:
   static SceneObjectShader& getInstance()
   {
       static SceneObjectShader instance;
       return instance;
   }
};

#endif // SCENE_OBJECT_SHADER_H
