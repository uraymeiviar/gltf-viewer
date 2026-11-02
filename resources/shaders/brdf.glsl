//
// Fresnel
//
// http://graphicrants.blogspot.com/2013/08/specular-brdf-reference.html
// https://github.com/wdas/brdf/tree/master/src/brdfs
// https://google.github.io/filament/Filament.md.html
//

// The following equation models the Fresnel reflectance term of the spec equation (aka F())
// Implementation of fresnel from [4], Equation 15
vec3 F_Schlick(vec3 f0, vec3 f90, float VdotH)
{
    return f0 + (f90 - f0) * pow(clamp(1.0 - VdotH, 0.0, 1.0), 5.0);
}

float F_Schlick(float f0, float f90, float VdotH)
{
    float x = clamp(1.0 - VdotH, 0.0, 1.0);
    float x2 = x * x;
    float x5 = x * x2 * x2;
    return f0 + (f90 - f0) * x5;
}

float F_Schlick(float f0, float VdotH)
{
    float f90 = 1.0; //clamp(50.0 * f0, 0.0, 1.0);
    return F_Schlick(f0, f90, VdotH);
}

vec3 F_Schlick(vec3 f0, float f90, float VdotH)
{
    float x = clamp(1.0 - VdotH, 0.0, 1.0);
    float x2 = x * x;
    float x5 = x * x2 * x2;
    return f0 + (f90 - f0) * x5;
}

vec3 F_Schlick(vec3 f0, float VdotH)
{
    float f90 = 1.0; //clamp(dot(f0, vec3(50.0 * 0.33)), 0.0, 1.0);
    return F_Schlick(f0, f90, VdotH);
}

vec3 Schlick_to_F0(vec3 f, vec3 f90, float VdotH) {
    float x = clamp(1.0 - VdotH, 0.0, 1.0);
    float x2 = x * x;
    float x5 = clamp(x * x2 * x2, 0.0, 0.9999);

    return (f - f90 * x5) / (1.0 - x5);
}

float Schlick_to_F0(float f, float f90, float VdotH) {
    float x = clamp(1.0 - VdotH, 0.0, 1.0);
    float x2 = x * x;
    float x5 = clamp(x * x2 * x2, 0.0, 0.9999);

    return (f - f90 * x5) / (1.0 - x5);
}

vec3 Schlick_to_F0(vec3 f, float VdotH) {
    return Schlick_to_F0(f, vec3(1.0), VdotH);
}

float Schlick_to_F0(float f, float VdotH) {
    return Schlick_to_F0(f, 1.0, VdotH);
}


// Smith Joint GGX
// Note: Vis = G / (4 * NdotL * NdotV)
// see Eric Heitz. 2014. Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs. Journal of Computer Graphics Techniques, 3
// see Real-Time Rendering. Page 331 to 336.
// see https://google.github.io/filament/Filament.md.html#materialsystem/specularbrdf/geometricshadowing(specularg)
float V_GGX(float NdotL, float NdotV, float alphaRoughness)
{
    float alphaRoughnessSq = alphaRoughness * alphaRoughness;

    float GGXV = NdotL * sqrt(NdotV * NdotV * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);
    float GGXL = NdotV * sqrt(NdotL * NdotL * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);

    float GGX = GGXV + GGXL;
    if (GGX > 0.0)
    {
        return 0.5 / GGX;
    }
    return 0.0;
}


// The following equation(s) model the distribution of microfacet normals across the area being drawn (aka D())
// Implementation from "Average Irregularity Representation of a Roughened Surface for Ray Reflection" by T. S. Trowbridge, and K. P. Reitz
// Follows the distribution function recommended in the SIGGRAPH 2013 course notes from EPIC Games [1], Equation 3.
float D_GGX(float NdotH, float alphaRoughness)
{
    float alphaRoughnessSq = alphaRoughness * alphaRoughness;
    float f = (NdotH * NdotH) * (alphaRoughnessSq - 1.0) + 1.0;
    return alphaRoughnessSq / (M_PI * f * f);
}


float lambdaSheenNumericHelper(float x, float alphaG)
{
    float oneMinusAlphaSq = (1.0 - alphaG) * (1.0 - alphaG);
    float a = mix(21.5473, 25.3245, oneMinusAlphaSq);
    float b = mix(3.82987, 3.32435, oneMinusAlphaSq);
    float c = mix(0.19823, 0.16801, oneMinusAlphaSq);
    float d = mix(-1.97760, -1.27393, oneMinusAlphaSq);
    float e = mix(-4.32054, -4.85967, oneMinusAlphaSq);
    return a / (1.0 + b * pow(x, c)) + d * x + e;
}


float lambdaSheen(float cosTheta, float alphaG)
{
    if (abs(cosTheta) < 0.5)
    {
        return exp(lambdaSheenNumericHelper(cosTheta, alphaG));
    }
    else
    {
        return exp(2.0 * lambdaSheenNumericHelper(0.5, alphaG) - lambdaSheenNumericHelper(1.0 - cosTheta, alphaG));
    }
}


