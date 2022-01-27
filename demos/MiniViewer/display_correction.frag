#version 330 core
out vec4 FragColor;
uniform mediump sampler2D inputTexture;
uniform vec2 resolution;

uniform float gamma_correction;
uniform float gain_correction;

float sRGB_to_linear(float sRGB_value){
    return sRGB_value <= 0.04045
        ? sRGB_value / 12.92
        : pow((sRGB_value + 0.055) / 1.055, 2.4);
}

float linear_to_sRGB(float linear_value){
        return linear_value <= 0.0031308f
        ? linear_value * 12.92
        : pow(linear_value, 1.0f/2.4) * 1.055f - 0.055f;
}

vec3 sRGB_to_linear(vec3 sRGB){
    return vec3(
        sRGB_to_linear(sRGB.r),
        sRGB_to_linear(sRGB.g),
        sRGB_to_linear(sRGB.b)
        );
}

vec3 linear_to_sRGB(vec3 linear){
    return vec3(
        linear_to_sRGB(linear.r),
        linear_to_sRGB(linear.g),
        linear_to_sRGB(linear.b)
        );
}

vec3 reinhart_tonemap(vec3 hdrColor){
    return vec3(1.0) - exp(-hdrColor);
}

void main(){
    vec2 uv = gl_FragCoord.xy/resolution;
    vec3 hdrColor = texture(inputTexture, uv).rgb;
                
    // apply corrections
    vec3 mapped = hdrColor;

    // apply exposure correction
    mapped = mapped * pow(2, gain_correction);

    // exposure tone mapping
    mapped = reinhart_tonemap(mapped);

    // apply gamma correction
    mapped = pow(mapped, vec3(gamma_correction));

    // sRGB device transform
    //mapped.rgb = linear_to_sRGB(mapped.rgb);
    //mapped.rgb = pow(mapped.rgb, vec3(1.0/2.2));
    FragColor = vec4(mapped,texture(inputTexture, uv).a);
}