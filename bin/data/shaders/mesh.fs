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
    //vec3 L = normalize(vec3(-3,5,2) - position);
    //float lambert = max(dot(L, normal), 0.0);

    //vec3 ambient = vec3(0.2,0.2,0.2);
    //vec3 diffuse = lambert * vec3(0.6);

    // texturing
    vec4 tex_color = texture2D(tex0, v_texcoord);


    // result fragment
    frag_color = v_color * tex_color;
}
