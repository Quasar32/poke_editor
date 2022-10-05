#version 330 core

uniform mat4 projection;

in VS_OUT {
	uint id; 
} gs_in[];

out vec2 tex_coord;

layout (points) in;
layout (triangle_strip, max_vertices = 4) out;

void main()
{
	uint id = gs_in[0].id & 255u;
	vec2 tile = vec2(id & 15u, id >> 4u) / 16.0; 

	float b = 1.0 / 4096.0;
	float s = 1.0 / 16.0;

	mat4 proj;
	vec4 pos;

	proj = projection;
	proj *= float(bool(id));

	pos = gl_in[0].gl_Position;

	gl_Position = proj * pos;
	tex_coord = vec2(tile.x + b, tile.y + b);
	EmitVertex();

	gl_Position = proj * (pos + vec4(1.0, 0.0, 0.0, 0.0));
	tex_coord = vec2(tile.x + s - b, tile.y + b);
	EmitVertex();

	gl_Position = proj * (pos + vec4(0.0, 1.0, 0.0, 0.0));
	tex_coord = vec2(tile.x + b, tile.y + s - b);
	EmitVertex();

	gl_Position = proj * (pos + vec4(1.0, 1.0, 0.0, 0.0));
	tex_coord = vec2(tile.x + s - b, tile.y + s - b);
	EmitVertex();

	EndPrimitive();
}
