#version 400
#extension GL_ARB_shading_language_include : require

#include "lighting.glsl"
// ######################################################
#define M_PI     3.14159265358
#define M_INV_PI 0.31830988618

#define IOR_IRON 2.9304
#define IOR_POLYSTYREN 1.5916

#define IOR_GOLD 0.27049
#define IOR_SILVER 0.15016
#define SCOLOR_GOLD      vec3(1.000000, 0.765557, 0.336057)
#define SCOLOR_SILVER    vec3(0.971519, 0.959915, 0.915324)
#define SCOLOR_ALUMINIUM vec3(0.913183, 0.921494, 0.924524)
#define SCOLOR_COPPER    vec3(0.955008, 0.637427, 0.538163)

float fresnel_F0(float n1, float n2) {
    float f0 = (n1 - n2)/(n1 + n2);
    return f0 * f0;
}

vec3 F_schlick(vec3 f0, float f90, float u) {
    return f0 + (f90 - f0) * pow(1.0 - u, 5.0);
}

float G_schlick_GGX(float k, float dotVal) {
    return dotVal / (dotVal * (1 - k) + k);
}

vec3 GGX(float NdotL, float NdotV, float NdotH, float LdotH, float roughness, vec3 F0) {
    float alpha2 = roughness * roughness;

    // F 
    vec3 F = F_schlick(F0, 1.0, LdotH);

    // D (Trowbridge-Reitz). Divide by PI at the end
    float denom = NdotH * NdotH * (alpha2 - 1.0) + 1.0;
    float D = alpha2 / (M_PI * denom * denom);

    // G (Smith GGX - Height-correlated)
    float lambda_GGXV = NdotL * sqrt((-NdotV * alpha2 + NdotV) * NdotV + alpha2);
    float lambda_GGXL = NdotV * sqrt((-NdotL * alpha2 + NdotL) * NdotL + alpha2);
    // float G = G_schlick_GGX(k, NdotL) * G_schlick_GGX(k, NdotV);

    // optimized G(NdotL) * G(NdotV) / (4 NdotV NdotL)
    // float G = 1.0 / (4.0 * (NdotL * (1 - k) + k) * (NdotV * (1 - k) + k));
    float G = 0.5 / (lambda_GGXL + lambda_GGXV);

    return D * F * G;
}

// Renormalized version of Burley's Disney Diffuse factor, used by Frostbite
float diffuse_Burley(float NdotL, float NdotV, float LdotH, float roughness) {
    float energyBias = mix(0.0, 0.5, roughness);
    float energyFactor = mix(1.0, 1.0 / 1.51, roughness);
    float fd90 = energyBias + 2.0 * LdotH * LdotH * roughness;
    vec3 f0 = vec3(1.0);
    float lightScatter = F_schlick(f0, fd90, NdotL).r;
    float viewScatter = F_schlick(f0, fd90, NdotV).r;

    return lightScatter * viewScatter * energyFactor * M_INV_PI;
}

float diffuse_Lambert(float NdotL) {
    return NdotL * M_INV_PI;
}

float smoothDistanceAttenuation(float sqrDist, float invSqrAttRadius) {
    float factor = sqrDist * invSqrAttRadius;
    float smoothFactor = clamp(1.0 - factor * factor, 0.0, 1.0);
    return smoothFactor * smoothFactor;
}

float getDistanceAttenuation(vec3 light_vec, float invSqrAttRadius) {
    float sqrDist = dot(light_vec, light_vec);
    float attenuation = 1.0 / (max(sqrDist, 0.0001));
    attenuation *= smoothDistanceAttenuation(sqrDist, invSqrAttRadius);

    return attenuation;
}
// ######################################################

in vec4 v_color;
in vec3 v_position;
in vec3 v_normal;
in vec2 v_texcoord;
in mat3 v_TBN;

struct Light {
    vec3 position;
    vec3 Ld;
    float radius;
};

layout (std140) uniform Lights {
    Light lights[8];
};

layout (std140) uniform Material {
    vec3 Ka;
    vec3 Kd;
    vec3 Ks;
    float shininess;
};

uniform int nLights;
uniform vec3 eyePosition;

// Texture buffers
uniform sampler2D DiffuseTex;   // index 0
uniform sampler2D SpecularTex;  // index 1
uniform sampler2D NormalTex;    // index 2

out vec4 frag_color;

vec4 depthBuffer() {
    float near = 1.0;
    float far = 100.0;
    float z = gl_FragCoord.z * 2.0 - 1.0;
    z = ((2.0 * far * near) / (far + near - z * (far - near))) / far;

    return vec4(vec3(z), 1.0);
}

void main() {
    // texturing
    vec3 diffuseTexColor = texture2D(DiffuseTex, v_texcoord).rgb;
    vec4 specularTexColor = texture2D(SpecularTex, v_texcoord);
    vec3 normalTexColor = texture2D(NormalTex, v_texcoord).rgb;

    // Normal vector from Normalmap
    // vec3 N = normalize(normalTexColor * 2.0 - 1.0);
    // N = normalize(v_TBN * N);
    vec3 objN = normalize(normalTexColor * 2.0 - 1.0);
    objN = normalize(v_TBN * objN);
    // objN += (v_TBN[1] + 1.0 * 0.5);

    // lighting
    vec3 light_contrib = vec3(0);

    float light_power = 200;

    vec3 diffuse_color = Kd;
    vec3 specular_color = Ks * specularTexColor.rgb;
    float roughness = 1.0 - (shininess * specularTexColor.r);

    vec3 N = objN;//normalize(v_normal);
    vec3 V = normalize(eyePosition - v_position);
    float NdotV = max(0, dot(N, V));

    for(int i = 0; i < nLights; ++i) {
        vec3 light_vec = lights[i].position - v_position;
        vec3 light_color = lights[i].Ld * light_power;
        float light_dist = length(light_vec);
        float light_radius = lights[i].radius;

        vec3 L = light_vec / light_dist;
        vec3 H = normalize(V + L); 
        float NdotL = max(dot(L, N), 0.0);
        if(NdotL > 0.0)
        {
            float NdotH = max(0, dot(N, H));
            float LdotH = max(0, dot(L, H));

            float att = 1;
            att *= getDistanceAttenuation(light_vec, 1.0/(light_radius*light_radius));

            vec3 Fd = diffuse_color * diffuse_Burley(NdotL, NdotV, LdotH, roughness);
            vec3 Fr = GGX(NdotL, NdotV, NdotH, LdotH, roughness, specular_color);
            // vec3 Fd = diffuse_color * diffuse_Lambert(NdotL);
            // vec3 R = 2.0 * NdotL * N - L;
            // vec3 Fr = F0 * NdotL * pow(max(0, dot(V, R)), (1.0/(roughness*roughness)));

            light_contrib += light_color * att * (Fd + Fr) * NdotL;
        }
    }


    vec3 finalcol = light_contrib + Ka;

    frag_color = vec4(finalcol, 1) *  // lighting
                 1 * //(0.3 + 0.7 * v_color) *  // color
                 vec4(diffuseTexColor, 1);                     // texture

    // To visualize depth :
    // frag_color = frag_color * 0.0001 + depthBuffer();
}
