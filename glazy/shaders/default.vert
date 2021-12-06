#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aUV;
layout (location = 2) in vec3 aNormal;
layout (location = 3) in vec3 aColor;
layout (location = 4) in mat4 instanceMatrix;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;
uniform bool useInstanceMatrix;

out vec2 vUV;
out vec3 vNormal;
out vec3 vColor;
out vec4 ScreenPos;

// Dash
flat out vec4 StartPos;
void dash(){
	StartPos = ScreenPos;
}
				
// Main
void main()
{
	vUV = aUV;
	vNormal = aNormal;
	vColor = aColor;

	mat4 viewModel = view*model;
	if(useInstanceMatrix){
		viewModel *= instanceMatrix;
	}

	ScreenPos = vec4(projection * viewModel * vec4(aPos, 1.0));
	//dash();
	gl_Position = projection * viewModel * vec4(aPos, 1.0);
}