#version 330 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D ourTexture;
uniform bool useTexture;

void main()
{
	FragColor = useTexture
		? texture(ourTexture, TexCoord)
		: vec4(0.15, 0.15, 0.20, 1.0);
}
