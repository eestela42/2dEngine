#version 450 core

in vec4 vertexID;

out vec4 FragColor;


void main() {
	vec4 color = vertexID;

	FragColor = color;
}