#version 400

layout (location = 0) out vec4 gObjectID;

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

void main() {
    gObjectID = vec4(ObjectID/100.0, 0, 0, 1);
}