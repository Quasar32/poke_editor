#version 330 core

out vec4 frag_color;

in vec2 tex_coord;

uniform sampler2D tex;
uniform sampler1D pal;

void main()
{
	vec4 i;

	i = texture(tex, tex_coord);
	frag_color = texture(pal, i.x);
}
