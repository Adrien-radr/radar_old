#version 400

in vec2 in_position;
//in vec4 in_color;
in vec2 in_texcoord;

//uniform vec2 Position;
uniform mat4 ProjMatrix;
uniform mat4 ModelMatrix;
//uniform int  Depth;
//uniform vec2 Scale;

out vec2 texcoord;

void main() {
    texcoord = in_texcoord;

    vec4 position = ProjMatrix * ModelMatrix * vec4(in_position, 1, 1);
    //position.z = 0.f;

    gl_Position = position;
}
