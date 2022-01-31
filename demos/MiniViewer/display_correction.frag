#version 330 core
out vec4 FragColor;
uniform mediump sampler2D inputTexture;
uniform vec2 resolution;

uniform float gamma_correction;
uniform float gain_correction;
uniform int convert_to_device; // 0:linear | 1:sRGB | 2:Rec709

float sRGB_to_linear(float channel){
    return channel <= 0.04045
        ? channel / 12.92
        : pow((channel + 0.055) / 1.055, 2.4);
}

vec3 sRGB_to_linear(vec3 color){
    return vec3(
        sRGB_to_linear(color.r),
        sRGB_to_linear(color.g),
        sRGB_to_linear(color.b)
        );
}

float linear_to_sRGB(float channel){
        return channel <= 0.0031308f
        ? channel * 12.92
        : pow(channel, 1.0f/2.4) * 1.055f - 0.055f;
}

vec3 linear_to_sRGB(vec3 color){
    return vec3(
        linear_to_sRGB(color.r),
        linear_to_sRGB(color.g),
        linear_to_sRGB(color.b)
    );
}

const float REC709_ALPHA = 0.099f;
float linear_to_rec709(float channel) {
    if(channel <= 0.018f)
        return 4.5f * channel;
    else
        return (1.0 + REC709_ALPHA) * pow(channel, 0.45f) - REC709_ALPHA;
}

vec3 linear_to_rec709(vec3 rgb) {
    return vec3(
        linear_to_rec709(rgb.r),
        linear_to_rec709(rgb.g),
        linear_to_rec709(rgb.b)
    );
}


vec3 reinhart_tonemap(vec3 hdrColor){
    return vec3(1.0) - exp(-hdrColor);
}

void main(){
    vec2 uv = gl_FragCoord.xy/resolution;
    vec3 rawColor = texture(inputTexture, uv).rgb;
                
    // apply corrections
    vec3 color = rawColor;

    // apply exposure correction
    color = color * pow(2, gain_correction);

    // exposure tone mapping
    //mapped = reinhart_tonemap(mapped);

    // apply gamma correction
    color = pow(color, vec3(gamma_correction));

    // convert color to device
    if(convert_to_device==0) // linear, no conversion
    {

    }
    else if(convert_to_device==1) // sRGB
    {
        color = linear_to_sRGB(color);
    }
    if(convert_to_device==2) // Rec.709
    {
        color = linear_to_rec709(color);
    }

    FragColor = vec4(color, texture(inputTexture, uv).a);
}