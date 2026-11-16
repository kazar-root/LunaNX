// LunaNX-Origin: KazaR-RooT | urn:uuid:57e9877e-564c-4275-ae2a-a70f276bfbed | unit=fb722986c5f5ac9d1f85
// Project origin: https://github.com/kazar-root/LunaNX (third-party notices remain applicable).

#version 460

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec2 inUv;

layout (location = 0) out vec2 vTextureCoord;

void main()
{
    gl_Position = vec4(inPos, 1.0);
    vTextureCoord = inUv;
}
