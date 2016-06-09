#version 400

in vec3 in_position;
in vec3 in_normal;
in vec2 in_texcoord;
in vec4 in_color;

uniform mat4 ModelMatrix;
uniform mat4 ViewMatrix;
uniform mat4 ProjMatrix;

out vec4 v_color;
out vec3 v_position;
out vec3 v_normal;
out vec2 v_texcoord;

void main() {
    vec4 world_position = ModelMatrix * (vec4(in_position, 1));

    v_color = in_color;
    v_position = vec3(world_position);
    v_normal = normalize(vec3(ModelMatrix * vec4(in_normal, 0)));
    v_texcoord = in_texcoord;

    gl_Position = ProjMatrix * ViewMatrix * world_position;
}
