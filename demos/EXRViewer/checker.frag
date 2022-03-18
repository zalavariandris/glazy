#version 330 core
#define LINE_WIDTH 1
#define TILE_SIZE 100
#define RELATIVE_RADIUS 0.1

out vec4 FragColor;
uniform vec2 uResolution;
uniform float radius;

in vec3 nearPoint;
in vec3 farPoint;

float near = 0.1;
float far = 100000;



float checker(in vec2 uv){
    return 0.0;
}

void main(){
    vec3 ray_origin = nearPoint;
    vec3 ray_dir = farPoint-nearPoint;

    vec3 col = vec3(1,1,1);
    float alpha = checker(gl_FragCoord);

    
    FragColor = vec4(col, alpha);
}