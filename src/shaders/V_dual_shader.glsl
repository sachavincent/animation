#version 430 core
layout (location = 0) in vec3 position; 
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 uv;
layout (location = 3) in ivec4 boneIds;
layout (location = 4) in vec4 boneWeights;

out vec2 tex_cord;
out vec3 v_normal;
out vec3 v_pos;
out vec4 bw;

uniform mat2x4 bone_transforms[100];

uniform mat4 view_projection_matrix;
uniform mat4 model_matrix;

mat4x4 DQtoMat(vec4 real, vec4 dual) {
	mat4x4 m;
	float len2 = dot(real, real);
	float w = real.w, x = real.x, y = real.y, z = real.z;
	float t0 = dual.w, t1 = dual.x, t2 = dual.y, t3 = dual.z;

	m[0][0] = w * w + x * x - y * y - z * z;
	m[1][0] = 2 * x * y - 2 * w * z;
	m[2][0] = 2 * x * z + 2 * w * y;

	m[0][1] = 2 * x * y + 2 * w * z;
	m[1][1] = w * w + y * y - x * x - z * z;
	m[2][1] = 2 * y * z - 2 * w * x;

	m[0][2] = 2 * x * z - 2 * w * y;
	m[1][2] = 2 * y * z + 2 * w * x;
	m[2][2] = w * w + z * z - x * x - y * y;

	m[3][0] = -2 * t0 * x + 2 * w * t1 - 2 * t2 * z + 2 * y * t3;
	m[3][1] = -2 * t0 * y + 2 * t1 * z - 2 * x * t3 + 2 * w * t2;
	m[3][2] = -2 * t0 * z + 2 * x * t2 + 2 * w * t3 - 2 * t1 * y;

	m[0][3] = 0;
	m[1][3] = 0;
	m[2][3] = 0;
	m[3][3] = len2;
	m /= len2;

	return m;
}

void main()
{
    bw = vec4(0);
    if(boneIds.x == 1)
        bw.z = boneIds.x;
    //boneWeights = normalize(boneWeights);

	mat2x4 dq0 = bone_transforms[boneIds[0]];
	mat2x4 dq1 = bone_transforms[boneIds[1]];
	mat2x4 dq2 = bone_transforms[boneIds[2]];
	mat2x4 dq3 = bone_transforms[boneIds[3]];

	if (dot(dq0[0], dq1[0]) < 0.0) dq1 *= -1.0;
	if (dot(dq0[0], dq2[0]) < 0.0) dq2 *= -1.0;
	if (dot(dq0[0], dq3[0]) < 0.0) dq3 *= -1.0;
	
	mat2x4 blendDQ = dq0 * boneWeights[0];
	blendDQ += dq1 * boneWeights[1];
	blendDQ += dq2 * boneWeights[2];
	blendDQ += dq3 * boneWeights[3];

    mat4 boneTransform = DQtoMat(blendDQ[0], blendDQ[1]);

    vec4 pos = boneTransform * vec4(position, 1.0);
    gl_Position = view_projection_matrix * model_matrix * pos;
    v_pos = vec3(model_matrix * boneTransform * pos);
    tex_cord = uv;
    v_normal = mat3(transpose(inverse(model_matrix * boneTransform))) * normal;
    v_normal = normalize(v_normal);
}