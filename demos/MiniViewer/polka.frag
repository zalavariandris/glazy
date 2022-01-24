#version 330 core
#define LINE_WIDTH 1
#define TILE_SIZE 1

out vec4 FragColor;
uniform vec2 uResolution;
uniform float radius;

in vec3 nearPoint;
in vec3 farPoint;

in mat4 fragView;
in mat4 fragProj;

float near = 0.1;
float far = 10000;

uniform sampler2D textureMap;

/// gives the grid lines alpha factor
float grid(in vec2 uv)
{
    vec2 grid = 1.0 * fwidth(uv)/mod(uv, TILE_SIZE);
    float line = step(1.0/LINE_WIDTH, max(grid.x, grid.y));
    return line;
}

///

///
float computeDepth(vec3 pos) {
    vec4 clip_space_pos = fragProj * fragView * vec4(pos.xyz, 1.0);
    return (clip_space_pos.z / clip_space_pos.w);
}

/// intersection with floor plane
bool ray_ground_intersection(in vec3 ray_origin, in vec3 ray_dir, out float t)
{
    t = - ray_origin.z/ ray_dir.z;
    return t > 0.0;
}

void main(){
    vec3 ray_origin = nearPoint;
    vec3 ray_dir = farPoint-nearPoint;

    vec3 col = vec3(1,1,1);
    float alpha = 0.0;
    float t;
    if(ray_ground_intersection(ray_origin, ray_dir, t)){
        vec3 fragPos3D = ray_origin + t * ray_dir;
        gl_FragDepth = computeDepth(fragPos3D);

        vec2 offset = vec2(0.25/2);
        vec2 tile_size = vec2(0.25);
        vec2 uv = mod(fragPos3D.xy+offset, tile_size);
        float distance = length(uv-offset);
        float r = fwidth(uv).x*3;
        distance = step(0.02, distance);
        alpha=1-distance;
        //col = vec3(uv, 0);
    }
    
    FragColor = vec4(col, alpha*0.3);

}            