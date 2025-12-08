#version 430 core
layout (local_size_x = 1, local_size_y = 1) in;

layout (rgba8, binding = 0) uniform image2D TrailMap;
layout (rgba8, binding = 3) uniform image2D DiffuseMap;
uniform float decay;

void Diffuse(ivec2 pos) {
    vec4 sum = vec4(0);
    vec4 first = imageLoad(TrailMap, pos);
    ivec2 dims = imageSize(TrailMap);


    for (int offsetX = -1; offsetX <= 1; offsetX++) {
        for (int offsetY = -1; offsetY <= 1; offsetY++) {
            int sampleX = min(dims.x-1, max(0, pos.x + offsetX));
            int sampleY = min(dims.y-1, max(0, pos.y + offsetY));
            sum += imageLoad(TrailMap, ivec2(sampleX, sampleY));
        }
    }
    vec4 blurCol = vec4((sum / 9) * decay);
    imageStore(DiffuseMap, pos, max(vec4(0), min(blurCol, vec4(1.0,1.0,1.0,1.0))));
}

void main() {
    ivec2 idx = ivec2(gl_GlobalInvocationID.xy);

    Diffuse(idx);
    memoryBarrierImage();
}