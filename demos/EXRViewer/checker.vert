#version 330 core
layout (location = 0) in vec3 aPos;
uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

void main()
{
    vec4 pos = vec4(aPos,1);
    pos = projection * view * pos;
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}