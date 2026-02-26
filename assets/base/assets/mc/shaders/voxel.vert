#version 460

// -------------------------------------------------------------------
// GPU-Driven Voxel Vertex Shader
//
// Reads per-object transform from ObjectData SSBO via gl_BaseInstance.
// Unpacks materialID and normal index from the packed vertex attribute.
// -------------------------------------------------------------------

layout(location = 0) in vec3 inPosition;
layout(location = 1) in uint inPacked;   // bits [0..15] = materialID, [16..18] = normalIdx
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec2  fragUV;
layout(location = 1) out flat uint fragMaterialID;
layout(location = 2) out flat uint fragNormalIdx;

layout(set = 0, binding = 0) uniform GlobalUbo {
    mat4  projection;
    mat4  view;
    mat4  inverseView;
    float time;
    float _pad0;
    float _pad1;
    float _pad2;
} ubo;

struct ObjectData {
    mat4 model;
    vec4 aabbMin;
    vec4 aabbMax;
};

layout(std430, set = 0, binding = 1) readonly buffer ObjectDataBuffer {
    ObjectData objects[];
};

void main() {
    ObjectData obj = objects[gl_BaseInstance];

    uint materialID = inPacked & 0xFFFFu;
    uint normalIdx  = (inPacked >> 16u) & 0x7u;

    vec4 worldPos = obj.model * vec4(inPosition, 1.0);
    gl_Position = ubo.projection * ubo.view * worldPos;

    fragUV         = inUV;
    fragMaterialID = materialID;
    fragNormalIdx  = normalIdx;
}
