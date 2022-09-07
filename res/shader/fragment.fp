#version 330 core

out vec4 frag_color;

in vec2 vert_coord;

uniform sampler2D tex;
uniform sampler1D pal;

void main()
{
	vec4 i = texture(tex, vert_coord);
	vec4 cr = texture(pal, i.x);
	frag_color = cr;
}
