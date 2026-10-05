#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 vertexColor;
layout(location = 1) out vec3 localPosition;

layout(binding = 0) uniform UniformBufferObject {
    mat4 mvp;
    vec4 tint;
} scene;

void main()
{
    gl_Position = scene.mvp * vec4(inPosition, 1.0);

    // Доп. №5: цвет вершины * цвет из ColorEdit
    vertexColor = inColor * scene.tint.rgb;

    // Передаём координаты вершины во fragment shader
    localPosition = inPosition;
}