#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inUV;
layout(location = 3) in uint inTexIndex;
layout(location = 4) in uint inAnimFrames;

layout(location = 0) out vec3  fragColor;
layout(location = 1) out vec2  fragUV;
layout(location = 2) out flat uint fragTexIndex;
layout(location = 3) out flat uint fragAnimFrames;

layout(set = 0, binding = 0) uniform GlobalUbo {
    mat4  projection;
    mat4  view;
    mat4  inverseView;
    float time;
    float _pad0;
    float _pad1;
    float _pad2;
} ubo;

layout(push_constant) uniform Push {
    mat4 modelMatrix;
} push;

void main() {
    gl_Position    = ubo.projection * ubo.view * push.modelMatrix * vec4(inPosition, 1.0);
    fragColor      = inColor;
    fragUV         = inUV;
    fragTexIndex   = inTexIndex;
    fragAnimFrames = inAnimFrames;
}
