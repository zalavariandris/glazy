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

uniform sampler2D textureMap;

/// gives the grid lines alpha factor
float grid(in vec2 uv)
{
    vec2 grid = 1.0 * fwidth(uv)/mod(uv, TILE_SIZE);
    float line = step(1.0/LINE_WIDTH, max(grid.x, grid.y));
    return line;
}

///
float computeDepth(vec3 pos, mat4 proj, mat4 view) {
    vec4 clip_space_pos = proj * view * vec4(pos.xyz, 1.0);
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

    vec3 col = vec3(0);
    float alpha = 0.0;
    float t;
    if(ray_ground_intersection(ray_origin, ray_dir, t)){
        vec3 fragPos3D = ray_origin + t * ray_dir;
        //gl_FragDepth = computeDepth(fragPos3D);

        vec2 offset_factor = vec2(0.5);
        vec2 uv = mod(fragPos3D.xy/TILE_SIZE+offset_factor, 1.0);
        float distance = length(uv-offset_factor);
        distance = step(RELATIVE_RADIUS, distance);
        alpha=1-distance;
        //col = vec3(uv, 0);
    }
    
    FragColor = vec4(col, alpha*0.3);
}