#version 330 core

out vec4 FragColor;
uniform ivec2 uResolution;

in vec3 nearPoint;
in vec3 farPoint;

uniform mat4 view;
uniform mat4 projection;

float near = 0.1;
float far = 100000;

/// intersection with floor plane
bool ray_ground_intersection(in vec3 ray_origin, in vec3 ray_dir, out float t)
{
    t = - ray_origin.z/ ray_dir.z;
    return t > 0.0;
}

void main(){
    vec2 uv;
    float TILE_SIZE = 100.0;
    // get XY plane UV
    vec3 ray_origin = nearPoint;
    vec3 ray_dir = farPoint-nearPoint;
    float t;
    if(ray_ground_intersection(ray_origin, ray_dir, t)){
        vec3 fragPos3D = ray_origin + t * ray_dir;
        uv = mod(fragPos3D.xy/TILE_SIZE, 1.0);
    }else{
        uv = vec2(0,0);
    }
    

    // get screen UV
    //uv = gl_FragCoord.xy/uResolution.x/TILE_SIZE;
    
    float H = mod(uv.x, 1);
    H = step(0.5,H);
    float V = mod(uv.y, 1);
    V = step(0.5, V);
    vec3 col = vec3(mix(V, 1-V, H));
    FragColor = vec4(col,1.0);
}