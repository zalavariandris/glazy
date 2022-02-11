#pragma once

const char* IMDRAW_VERTEX_SHADER = R"(#version 330 core
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
};
)";

const char* IMDRAW_FRAGMENT_SHADER = R"(#version 330 core
out vec4 FragColor;

uniform vec3 color;
uniform bool useTextureMap;
uniform sampler2D textureMap;

uniform bool useVertexColor;

uniform vec2 uv_tiling;
uniform vec2 uv_offset;

in vec4 ScreenPos;
				
in vec3 vNormal;
in vec3 vColor;
in vec2 vUV;

uniform float opacity;

struct Light
{
	vec3 dir;
	vec3 color;
};

// Dash
flat in vec4 StartPos;
void dash(){
	float dashSize = 0.05;
	float gapSize = 0.05;
	float dist = abs(ScreenPos.x-StartPos.x) + abs(ScreenPos.y-StartPos.y);
	if (fract(dist / (dashSize + gapSize)) > dashSize/(dashSize + gapSize)){
		discard;
	}
}

void main()
{
	// color
	vec3 col = color;
	float alpha = 1.0;
	if(useTextureMap){
		vec4 tex = texture(textureMap, vUV*uv_tiling+uv_offset);
		col *= tex.rgb;
		alpha *= tex.a;
	}

	if(useVertexColor){
		col*=vColor;
	}

	// Lighting
	vec3 norm = normalize(vNormal);
	
	// ambient
	vec3 diff = vec3(0.03);

	//keylight
	Light keylight = Light(
		normalize(vec3(0.7,1,-0.5)), 
		vec3(1,1,1)
	);
	diff += max(dot(norm, keylight.dir), 0.0)*keylight.color;

	Light filllight = Light(
		normalize(vec3(-0.7,0.5,-0.7)), 
		vec3(0.5,0.4,0.3)*0.3
	);
	diff += max(dot(norm, filllight.dir), 0.0)*filllight.color;

	Light rimlight = Light(
		normalize(vec3(0.6,0.1,0.9)), 
		vec3(0.5,0.5,0.7)*0.3
	);
	diff += max(dot(norm, rimlight.dir), 0.0)*rimlight.color;
	//col*=diff;
	// dashed
	//dash();

	// calc frag color
	FragColor = vec4(col, alpha*(1.0-opacity));
};
)";