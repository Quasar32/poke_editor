#version 330 core

uniform mat4 projection;
uniform vec2 scroll; 

layout (location = 0) in uint id;

out VS_OUT {
	uint id;
} vs_out;

void main()
{
	vec2 t = vec2(gl_VertexID, gl_VertexID >> 5);
	vec2 p = mod(t - scroll, 32) - 1.0;
	gl_Position = vec4(p, 0, 1);
	vs_out.id = id;
}
