#version 400

in vec3 v_texcoord;

uniform samplerCube skybox;

out vec4 frag_color;

void main() {
    // vec3 reversecoord = vec3(v_texcoord.x, -v_texcoord.y, v_texcoord.z);
    frag_color = vec4(1.5,1.5,1.5,1) * texture(skybox, -v_texcoord);
}