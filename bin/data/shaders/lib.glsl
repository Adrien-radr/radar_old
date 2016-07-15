#version 400

#define M_PI     3.14159265358
#define M_INV_PI 0.31830988618

// ######################################################
// Common structures

struct Ray {
    vec3 origin;
    vec3 dir;
};

struct Rect {
    vec3  center;
    vec3  dirx;
    vec3  diry;
    float halfx;
    float halfy;

    vec4  plane;
};


bool RayPlaneIntersect(Ray ray, vec4 plane, out float t) {
    t = -dot(plane, vec4(ray.origin, 1.0))/dot(plane.xyz, ray.dir);
    return t > 0.0;
}

bool RayRectIntersect(Ray ray, Rect rect, out float t) {
    bool intersect = RayPlaneIntersect(ray, rect.plane, t);
    if (intersect)
    {
        vec3 pos  = ray.origin + ray.dir*t;
        vec3 lpos = pos - rect.center;
        
        float x = dot(lpos, rect.dirx);
        float y = dot(lpos, rect.diry);    

        if (abs(x) > rect.halfx || abs(y) > rect.halfy)
            intersect = false;
    }

    return intersect;
}

// With added fix from sect. 3.7 of
// "Linear Efficient Antialiased Displacement and Reflectance Mapping: Supplemental Material"
vec3 FetchCorrectedNormal(sampler2D nrmTex, vec2 texcoord, mat3 TBN, vec3 V) {
    vec3 n = normalize(texture2D(nrmTex, texcoord).xyz * 2.0 - 1.0);
    n = normalize(TBN * n);

    if(dot(n, V) < 0.0)
        n  = normalize(n - 1.01 * V * dot(n, V));
    return n;
}



// ######################################################
// SHADING Diffuse - GGX

#define IOR_IRON 2.9304
#define IOR_POLYSTYREN 1.5916

#define IOR_GOLD 0.27049
#define IOR_SILVER 0.15016
#define SCOLOR_GOLD      vec3(1.000000, 0.765557, 0.336057)
#define SCOLOR_SILVER    vec3(0.971519, 0.959915, 0.915324)
#define SCOLOR_ALUMINIUM vec3(0.913183, 0.921494, 0.924524)
#define SCOLOR_COPPER    vec3(0.955008, 0.637427, 0.538163)

float fresnelF0(float n1, float n2) {
    float f0 = (n1 - n2)/(n1 + n2);
    return f0 * f0;
}

vec3 fresnelSchlick(vec3 f0, float f90, float u) {
    return f0 + (f90 - f0) * pow(1.0 - u, 5.0);
}

float G_schlick_GGX(float k, float dotVal) {
    return dotVal / (dotVal * (1 - k) + k);
}