float V_Sheen(float NdotL, float NdotV, float sheenRoughness)
{
    sheenRoughness = max(sheenRoughness, 0.000001); //clamp (0,1]
    float alphaG = sheenRoughness * sheenRoughness;

    return clamp(1.0 / ((1.0 + lambdaSheen(NdotV, alphaG) + lambdaSheen(NdotL, alphaG)) *
        (4.0 * NdotV * NdotL)), 0.0, 1.0);
}


//Sheen implementation-------------------------------------------------------------------------------------
// See  https://github.com/sebavan/glTF/tree/KHR_materials_sheen/extensions/2.0/Khronos/KHR_materials_sheen

// Estevez and Kulla http://www.aconty.com/pdf/s2017_pbs_imageworks_sheen.pdf
float D_Charlie(float sheenRoughness, float NdotH)
{
    sheenRoughness = max(sheenRoughness, 0.000001); //clamp (0,1]
    float alphaG = sheenRoughness * sheenRoughness;
    float invR = 1.0 / alphaG;
    float cos2h = NdotH * NdotH;
    float sin2h = 1.0 - cos2h;
    return (2.0 + invR) * pow(sin2h, invR * 0.5) / (2.0 * M_PI);
}


//https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#acknowledgments AppendixB
vec3 BRDF_lambertian(vec3 f0, vec3 f90, vec3 diffuseColor, float specularWeight, float VdotH)
{
    // see https://seblagarde.wordpress.com/2012/01/08/pi-or-not-to-pi-in-game-lighting-equation/
    return (1.0 - specularWeight * F_Schlick(f0, f90, VdotH)) * (diffuseColor / M_PI);
}


#ifdef MATERIAL_IRIDESCENCE
//https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#acknowledgments AppendixB
vec3 BRDF_lambertianIridescence(vec3 f0, vec3 f90, vec3 iridescenceFresnel, float iridescenceFactor, vec3 diffuseColor, float specularWeight, float VdotH)
{
    // Use the maximum component of the iridescence Fresnel color
    // Maximum is used instead of the RGB value to not get inverse colors for the diffuse BRDF
    vec3 iridescenceFresnelMax = vec3(max(max(iridescenceFresnel.r, iridescenceFresnel.g), iridescenceFresnel.b));

    vec3 schlickFresnel = F_Schlick(f0, f90, VdotH);

    // Blend default specular Fresnel with iridescence Fresnel
    vec3 F = mix(schlickFresnel, iridescenceFresnelMax, iridescenceFactor);

    // see https://seblagarde.wordpress.com/2012/01/08/pi-or-not-to-pi-in-game-lighting-equation/
    return (1.0 - specularWeight * F) * (diffuseColor / M_PI);
}
#endif


//  https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#acknowledgments AppendixB
vec3 BRDF_specularGGX(vec3 f0, vec3 f90, float alphaRoughness, float specularWeight, float VdotH, float NdotL, float NdotV, float NdotH)
{
    vec3 F = F_Schlick(f0, f90, VdotH);
    float Vis = V_GGX(NdotL, NdotV, alphaRoughness);
    float D = D_GGX(NdotH, alphaRoughness);

    return specularWeight * F * Vis * D;
}


#ifdef MATERIAL_IRIDESCENCE
vec3 BRDF_specularGGXIridescence(vec3 f0, vec3 f90, vec3 iridescenceFresnel, float alphaRoughness, float iridescenceFactor, float specularWeight, float VdotH, float NdotL, float NdotV, float NdotH)
{
    vec3 F = mix(F_Schlick(f0, f90, VdotH), iridescenceFresnel, iridescenceFactor);
    float Vis = V_GGX(NdotL, NdotV, alphaRoughness);
    float D = D_GGX(NdotH, alphaRoughness);

    return specularWeight * F * Vis * D;
}
#endif


#ifdef MATERIAL_ANISOTROPY
// KHR_materials_anisotropy (glTF-Sample-Viewer): anisotropic GGX distribution and the matching
// height-correlated Smith visibility. With at == ab both reduce to D_GGX / V_GGX.
float D_GGX_anisotropic(float NdotH, float TdotH, float BdotH, float at, float ab)
{
    float a2 = at * ab;
    vec3 f = vec3(ab * TdotH, at * BdotH, a2 * NdotH);
    float w2 = a2 / max(dot(f, f), 1e-30);
    return a2 * w2 * w2 / M_PI;
}


float V_GGX_anisotropic(float NdotL, float NdotV, float BdotV, float TdotV, float TdotL, float BdotL, float at, float ab)
{
    float GGXV = NdotL * length(vec3(at * TdotV, ab * BdotV, NdotV));
    float GGXL = NdotV * length(vec3(at * TdotL, ab * BdotL, NdotL));
    float GGX = GGXV + GGXL;
    return GGX > 0.0 ? clamp(0.5 / GGX, 0.0, 1.0) : 0.0;
}


