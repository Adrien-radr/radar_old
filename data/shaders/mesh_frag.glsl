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
void InitRectPoints(in AreaLight rect, out vec3 points[4]) {
    vec3 ex = rect.hwidthx * rect.dirx;
    vec3 ey = rect.hwidthy * rect.diry;

    points[0] = rect.position - ex - ey;
    points[1] = rect.position + ex - ey;
    points[2] = rect.position + ex + ey;
    points[3] = rect.position - ex + ey;
}

bool CullAreaLight(in AreaLight l, in vec3 points[4], in vec3 P, in vec3 N, in float w) {
	bool P_rightSide = dot(l.plane.xyz, P) + l.plane.w > 1e-5;

	bool A_rightSide =
		(dot(N, points[0]) + w > 1e-5) ||
		(dot(N, points[1]) + w > 1e-5) ||
		(dot(N, points[2]) + w > 1e-5) ||
		(dot(N, points[3]) + w > 1e-5);

	return !(P_rightSide && A_rightSide);
}

// FWD Decl of functions in lib.glsl
vec3 GGX(in float NdotL, in float NdotV, in float NdotH, in float LdotH, in float roughness, in vec3 F0);
float diffuseBurley(in float NdotL, in float NdotV, in float LdotH, in float roughness);
float getDistanceAttenuation(in vec3 light_vec, in float invSqrAttRadius);
vec3 LTCEvaluate(in vec3 N, in vec3 V, in vec3 P, in mat3 Minv, in vec3 points[4], in bool twoSided);
vec3 FetchCorrectedNormal(in sampler2D nrmTex, in vec2 texcoord, in mat3 TBN, in vec3 V);
vec2 LTCCoords(in float NdotV, in float roughness);
mat3 LTCMatrix(in sampler2D ltcMatrix, in vec2 coord);
float rand(in vec2 seed);
vec3 rand3(in vec3 seed);

// INPUTS
in vec4 v_color;
in vec3 v_position;
in vec3 v_normal;
in vec2 v_texcoord;
in mat3 v_TBN;

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
uniform int GTMode;
uniform float globalTime;

// Texture buffers
uniform sampler2D DiffuseTex;   // index 0
uniform sampler2D SpecularTex;  // index 1
uniform sampler2D NormalTex;    // index 2
uniform sampler2D AOTex;        // index 3
uniform sampler2D ltc_mat;
uniform sampler2D ltc_amp;

out vec4 frag_color;

vec4 depthBuffer() {
    float near = 1.0;
    float far = 100.0;
    float z = gl_FragCoord.z * 2.0 - 1.0;
    z = ((2.0 * far * near) / (far + near - z * (far - near))) / far;

    return vec4(vec3(z), 1.0);
}

struct SphericalQuad
{
    vec3 o, x, y, z;
    float z0, z0sq;
    float x0, y0, y0sq;
    float x1, y1, y1sq;
    float b0, b1, b0sq, k;
    float S;
};

SphericalQuad SphericalQuadInit(in vec3 s, in vec3 ex, in vec3 ey, in vec3 o)
{
    SphericalQuad squad;

    squad.o = o;
    float exl = length(ex);
    float eyl = length(ey);

    // compute local reference system ’R’
    squad.x = ex / exl;
    squad.y = ey / eyl;
    squad.z = cross(squad.x, squad.y);

    // compute rectangle coords in local reference system
    vec3 d = s - o;
    squad.z0 = dot(d, squad.z);

    // flip ’z’ to make it point against ’Q’
    if (squad.z0 > 0.0)
    {
        squad.z  *= -1.0;
        squad.z0 *= -1.0;
    }

    squad.z0sq = squad.z0 * squad.z0;
    squad.x0 = dot(d, squad.x);
    squad.y0 = dot(d, squad.y);
    squad.x1 = squad.x0 + exl;
    squad.y1 = squad.y0 + eyl;
    squad.y0sq = squad.y0 * squad.y0;
    squad.y1sq = squad.y1 * squad.y1;

    // create vectors to four vertices
    vec3 v00 = vec3(squad.x0, squad.y0, squad.z0);
    vec3 v01 = vec3(squad.x0, squad.y1, squad.z0);
    vec3 v10 = vec3(squad.x1, squad.y0, squad.z0);
    vec3 v11 = vec3(squad.x1, squad.y1, squad.z0);

    // compute normals to edges
    vec3 n0 = normalize(cross(v00, v10));
    vec3 n1 = normalize(cross(v10, v11));
    vec3 n2 = normalize(cross(v11, v01));
    vec3 n3 = normalize(cross(v01, v00));

    // compute internal angles (gamma_i)
    float g0 = acos(-dot(n0, n1));
    float g1 = acos(-dot(n1, n2));
    float g2 = acos(-dot(n2, n3));
    float g3 = acos(-dot(n3, n0));

    // compute predefined constants
    squad.b0 = n0.z;
    squad.b1 = n2.z;
    squad.b0sq = squad.b0 * squad.b0;
    squad.k = 2.0*M_PI - g2 - g3;

    // compute solid angle from internal angles
    squad.S = g0 + g1 - squad.k;

    return squad;
}

