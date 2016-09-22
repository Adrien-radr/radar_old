#version 400

layout (location = 0) out vec4 gObjectID;
layout (location = 1) out vec4 gDepth;
layout (location = 2) out vec4 gNormal;
layout (location = 3) out vec4 gWorldPos;

in vec4 v_color;
in vec3 v_position;
in vec3 v_normal;
in vec2 v_texcoord;

uniform int ObjectID;

vec4 depthBuffer() {
    float near = 1.0;
    float far = 100.0;
    float z = gl_FragCoord.z * 2.0 - 1.0;
    z = ((2.0 * far * near) / (far + near - z * (far - near))) / far;

    return vec4(vec3(z), 1.0);
}

float rand(in vec2 co){
    return 0.3 + 0.7 * fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

void main() {
    //vec2 randSeed = vec2(24.5232, 13.53242);    // fixed seed for the 'random' generator 
    //vec3 objColor = vec3(rand(randSeed * (ObjectID + 1) * 1),
                         //rand(randSeed * (ObjectID + 1) * 2),
                         //rand(randSeed * (ObjectID + 1) * 3)); // object-dependant color

	vec4 depth = depthBuffer();
    gObjectID = vec4(float(ObjectID), float(gl_PrimitiveID + 1), depth.x, 1);
    gDepth = depth;
    gNormal = vec4(v_normal, 1);
    gWorldPos = vec4(v_position, 1);
}