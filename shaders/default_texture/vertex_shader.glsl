#version 450 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in int light;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in int type;


out vec2 TexCoord;
out float out_light;
flat out int out_type;

uniform mat4 matrix;

void main()
{
	out_light = 0.5f + 0.5f * float(light);
	TexCoord = aTexCoord;
	out_type =  type;
    gl_Position = matrix * vec4(aPos.xzy, 1.0);
}