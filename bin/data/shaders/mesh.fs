#version 400
#define M_PI 3.1415926535897932384626433832795

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
uniform vec3 eyePosition;

out vec4 frag_color;

vec3 fresnel_schlick(vec3 spec_color, vec3 w_o, vec3 w_h) {
    return spec_color + (1.0 - spec_color) * pow(1.0 - clamp(dot(w_o, w_h), 0, 1), 5);
}

vec3 direct_specular(vec3 spec_color, float spec_power, float NdotL, vec3 w_o, vec3 w_h) {
    return  fresnel_schlick(spec_color, w_o, w_h) *
            ((spec_power + 2) / 8) * pow(clamp(dot(v_normal, w_h), 0, 1), spec_power) * NdotL;
}

vec3 direct_diffuse(vec3 diff_color, float NdotL) {
    // TODO : Add c_diff the diffuse color of material
    //return 1.0 / M_PI;

    return diff_color * NdotL;
}

void main() {
    vec3 light_color = vec3(1,1,1);
    vec3 specular_color = vec3(0.8, 0.8, 0.8);
    vec3 diffuse_color = vec3(0.1, 0.1, 0.1);
    float specular_power = 50.0;

    vec3 w_o = normalize(eyePosition - v_position);
    vec3 w_i = normalize(vec3(-3,5,2) - v_position);
    vec3 w_h = normalize(w_o + w_i);

    float NdotL = max(dot(w_i, v_normal), 0.0);

    vec3 light_contrib;
    if(NdotL > 0.0)
    {
        vec3 diffuse_refl = direct_diffuse(diffuse_color, NdotL);
        vec3 specular_refl = direct_specular(specular_color, specular_power, NdotL, w_o, w_h);

        light_contrib = light_color * (diffuse_refl + specular_refl);
    }

    // texturing
    vec4 tex_color = texture2D(tex0, v_texcoord);

    vec4 ambient = vec4(0.2, 0.2, 0.2, 1);


    frag_color = (ambient + vec4(light_contrib, 1)) *      // lighting
                 1 * //(0.3 + 0.7 * v_color) *    // color
                 tex_color;                 // texture
}
