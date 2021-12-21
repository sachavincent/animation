#version 430 core

in vec2 tex_cord;
in vec3 v_normal;
in vec3 v_pos;
out vec4 color;

uniform sampler2D diff_texture;

vec3 lightPos = vec3(0.2, 1.0, -3.0);
    
void main()
{
    vec3 lightDir = normalize(lightPos - v_pos);
    float diff = max(dot(v_normal, lightDir), 0.2);
    vec3 dCol = diff * texture(diff_texture, tex_cord).rgb; 
    color = vec4(dCol, 1);
}