vec3 GGX(float NdotL, float NdotV, float NdotH, float LdotH, float roughness, vec3 F0) {
    float alpha2 = roughness * roughness;

    // F 
    vec3 F = fresnelSchlick(F0, 1.0, LdotH);

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
float diffuseBurley(float NdotL, float NdotV, float LdotH, float roughness) {
    float energyBias = mix(0.0, 0.5, roughness);
    float energyFactor = mix(1.0, 1.0 / 1.51, roughness);
    float fd90 = energyBias + 2.0 * LdotH * LdotH * roughness;
    vec3 f0 = vec3(1.0);
    float lightScatter = fresnelSchlick(f0, fd90, NdotL).r;
    float viewScatter = fresnelSchlick(f0, fd90, NdotV).r;

    return lightScatter * viewScatter * energyFactor * M_INV_PI;
}

float diffuseLambert(float NdotL) {
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
    //attenuation *= smoothDistanceAttenuation(sqrDist, invSqrAttRadius);

    return attenuation;
}

// ######################################################
// LTC
void ClipQuadToHorizon(inout vec3 L[5], out int n)
{
    // detect clipping config
    int config = 0;
    if (L[0].z > 0.0) config += 1;
    if (L[1].z > 0.0) config += 2;
    if (L[2].z > 0.0) config += 4;
    if (L[3].z > 0.0) config += 8;

    // clip
    n = 0;

    if (config == 0)
    {
        // clip all
    }
    else if (config == 1) // V1 clip V2 V3 V4
    {
        n = 3;
        L[1] = -L[1].z * L[0] + L[0].z * L[1];
        L[2] = -L[3].z * L[0] + L[0].z * L[3];
    }
    else if (config == 2) // V2 clip V1 V3 V4
    {
        n = 3;
        L[0] = -L[0].z * L[1] + L[1].z * L[0];
        L[2] = -L[2].z * L[1] + L[1].z * L[2];
    }
    else if (config == 3) // V1 V2 clip V3 V4
    {
        n = 4;
        L[2] = -L[2].z * L[1] + L[1].z * L[2];
        L[3] = -L[3].z * L[0] + L[0].z * L[3];
    }
    else if (config == 4) // V3 clip V1 V2 V4
    {
        n = 3;
        L[0] = -L[3].z * L[2] + L[2].z * L[3];
        L[1] = -L[1].z * L[2] + L[2].z * L[1];
    }
    else if (config == 5) // V1 V3 clip V2 V4) impossible
    {
        n = 0;
    }
    else if (config == 6) // V2 V3 clip V1 V4
    {
        n = 4;
        L[0] = -L[0].z * L[1] + L[1].z * L[0];
        L[3] = -L[3].z * L[2] + L[2].z * L[3];
    }
    else if (config == 7) // V1 V2 V3 clip V4
    {
        n = 5;
        L[4] = -L[3].z * L[0] + L[0].z * L[3];
        L[3] = -L[3].z * L[2] + L[2].z * L[3];
    }
    else if (config == 8) // V4 clip V1 V2 V3
    {
        n = 3;
        L[0] = -L[0].z * L[3] + L[3].z * L[0];
        L[1] = -L[2].z * L[3] + L[3].z * L[2];
        L[2] =  L[3];
    }
    else if (config == 9) // V1 V4 clip V2 V3
    {
        n = 4;
        L[1] = -L[1].z * L[0] + L[0].z * L[1];
        L[2] = -L[2].z * L[3] + L[3].z * L[2];
    }
    else if (config == 10) // V2 V4 clip V1 V3) impossible
    {
        n = 0;
    }
    else if (config == 11) // V1 V2 V4 clip V3
    {
        n = 5;
        L[4] = L[3];
        L[3] = -L[2].z * L[3] + L[3].z * L[2];
        L[2] = -L[2].z * L[1] + L[1].z * L[2];
    }
    else if (config == 12) // V3 V4 clip V1 V2
    {
        n = 4;
        L[1] = -L[1].z * L[2] + L[2].z * L[1];
        L[0] = -L[0].z * L[3] + L[3].z * L[0];
    }
    else if (config == 13) // V1 V3 V4 clip V2
    {
        n = 5;
        L[4] = L[3];
        L[3] = L[2];
        L[2] = -L[1].z * L[2] + L[2].z * L[1];
        L[1] = -L[1].z * L[0] + L[0].z * L[1];
    }
    else if (config == 14) // V2 V3 V4 clip V1
    {
        n = 5;
        L[4] = -L[0].z * L[3] + L[3].z * L[0];
        L[0] = -L[0].z * L[1] + L[1].z * L[0];
    }
    else if (config == 15) // V1 V2 V3 V4
    {
        n = 4;
    }
    
    if (n == 3)
        L[3] = L[0];
    if (n == 4)
        L[4] = L[0];
}

float IntegrateEdge(vec3 P1, vec3 P2) {
    float cosTheta = dot(P1, P2);
    cosTheta = clamp(cosTheta, -0.9999, 0.9999);

    float theta = acos(cosTheta);
    float r = cross(P1, P2).z * theta / sin(theta);
    return r;
}

// Returns a vec2 to index the BRDF matrix depending on the view angle and surface roughness
vec2 LTCCoords(float NdotV, float roughness) {
    float theta = acos(NdotV);
    vec2 coords = vec2(roughness, theta/(0.5*M_PI));

    // corrected lookup (x - 1 + 0.5)
    const float LUT_SIZE = 32.0;
    coords = coords * (LUT_SIZE - 1.0) / LUT_SIZE + 0.5 / LUT_SIZE;

    return coords;
}

mat3 LTCMatrix(sampler2D ltcMatrix, vec2 coord) {
    vec4 v = texture2D(ltcMatrix, coord);
    v = v.rgba;
    // bgra
    // rgba

    // inverse of 
    // a 0 b
    // 0 c 0
    // d 0 1
    mat3 Minv = mat3(
       vec3(1,   0, v.g),       // 1st column
        vec3(0,  v.b,   0),
        vec3(v.a,  0,v.r)
    );
    return Minv;
}

vec3 LTCEvaluate(vec3 N, vec3 V, vec3 P, mat3 Minv, vec3 points[4], bool twoSided) {
    // N orthonormal frame
    vec3 T, B;
    T = normalize(V - N * dot(V, N));
    B = cross(N, T);

    Minv = Minv * transpose(mat3(T, B, N));

    // polygon in TBN frame 
    vec3 L[5];
    L[0] = Minv * (points[0] - P);
    L[1] = Minv * (points[1] - P);
    L[2] = Minv * (points[2] - P);
    L[3] = Minv * (points[3] - P);
    L[4] = L[3];

    int n;
    ClipQuadToHorizon(L, n);

    if(n == 0)
        return vec3(0, 0, 0);

    // polygon projected onto sphere
    L[0] = normalize(L[0]);
    L[1] = normalize(L[1]);
    L[2] = normalize(L[2]);
    L[3] = normalize(L[3]);
    L[4] = normalize(L[4]);

    // integration
    float sum = 0.0;

    sum += IntegrateEdge(L[0], L[1]);
    sum += IntegrateEdge(L[1], L[2]);
    sum += IntegrateEdge(L[2], L[3]);
    if(n >= 4)
        sum += IntegrateEdge(L[3], L[4]);
    if(n == 5)
        sum += IntegrateEdge(L[4], L[0]);

    sum = twoSided ? abs(sum) : max(0.0, -sum);

    return vec3(sum);
} 