#version 330 core
layout (location = 0) in vec3 position;
layout (location = 2) in vec3 normal;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

out vec3 Eye;
out vec3 Normal;

void main() {

	mat4 modelView =  view * model;
	Eye = normalize( vec3( modelView * vec4( position, 1.0 ) ) );
	mat3 normalMatrix = transpose(inverse(mat3(modelView)));
	Normal = normalize( normalMatrix * normal );
	gl_PointSize = 5.0;
	gl_Position = projection * view * model * vec4( position, 1.0 );
}