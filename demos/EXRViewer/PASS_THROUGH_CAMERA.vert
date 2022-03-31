#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

out vec3 nearPoint;
out vec3 farPoint;

uniform bool use_geometry;

vec3 UnprojectPoint(float x, float y, float z, mat4 view, mat4 projection) {
    mat4 viewInv = inverse(view);
    mat4 projInv = inverse(projection);
    vec4 unprojectedPoint =  viewInv * projInv * vec4(x, y, z, 1.0);
    return unprojectedPoint.xyz / unprojectedPoint.w;
}
			
void main()
{
    nearPoint = UnprojectPoint(aPos.x, aPos.y, 0.0, view, projection).xyz;
    farPoint = UnprojectPoint(aPos.x, aPos.y, 1.0, view, projection).xyz;
    gl_Position = use_geometry ? (projection * view * model * vec4(aPos, 1.0)) : vec4(aPos, 1.0);
}