vec3 SphericalQuadSample(in SphericalQuad squad, in float u, in float v) {
    // 1. compute 'cu'
    float au = u * squad.S + squad.k;
    float fu = (cos(au) * squad.b0 - squad.b1) / sin(au);
    float cu = 1.0 / sqrt(fu*fu + squad.b0sq) * (fu > 0.0 ? 1.0 : -1.0);
    cu = clamp(cu, -1.0, 1.0); // avoid NaNs

    // 2. compute 'xu'
    float xu = -(cu * squad.z0) / sqrt(1.0 - cu * cu);
    xu = clamp(xu, squad.x0, squad.x1); // avoid Infs

    // 3. compute 'yv'
    float d = sqrt(xu * xu + squad.z0sq);
    float h0 = squad.y0 / sqrt(d*d + squad.y0sq);
    float h1 = squad.y1 / sqrt(d*d + squad.y1sq);
    float hv = h0 + v * (h1 - h0), hv2 = hv * hv;
    float yv = (hv2 < 1.0 - 1e-6) ? (hv * d) / sqrt(1.0 - hv2) : squad.y1;

    // 4. transform (xu, yv, z0) to world coords
    return squad.o + xu*squad.x + yv*squad.y + squad.z0*squad.z;

}

mat3 BasisFrisvad(in vec3 v)
{
    vec3 x, y;

    if (v.z < -0.999999)
    {
        x = vec3( 0, -1, 0);
        y = vec3(-1,  0, 0);
    }
    else
    {
        float a = 1.0 / (1.0 + v.z);
        float b = -v.x*v.y*a;
        x = vec3(1.0 - v.x*v.x*a, b, -v.x);
        y = vec3(b, 1.0 - v.y*v.y*a, -v.y);
    }

    return mat3(x, y, v);
}

vec3 pointLightContribution(in vec3 N, in vec3 V, in float NdotV, in vec3 Kd, in vec3 Ks, in float roughness) {
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
            // vec3 FrPhong = Ks * pow(max(0, dot(V, R)), (1.0/(roughness*roughness)));

            contrib += light_color * att * (Fd + Fr) * NdotL;
        }
    }

    return contrib;
}

vec3 areaLightContribution(in vec3 N, in vec3 V, in float NdotV, in vec3 diff_color, in vec3 spec_color, in float roughness) {
    vec3 contrib = vec3(0);
    vec3 points[4];

    vec2 ltcCoords = LTCCoords(NdotV, roughness);
    mat3 MinvSpec = LTCMatrix(ltc_mat, ltcCoords);
    mat3 MinvDiff = mat3(1);
    vec2 schlick = texture2D(ltc_amp, ltcCoords).xy;

    for(int i = 0; i < nAreaLights; ++i) {
        vec3 light_vec = alights[i].position - v_position;
        float light_dist = length(light_vec);
        vec3 L = light_vec / light_dist;
        vec3 pN = alights[i].plane.xyz;
		
        InitRectPoints(alights[i], points);

        if(!CullAreaLight(alights[i], points, v_position, N, -dot(v_position, N))) {

            // diffuse
            vec3 diffuse = LTCEvaluate(N, V, v_position, MinvDiff, points, false);
            diffuse *= diff_color;

            // specular
            vec3 specular =  LTCEvaluate(N, V, v_position, MinvSpec, points, false);
            specular *= spec_color * schlick.x + (1.0 - spec_color) * schlick.y;

            contrib += alights[i].Ld * (diffuse + specular);
        }
    }

    contrib /= 2.0 * M_PI;

    return contrib;
}

