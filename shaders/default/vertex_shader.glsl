#version 450 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in int light;


out vec4 vertexID;

uniform mat4 matrix;

void main()
{
	vertexID = vec4(sin(aPos.x * 0.0002), cos(aPos.y * 0.002) , cos(aPos.z), 1.0);
    gl_Position = matrix * vec4(aPos.xzy, 1.0);
}