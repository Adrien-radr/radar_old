#version 400

#define M_PI     3.14159265358
#define M_INV_PI 0.31830988618

struct PointLight {
    vec3  position;
    vec3  Ld;
    float radius;
};

struct AreaLight {
    vec3  position;
    vec3  dirx;
    float hwidthx;
    vec3  diry;
    float hwidthy;
    vec3  Ld;
    vec4  plane;
};
void InitRectPoints(AreaLight rect, out vec3 points[4]) {
    vec3 ex = rect.hwidthx * rect.dirx;
    vec3 ey = rect.hwidthy * rect.diry;

    points[0] = rect.position - ex - ey;
    points[1] = rect.position + ex - ey;
    points[2] = rect.position + ex + ey;
    points[3] = rect.position - ex + ey;
}

// FWD Decl of functions in lib.glsl
vec3 GGX(float NdotL, float NdotV, float NdotH, float LdotH, float roughness, vec3 F0);
float diffuseBurley(float NdotL, float NdotV, float LdotH, float roughness);
float getDistanceAttenuation(vec3 light_vec, float invSqrAttRadius);
vec3 LTCEvaluate(vec3 N, vec3 V, vec3 P, mat3 Minv, vec3 points[4], bool twoSided);
vec3 FetchCorrectedNormal(sampler2D nrmTex, vec2 texcoord, mat3 TBN, vec3 V);

// INPUTS
in vec4 v_color;
in vec3 v_position;
in vec3 v_normal;
in vec2 v_texcoord;
in mat3 v_TBN;
in vec3 v_viewPosition;
in vec3 v_viewNormal;

layout (std140) uniform PointLights {
    PointLight plights[8];
};

layout (std140) uniform AreaLights {
    AreaLight alights[8];
};

layout (std140) uniform Material {
    vec3 Ka;
    vec3 Kd;
    vec3 Ks;
    float shininess;
};

uniform int nPointLights;
uniform int nAreaLights;
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

vec3 pointLightContribution(vec3 N, vec3 V, float NdotV, vec3 Kd, vec3 Ks, float roughness) {
    vec3 contrib = vec3(0);

    float light_power = 200;

    for(int i = 0; i < nPointLights; ++i) {
        vec3 light_vec = plights[i].position - v_position;
        vec3 light_color = plights[i].Ld * light_power;
        float light_dist = length(light_vec);
        float light_radius = plights[i].radius;

        vec3 L = light_vec / light_dist;
        vec3 H = normalize(V + L); 
        float NdotL = max(dot(L, N), 0.0);
        if(NdotL > 0.0)
        {
            float NdotH = max(0, dot(N, H));
            float LdotH = max(0, dot(L, H));

            float att = 1;
            att *= getDistanceAttenuation(light_vec, 1.0/(light_radius*light_radius));

            vec3 Fd = Kd * diffuseBurley(NdotL, NdotV, LdotH, roughness);
            vec3 Fr = GGX(NdotL, NdotV, NdotH, LdotH, roughness, Ks);
            // vec3 Fd = diffuse_color * diffuse_Lambert(NdotL);
            // vec3 R = 2.0 * NdotL * N - L;
            // vec3 Fr = F0 * NdotL * pow(max(0, dot(V, R)), (1.0/(roughness*roughness)));

            contrib += light_color * att * (Fd + Fr) * NdotL;
        }
    }

    return contrib;
}

vec3 areaLightContribution(vec3 N, vec3 V, float NdotV, vec3 Kd, vec3 Ks, float roughness) {
    vec3 contrib = vec3(0);
    vec3 points[4];

    vec3 viewV = normalize(-v_viewPosition);

    for(int i = 0; i < nAreaLights; ++i) {
        vec3 light_vec = alights[i].position - v_position;
        float light_dist = length(light_vec);
        vec3 L = light_vec / light_dist;
        vec3 pN = alights[i].plane.xyz;

        InitRectPoints(alights[i], points);

        // diffuse
        mat3 Minv = mat3(1);
        contrib += 1 * alights[i].Ld * LTCEvaluate(N, V, v_position, Minv, points, false);

        // specular
        // Minv = LTC_Matrix()
    }

    contrib /= 2.0 * M_PI;

    return contrib;
}

void main() {
    // texturing
    vec3 diffuseTexColor = texture2D(DiffuseTex, v_texcoord).rgb;
    vec4 specularTexColor = texture2D(SpecularTex, v_texcoord);
    vec3 normalTexColor = texture2D(NormalTex, v_texcoord).rgb;

    // lighting
    vec3 Li = vec3(0);

    vec3 diffuse_color = Kd;
    vec3 specular_color = Ks * specularTexColor.rgb;
    float roughness = 1.0 - (shininess * specularTexColor.r);

    // Normal vector from Normalmap
    // vec3 objN = normalize(normalTexColor * 2.0 - 1.0);
    // objN = normalize(v_TBN * objN);

    vec3 V = normalize(eyePosition - v_position);
    vec3 N = FetchCorrectedNormal(NormalTex, v_texcoord, v_TBN, V);
    float NdotV = max(0, dot(N, V));

    Li += pointLightContribution(N, V, NdotV, diffuse_color, specular_color, roughness);
    Li += areaLightContribution(N, V, NdotV, diffuse_color, specular_color, roughness);

    vec3 finalcol = Ka + Li;

    frag_color = vec4(finalcol, 1) *  // lighting
                 1 * //(0.3 + 0.7 * v_color) *  // color
                 vec4(diffuseTexColor, 1);                     // texture

    // To visualize depth :
    // frag_color = frag_color * 0.0001 + depthBuffer();
}
