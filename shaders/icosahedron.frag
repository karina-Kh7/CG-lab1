#version 450

layout(location = 0) in vec3 vertexColor;
layout(location = 1) in vec3 localPosition;

layout(location = 0) out vec4 outColor;

void main()
{
    vec3 normal = normalize(
        cross(
            dFdx(localPosition),
            dFdy(localPosition)
        )
    );

    vec3 lightDirection =
        normalize(vec3(1.0, 1.0, 1.0));

    float light =
        0.45 +
        0.55 * abs(dot(normal, lightDirection));

    // Дополнительно немного различаем грани
    float faceShade =
        0.85 +
        0.15 * (
            abs(normal.x) +
            2.0 * abs(normal.y) +
            3.0 * abs(normal.z)
        ) / 6.0;

    outColor =
        vec4(
            vertexColor * light * faceShade,
            1.0
        );
}