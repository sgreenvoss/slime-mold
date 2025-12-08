#version 430 core
layout(local_size_x = 256) in;
// once per sliiime

struct Particle {
    ivec2 pos;
    float heading;
    int species_id;
};

struct ParticleSettings {
    int move_dist;
    int sensor_dist;
    float rotation_angle;
    float sensor_rotation;
    vec4 color;
    int pref;
};

layout(rgba8, binding = 0) uniform image2D TrailMap;
layout(std430, binding = 1) buffer Settings { ParticleSettings settings[]; };
layout(std430, binding = 2) buffer Particles { Particle particles[]; };
layout(rgba8, binding = 3) uniform image2D DiffuseMap;

uniform uint numSlimes;
uniform int numSpecies;
uniform float time;

// pseudorand from stack overflow------------------------------------------------
uint hash( uint x ) {
    x += ( x << 10u );
    x ^= ( x >>  6u );
    x += ( x <<  3u );
    x ^= ( x >> 11u );
    x += ( x << 15u );
    return x;
}

float scaleToRange01(uint state)
{
    return state / 4294967295.0;
}

uint hash( uvec3 v ) { return hash( v.x ^ hash(v.y) ^ hash(v.z)); }

float floatConstruct( uint m ) {
    const uint ieeeMantissa = 0x007FFFFFu; // binary32 mantissa bitmask
    const uint ieeeOne      = 0x3F800000u; // 1.0 in IEEE binary32

    m &= ieeeMantissa;                     // Keep only mantissa bits (fractional part)
    m |= ieeeOne;                          // Add fractional part to 1.0

    float  f = uintBitsToFloat( m );       // Range [1:2]
    return f - 1.0;                        // Range [0:1]
}

float random(vec3 v) {
    uvec3 bits = floatBitsToUint(v);
    return floatConstruct(hash(bits));
}

// end stack overflow section-----------------------------------------------------

int turn(float u, float r, float l, uint rand) {
    if (u > r && u > l) return 0; // no change

    if (r == l) {
        // turn randomly (based on coin flip
        if (random(vec3(float(gl_GlobalInvocationID.x), time, float(rand))) > 0.5)
            return 1;  // turn right (?)
        else return -1;
    }
    else if (l > r) {
        return -1;
    }
    else return 1;
}

float sense(Particle s, ParticleSettings set, int angleMult, ivec2 worldDim) {
    vec2 loc = vec2(s.pos.x + cos(s.heading + radians(set.sensor_rotation) * angleMult) * set.sensor_dist,
                    s.pos.y + sin(s.heading + radians(set.sensor_rotation) * angleMult) * set.sensor_dist);

    int x = max(0, min(int(round(loc.x)), worldDim.x - 1));
    int y = max(0, min(int(round(loc.y)), worldDim.y - 1));
    vec4 pixel = imageLoad(TrailMap, ivec2(x, y));
    return length(pixel.rgb);
}

void main() {
    uint idx = gl_GlobalInvocationID.x;
    ivec2 dims = imageSize(TrailMap);

    Particle slime = particles[idx];
    ParticleSettings set = settings[slime.species_id];

    uint random = hash(slime.pos.y * dims.x + slime.pos.x + hash(idx));
    float randomSteer = scaleToRange01(random);

    float up_trail = sense(slime, set, 0, dims);
    float left_trail = sense(slime, set, -1, dims);
    float right_trail = sense(slime, set, 1, dims);

    float new_heading  = slime.heading + float(turn(up_trail, left_trail, right_trail, random)) * set.rotation_angle;
    vec2 new_direction = vec2(cos(new_heading), sin(new_heading));

    vec2 newPos = vec2(slime.pos.x + new_direction.x * set.move_dist,
                         slime.pos.y + new_direction.y * set.move_dist);

    newPos = clamp(newPos, ivec2(0), dims - ivec2(1));
    ivec2 finPos = ivec2(round(newPos));
    particles[idx].heading = new_heading;

    //vec4 pixel = imageLoad(TrailMap, ivec2(finPos.x, finPos.y));
    //pixel = clamp(pixel + vec4(0.5), vec4(0), vec4(1.0));

    imageStore(TrailMap, finPos, vec4(1.0));
    particles[gl_GlobalInvocationID.x].pos = finPos;
}