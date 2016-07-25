#version 400

layout (location = 0) out int gObjectID;

in vec4 v_color;
in vec3 v_position;
in vec3 v_normal;
in vec2 v_texcoord;

uniform int ObjectID;

void main() {
    gObjectID = ObjectID;
}