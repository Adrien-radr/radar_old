#version 400

in vec4 v_color;
in vec3 v_position;
in vec3 v_normal;
in vec2 v_texcoord;

struct Light {
    vec3 position;
    vec4 ambient;
    vec4 diffuse;
    float radius;
};

uniform Light lights[10];
uniform sampler2D tex0;

out vec4 frag_color;

void main() {
    // lighting
    vec3 L = normalize(vec3(-3,5,2) - v_position);
    float lambert = max(dot(L, v_normal), 0.0);

    //vec3 ambient = vec3(0.2,0.2,0.2);
    //vec3 diffuse = lambert;

    // texturing
    vec4 tex_color = texture2D(tex0, v_texcoord);

    // result fragment
    vec4 diffuse = vec4(lambert, lambert, lambert, 1);
    frag_color =  (0.3 + 0.7 * v_color) * diffuse * tex_color;
}
