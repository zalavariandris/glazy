#version 330 core

out vec4 FragColor;
uniform int tile_size;

float checker(in vec2 uv, in int tile_size)
{
    return mod( floor(uv.x/tile_size) + floor(uv.y/tile_size),2);
}

void main()
{
    vec3 col = vec3(0);
    float alpha = 1;

    vec2 uv = gl_FragCoord.xy;
    //col = vec3( checker(uv, tile_size) );
    float mask = checker(uv, tile_size);
    FragColor = vec4(col, mask*0.4);
}