layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec3 vertexNormal;
layout(location = 2) in float vertexTextureIndex;
layout(location = 3) in vec2 vertexTextureCoords;
layout(location = 4) in vec3 vertexColor;

out vec3 fragmentPosition;
out vec3 fragmentNormal;
out float fragmentTextureIndex;
out vec2 fragmentTextureCoords;
out vec3 fragmentColor;

uniform mat4 model;
uniform mat4 VP;

void main()
{
	vec4 worldSpace = model * vec4(vertexPosition, 1.0);
	gl_Position = VP * worldSpace;

	fragmentPosition = worldSpace.xyz;
	fragmentNormal = vertexNormal;
	fragmentTextureIndex = vertexTextureIndex;
	fragmentTextureCoords = vertexTextureCoords;
	fragmentColor = vertexColor;
}
