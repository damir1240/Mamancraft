#version 460
#extension GL_EXT_nonuniform_qualifier : enable

// -------------------------------------------------------------------
// GPU-Driven Voxel Fragment Shader
//
// Reads MaterialData from SSBO using materialID passed from vertex shader.
// Supports animated textures, alpha testing, and transparency.
// -------------------------------------------------------------------

layout(location = 0) in vec2      fragUV;
layout(location = 1) in flat uint fragMaterialID;
layout(location = 2) in flat uint fragNormalIdx;

layout(location = 0) out vec4 outColor;

struct MaterialData {
    vec4  albedoTint;       // RGBA tint
    uint  albedoTexIndex;   // bindless texture index
    uint  animFrames;       // 1 = static, N > 1 = animated
    float animFPS;          // animation speed
    uint  flags;            // bit 0 = transparent
};

layout(set = 0, binding = 0) uniform GlobalUbo {
    mat4  projection;
    mat4  view;
    mat4  inverseView;
    float time;
    float _pad0;
    float _pad1;
    float _pad2;
} ubo;

layout(std430, set = 0, binding = 2) readonly buffer MaterialBuffer {
    MaterialData materials[];
};

layout(set = 1, binding = 0) uniform sampler2D textures[];

void main() {
    MaterialData mat = materials[fragMaterialID];
    vec2 uv = fragUV;

    // ── Animated texture (vertical strip) ──────────────────────────
    if (mat.animFrames > 1u) {
        float N         = float(mat.animFrames);
        float frameSize = 1.0 / N;
        float frame     = mod(floor(ubo.time * mat.animFPS), N);
        float localV    = fract(uv.y) * frameSize;
        uv.y            = frame * frameSize + localV;
        uv.x            = fract(uv.x);
    }

    vec4 texColor = texture(textures[nonuniformEXT(mat.albedoTexIndex)], uv);
    outColor = mat.albedoTint * texColor;

    // Alpha test: discard nearly-invisible fragments
    if (outColor.a < 0.05) {
        discard;
    }

    // ── Transparency ───────────────────────────────────────────────
    bool isTransparent = (mat.flags & 0x1u) != 0u;
    if (isTransparent) {
        outColor.a *= 0.78;
    }
}
