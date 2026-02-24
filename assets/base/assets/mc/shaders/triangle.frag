#version 450
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec3      fragColor;
layout(location = 1) in vec2      fragUV;
layout(location = 2) in flat uint fragTexIndex;
layout(location = 3) in flat uint fragAnimFrames;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform GlobalUbo {
    mat4  projection;
    mat4  view;
    mat4  inverseView;
    float time;
    float _pad0;
    float _pad1;
    float _pad2;
} ubo;

layout(set = 1, binding = 0) uniform sampler2D textures[];

void main() {
    vec2 uv = fragUV;

    // ── Animated texture (water strip) ─────────────────────────────────────
    // The texture is a VERTICAL strip of N frames, each 16×16 px.
    // Total image size: 16×(16*N).
    // UV.y must be remapped to select only one frame at a time.
    //
    // Formula:
    //   frame      = floor(time * FPS) mod N          ← current frame index
    //   frameSize  = 1.0 / N                          ← fraction of texture per frame
    //   uv.y_raw   = fract(uv.y)                      ← local 0..1 within the quad
    //   uv.y_final = frame * frameSize + uv.y_raw * frameSize
    //
    if (fragAnimFrames > 1u) {
        const float FPS       = 8.0;        // animation speed (frames per second)
        float N               = float(fragAnimFrames);
        float frameSize       = 1.0 / N;
        float frame           = mod(floor(ubo.time * FPS), N);
        float localV          = fract(uv.y) * frameSize;
        uv.y                  = frame * frameSize + localV;

        // Keep U in [0,1]: water texture is only 16px wide, no tiling needed
        uv.x = fract(uv.x);
    }

    vec4 texColor = texture(textures[nonuniformEXT(fragTexIndex)], uv);
    outColor = vec4(fragColor, 1.0) * texColor;

    // Alpha test: discard nearly-invisible fragments
    if (outColor.a < 0.05) {
        discard;
    }

    // ── Water transparency ─────────────────────────────────────────────────
    // Apply semi-transparency for water blocks (fragAnimFrames == water marker).
    // We render water faces at ~75% alpha for a nice translucent look.
    if (fragAnimFrames > 1u) {
        outColor.a = outColor.a * 0.78;
    }
}
