#version 330

in vec4 color;
in vec2 coords;
in float factor;

out vec4 outColor;

uniform sampler2D tex;

void main() {
    vec4 oColor = mix(color, color * texture(tex, coords), factor);

    // float gamma = 2.2;
    // outColor = vec4(pow(oColor.rgb, vec3(1.0/gamma)), oColor.w);
    // outColor = pow(oColor, vec4(1.0f/gamma));

    // float gamma = 2.2;
    // vec3 orgb = pow(oColor.rgb, vec3(1.0f/gamma));
    // orgb = floor(orgb * 15) / 15;
    // orgb = pow(orgb, vec3(gamma));

    // outColor = vec4(orgb, oColor.a);
    // outColor = vec4(oColor.xyz, 1);
    outColor = oColor;
}