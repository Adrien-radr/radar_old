#version 400

//in vec4 color;
in vec2 texcoord;

uniform sampler2D   tex0;
uniform vec4        text_color;

out vec4 frag_color;

void main() {
    vec4 tex_color = texture2D(tex0, texcoord);
    frag_color = text_color * vec4(1,1,1,tex_color.r);
}
