#version 400
#define M_PI     3.14159265358
#define M_INV_PI 0.31830988618
#define IOR_IRON 2.9304
#define IOR_GOLD 0.27049
#define IOR_SILVER 0.15016
#define IOR_POLYSTYREN 1.5916
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

//vec3 fresnel_schlick(vec3 spec_color, vec3 w_o, vec3 w_h) {
//    return spec_color + (1.0 - spec_color) * pow(1.0 - clamp(dot(w_o, w_h), 0, 1), 5);
//}



float fresnel_F0(float n1, float n2) {
    float f0 = (n1 - n2)/(n1 + n2);
    return f0 * f0;
}

float fresnel_schlick(float dotVal, float F0) {
    return F0 + (1.0 - F0) * pow(1.0 - dotVal, 5);
}

float fresnel_diffuse(float dotVal, float FD90) {
    return 1.0 + (FD90 - 1.0) * pow(1.0 - dotVal, 5);
}

float G1V(float dotVal, float k) {
    return 1.0 / (dotVal * (1 - k) + k);
}

float GGX(float NdotL, float NdotV, float NdotH, float LdotH, float roughness, float F0) {
    float alpha = roughness * roughness;
    // F
    float F = fresnel_schlick(LdotH, F0);

    // D
    float alphaSqr = alpha * alpha;
    float denom = NdotH * NdotH * (alphaSqr-1.0) + 1.0;
    float D = alphaSqr / (M_PI * denom * denom);

    // G
    float k = alpha / 2.0;
    float G = G1V(NdotL, k) * G1V(NdotV, k);

    return NdotL * D * F * G;
}

float direct_diffuse(float NdotL, float NdotV, float LdotH, float roughness) {
    float FD90 = 0.5 + 2 * pow(LdotH, 2) * roughness;
    float F = fresnel_diffuse(NdotL, FD90) * fresnel_diffuse(NdotV, FD90);

    return F * M_INV_PI;
}

void main() {
    vec3 light_color = vec3(1,1,1);
    vec3 specular_color = vec3(0.8, 0.8, 0.8);
    vec3 diffuse_color = vec3(0.5, 0.5, 0.5);
    float specular_power = 2.0;
    float roughness = 1.0 / specular_power;
    float F0 = fresnel_F0(1.0, IOR_SILVER);

    vec3 N = normalize(v_normal);
    vec3 V = normalize(eyePosition - v_position);
    vec3 L = normalize(vec3(-3,5,2) - v_position);
    vec3 H = normalize(V + L);

    float NdotL = max(dot(L, N), 0.0);

    vec3 light_contrib;
    if(NdotL > 0.0)
    {
        float NdotV = max(0, dot(N, V));
        float NdotH = max(0, dot(N, H));
        float LdotH = max(0, dot(L, H));

        vec3 diffuse_refl = diffuse_color * direct_diffuse(NdotL, NdotV, LdotH, roughness);
        vec3 specular_refl = specular_color * GGX(NdotL, NdotV, NdotH, LdotH, roughness, F0);

        light_contrib = light_color * (diffuse_refl + specular_refl);
    }

    // texturing
    vec4 tex_color = texture2D(tex0, v_texcoord);

    vec4 ambient = vec4(0.2, 0.2, 0.2, 1);


    frag_color = (ambient + vec4(light_contrib, 1)) *      // lighting
                 1 * //(0.3 + 0.7 * v_color) *    // color
                 tex_color;                 // texture
}
