#version 330 core
layout (location = 0) in vec3 inVertexPosition;
layout (location = 1) in vec3 inVertexNormal;
layout (location = 2) in vec2 inTextureCoordinate;

out vec3 fragmentPosition;
out vec3 fragmentVertexNormal;
out vec2 fragmentTextureCoordinate;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    // World-space position used for point / spot light distance calculations
    fragmentPosition = vec3(model * vec4(inVertexPosition, 1.0));

    // Normal matrix: inverse-transpose of the upper-left 3x3 of the model
    // matrix.  This preserves normal perpendicularity under non-uniform scale.
    fragmentVertexNormal = mat3(transpose(inverse(model))) * inVertexNormal;

    // Pass through texture coordinates unchanged
    fragmentTextureCoordinate = inTextureCoordinate;

    // Final clip-space position
    gl_Position = projection * view * model * vec4(inVertexPosition, 1.0f);
}