vec3 areaLightGT(in vec3 N, in vec3 V, in float NdotV, in vec3 diff_color, in vec3 spec_color, in float roughness) {
    vec3 contrib = vec3(0);
    vec3 points[4];
    const int nSamples = 30;

    mat3 t2w = BasisFrisvad(N);
    mat3 w2t = transpose(t2w);
    vec3 wo = w2t * V;

    for(int i = 0; i < nAreaLights; ++i) {
        vec3 pN = alights[i].plane.xyz;
				
        InitRectPoints(alights[i], points);

        if(!CullAreaLight(alights[i], points, v_position, N, -dot(v_position, N))) {
            // luminaire in tangent space
            vec3 ex = points[1] - points[0];
            vec3 ey = points[3] - points[0];
            SphericalQuad squad = SphericalQuadInit(points[0], ex, ey, v_position);
            float rcpSolidAngle = 1.0/squad.S;
            vec3 ez = normalize(cross(ex,ey));
            ez = w2t * ez;


            vec3 sum = vec3(0);
            for(int s = 1; s <= nSamples; ++s) {
                vec3 randSeed = v_position * globalTime * s;
                vec3 randVal = vec3(rand(randSeed.xy), rand(randSeed.xz), rand(randSeed.yz));//rand3(randSeed);//
                vec3 samplePos = SphericalQuadSample(squad, randVal.x, randVal.y);
                vec3 wi = normalize(samplePos - v_position);
                wi = w2t * wi;

                float cosTheta = wi.z;
                if(cosTheta > 0.0 /*&& (dot(wi,ez) < 0.0)*/) {
					vec3 diffBRDF = diff_color / M_PI;

					vec3 h = normalize(wi + wo);
					float LdotH = max(0, dot(wi, h));
					float NdotH = h.z;
					vec3 specBRDF = GGX(wi.z, wo.z, NdotH, LdotH, roughness, spec_color);

					float pdfLight = rcpSolidAngle;

                    sum += (diffBRDF + specBRDF) * cosTheta / (pdfLight);
                }
            }

            contrib += sum * alights[i].Ld;
        }
    }

    return contrib / float(nSamples);
}

void main() {
    // texturing
    vec3 diffuseTexColor = texture2D(DiffuseTex, v_texcoord).rgb;
    vec4 specularTexColor = texture2D(SpecularTex, v_texcoord);
    vec3 normalTexColor = texture2D(NormalTex, v_texcoord).rgb;
    float AmbientOcclusion = texture2D(AOTex, v_texcoord).r;

    // lighting
    vec3 Li = vec3(0);

    vec3 diffuse_color = Kd;
    vec3 specular_color = Ks * specularTexColor.rrr;
    float texShininess = specularTexColor.a < 1 ? specularTexColor.a : specularTexColor.r;
    float roughness = max(0.001, 1.0 - (shininess * texShininess));

    // Normal vector from Normalmap
    // vec3 objN = normalize(normalTexColor * 2.0 - 1.0);
    // objN = normalize(v_TBN * objN);

    vec3 V = normalize(eyePosition - v_position);
    vec3 N = FetchCorrectedNormal(NormalTex, v_texcoord, v_TBN, V);
    // N = N * 0.00000001 + normalize(v_normal);
    float NdotV = max(0, dot(N, V));

    if(GTMode == 0) {
        Li += pointLightContribution(N, V, NdotV, diffuse_color, specular_color, roughness);
        Li += areaLightContribution(N, V, NdotV, diffuse_color, specular_color, roughness);

        vec3 finalcol = Ka * AmbientOcclusion + Li;

        frag_color = vec4(finalcol, 1) *  // lighting
                    //  1 * //(0.3 + 0.7 * v_color) *  // color
                    vec4(diffuseTexColor, 1);                     // texture

        // To visualize depth :
        // frag_color = frag_color * 0.0001 + depthBuffer();
    } else {
        Li += pointLightContribution(N, V, NdotV, diffuse_color, specular_color, roughness);
        Li += areaLightGT(N, V, NdotV, diffuse_color, specular_color, roughness);

        vec3 finalcol = Ka * AmbientOcclusion + Li;
        frag_color = vec4(diffuseTexColor * finalcol, 1);
    }    
}