// Without NdotL. alphaRoughness is the roughness along b; along t it rises to 1 with anisotropy^2.
vec3 BRDF_specularGGXAnisotropy(vec3 F, float alphaRoughness, float anisotropy, vec3 n, vec3 v, vec3 l, vec3 h, vec3 t, vec3 b)
{
    float ab = clamp(alphaRoughness, 0.001, 1.0);
    float at = max(mix(alphaRoughness, 1.0, anisotropy * anisotropy), ab);

    float NdotL = clamp(dot(n, l), 0.0, 1.0);
    float NdotH = clamp(dot(n, h), 0.001, 1.0);
    float NdotV = clamp(dot(n, v), 0.0, 1.0);

    float V = V_GGX_anisotropic(NdotL, NdotV, dot(b, v), dot(t, v), dot(t, l), dot(b, l), at, ab);
    float D = D_GGX_anisotropic(NdotH, dot(t, h), dot(b, h), at, ab);
    return F * (V * D);
}
#endif


// f_sheen
vec3 BRDF_specularSheen(vec3 sheenColor, float sheenRoughness, float NdotL, float NdotV, float NdotH)
{
    float sheenDistribution = D_Charlie(sheenRoughness, NdotH);
    float sheenVisibility = V_Sheen(NdotL, NdotV, sheenRoughness);
    return sheenColor * sheenDistribution * sheenVisibility;
}


#ifdef MATERIAL_DIFFUSE_ROUGHNESS
// KHR_materials_diffuse_roughness: the energy-preserving Oren-Nayar model (EON) of Portsmouth, Kutz and Hill, "EON: A
// practical energy-preserving rough diffuse BRDF" (JCGT 2025, arXiv:2410.18026, listing 1; the OpenPBR base substrate
// model the draft extension recommends), with the paper's fast albedo approximation E_FON_approx. rho is the diffuse
// albedo (c_diff), r the roughness in [0, 1], mu_i = N.L, mu_o = N.V, LdotV = dot(l, v). Returns the BRDF value
// (including 1/pi); r = 0 is Lambert (rho / pi) analytically, but the caller keeps the Lambert code for r = 0.
const float kEonConstant1 = 0.5 - 2.0 / (3.0 * M_PI);            // FON A coefficient term
const float kEonConstant2 = 2.0 / 3.0 - 28.0 / (15.0 * M_PI);    // FON average albedo term

float E_FON_approx(float mu, float r)
{
    float mucomp = 1.0 - mu;
    const float g1 = 0.0571085289;
    const float g2 = 0.491881867;
    const float g3 = -0.332181442;
    const float g4 = 0.0714429953;
    float GoverPi = mucomp * (g1 + mucomp * (g2 + mucomp * (g3 + mucomp * g4)));
    return (1.0 + r * GoverPi) / (1.0 + kEonConstant1 * r);
}

vec3 f_EON(vec3 rho, float r, float mu_i, float mu_o, float LdotV)
{
    float s = LdotV - mu_i * mu_o;                               // QON s term
    float sovertF = s > 0.0 ? s / max(max(mu_i, mu_o), 1e-7) : s; // FON s / t
    float AF = 1.0 / (1.0 + kEonConstant1 * r);                  // FON A coefficient
    vec3 f_ss = (rho / M_PI) * AF * (1.0 + r * sovertF);         // single scattering
    float EFo = E_FON_approx(mu_o, r);                            // FON albedo, outgoing direction
    float EFi = E_FON_approx(mu_i, r);                            // FON albedo, incoming direction
    float avgEF = AF * (1.0 + kEonConstant2 * r);                 // average albedo
    vec3 rho_ms = (rho * rho) * avgEF / (vec3(1.0) - rho * (1.0 - avgEF));
    const float eps = 1.0e-7;
    vec3 f_ms = (rho_ms / M_PI) * max(eps, 1.0 - EFo) * max(eps, 1.0 - EFi) / max(eps, 1.0 - avgEF); // multiple scattering
    return f_ss + f_ms;
}
#endif


#ifdef MATERIAL_DIFFUSE_TRANSMISSION
// KHR_materials_diffuse_transmission, direct light (glTF-Sample-Renderer pbr.frag): a light behind the surface
// (dot(n, l) < 0) is transmitted by the Lambert BTDF of the transmission colour, with N.L taken on the back side
// (clampedDot(-n, l)) and the Fresnel weight from the half vector of the light mirrored onto the front side. The
// caller multiplies by the diffuse transmission factor (the BSDF mix) and applies the volume attenuation.
vec3 diffuseTransmissionBtdf(vec3 intensity, vec3 n, vec3 v, vec3 l, vec3 f0, vec3 f90, float specularWeight, vec3 color)
{
    float backNdotL = clampedDot(-n, l);
    vec3 mirrored = normalize(l + 2.0 * n * dot(-l, n));
    float mirroredVdotH = clampedDot(v, normalize(mirrored + v));
    return intensity * backNdotL * BRDF_lambertian(f0, f90, color, specularWeight, mirroredVdotH);
}
#endif
