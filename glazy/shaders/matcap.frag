#version 330 core

in vec3 Eye;
in vec3 Normal;

uniform sampler2D matCap;

out vec4 FragColor;

void main(){

	vec3 normal = normalize(Normal);
	vec3 R=reflect(Eye, normal);

	float m = 2. * sqrt( pow( R.x, 2. ) + pow( R.y, 2. ) + pow( R.z + 1., 2. ) );
	vec2 vN = R.xy / m + 0.5;

	vec3 color = texture( matCap, vN ).rgb;

	FragColor = vec4( color, 1.0);
}