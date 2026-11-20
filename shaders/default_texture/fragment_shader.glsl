#version 450 core

in vec2 TexCoord;
in float out_light;
flat in int out_type;


out vec4 FragColor;

uniform int TextureArraySize;
uniform sampler2DArray ourTextureArray;

void main() {
	FragColor = texture(ourTextureArray, vec3(TexCoord, out_type)) * vec4(out_light, out_light, out_light, 1.0);
}