#version 450
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragUV;
layout(location = 2) in flat uint fragTexIndex;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D textures[];

void main() {
    vec4 texColor = texture(textures[nonuniformEXT(fragTexIndex)], fragUV);
    outColor = vec4(fragColor, 1.0) * texColor;
    
    // Simple alpha test for performance
    if (outColor.a < 0.1) {
        discard;
    }
}
