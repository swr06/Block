#version 330 core

// Narkowicz 2015, "ACES Filmic Tone Mapping Curve"
vec3 aces(vec3 x) {
  const float a = 2.51;
  const float b = 0.03;
  const float c = 2.43;
  const float d = 0.59;
  const float e = 0.14;
  return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

in vec2 v_TexCoords;
layout(location = 0) out vec4 o_Color;

uniform sampler2D u_FramebufferTexture;
uniform sampler2D u_VolumetricTexture;
uniform float u_Exposure = 1.0f;

const vec3 SUN_COLOR = vec3(196.0f / 255.0f, 224.0f / 255.0f, 253.0f / 255.0f);

void main()
{
    vec3 HDR = texture(u_FramebufferTexture, v_TexCoords).rgb;
    float volumetric = texture(u_VolumetricTexture, v_TexCoords).r;
    vec3 volumetric_col = (volumetric * SUN_COLOR);

    vec3 final_color;
    final_color = HDR + (volumetric_col * 0.1f);

    o_Color = vec4(aces(final_color), 1.0);
}