#version 430 core
out vec4 FragColor;
in vec2 TexCoord;
uniform sampler2D TrailMap;

void main()
{
	FragColor = texture(TrailMap, TexCoord);
}