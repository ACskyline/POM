#define VS_OUTPUT_LITE

#ifndef PS_OUTPUT_COUNT
#define PS_OUTPUT_COUNT 1
#endif

#include "GlobalInclude.hlsl"

#define SUN_DISTANCE 1000.0f
#define SUN_RADIUS 100.0f
#define SUN_RADIANCE_SCALING_FACTOR 1e6
#define EARTH_RADIUS 6378000.0f
#define ATMOSPHERE_THICKNESS 480000.0f
#define RAYLEIGH_SCATTERING_R 6.55e-6
#define RAYLEIGH_SCATTERING_G 1.73e-5
#define RAYLEIGH_SCATTERING_B 2.3e-5
#define MIE_SCATTERING 2e-6

struct PhaseParam
{
    float3 rayleighScattering;
    float3 rayleighExtinction;
    float3 mieScattering;
    float3 mieExtinction;
};

float IntersectSphereFast(float3 eye, float3 dir, float3 center, float radius)
{
    bool result = false;
    
    float3 centerVec = center - eye;
    float centerVecProj = dot(centerVec, dir);
    if(centerVecProj < 0.0f)
        return false;
    float d = length(centerVec - centerVecProj * dir);
    if (d < radius)
        result = true;
    return result;
}

// radius = 0.5
bool IntersectUnitSphereAtOrigin(float3 ori, float3 dir, inout float3 start, inout float3 finish)
{
    bool result = false;

    float A = dot(dir, dir);
    float B = 2 * dot(ori, dir);
    float C = dot(ori, ori) - 0.25;

    float delta = B * B - 4 * A * C;

    if (delta >= 0)
    {
        float t1 = (-B - sqrt(delta)) / (2 * A);
        float t2 = (-B + sqrt(delta)) / (2 * A);
		
        float tMin = t1 < t2 ? t1 : t2;
        float tMax = t1 < t2 ? t2 : t1;

        if (tMax >= 0)
        {
            tMin = max(0, tMin); //clamp start to camera if camera is inside of the volume

            start = ori + tMin * dir;
            finish = ori + tMax * dir;

            result = true;
        }
    }

    return result;
}

bool IntersectSphere(float3 ori, float3 dir, float3 center, float radius, inout float3 start, inout float3 finish)
{
    bool result = false;

    float3 co = ori - center;
    float A = dot(dir, dir);
    float B = 2 * dot(co, dir);
    float C = dot(co, co) - radius * radius;

    float delta = B * B - 4 * A * C;

    if (delta >= 0)
    {
        float t1 = (-B - sqrt(delta)) / (2 * A);
        float t2 = (-B + sqrt(delta)) / (2 * A);
		
        float tMin = t1 < t2 ? t1 : t2;
        float tMax = t1 < t2 ? t2 : t1;

        if (tMax >= 0)
        {
            tMin = max(0, tMin); //clamp start to camera if camera is inside of the volume

            start = ori + tMin * dir;
            finish = ori + tMax * dir;

            result = true;
        }
    }

    return result;
}

float RayleighDensity(float height)
{
    return exp(-height / 8000.0f);
}

float MieDensity(float height)
{
    return exp(-height / 1200.0f);
}

float Rayleigh(float cosTheta)
{
    return (1.0f + cosTheta * cosTheta) * 3.0f / 4.0f;
}

float Mie(float cosTheta, float g)
{
    // Henyey-Grennstein
    return (1.0f - g * g) / (4.0f * PI * pow((1.0f + g * g - 2.0f * g * cosTheta), 1.5f));
    // Cornette-Shanks
    //return 3.0f * (1.0f - g * g) * (1 + cosTheta * cosTheta) / (2.0f * (2.0f + g * g) * pow(1.0f + g * g - 2 * g * cosTheta * cosTheta, 1.5f));
}

PhaseParam DefaultPhaseParam()
{
    PhaseParam pp;
    pp.rayleighScattering = float3(RAYLEIGH_SCATTERING_R, RAYLEIGH_SCATTERING_G, RAYLEIGH_SCATTERING_B);
    pp.rayleighExtinction = pp.rayleighScattering;
    pp.mieScattering = float3(MIE_SCATTERING, MIE_SCATTERING, MIE_SCATTERING);
    pp.mieExtinction = pp.mieScattering / 0.9f;
    return pp;
}

float3 Transmittance(float3 earthPos, float earthRadius, float3 begin, float3 end, float3 dir)
{
    PhaseParam pp = DefaultPhaseParam();
    float3 deltaPos = (end - begin) / (float)sSkyMarchStepTr;
    float3 currentPos = begin;
    float rayleighDensity = 0.0f;
    float mieDensity = 0.0f;
    for (uint i = 0; i < sSkyMarchStepTr; i++)
    {
        float height = length(currentPos - earthPos) - earthRadius;
        rayleighDensity += RayleighDensity(height);
        mieDensity += MieDensity(height);
        currentPos += deltaPos;
    }
    return exp(-(pp.rayleighExtinction * rayleighDensity + pp.mieExtinction * mieDensity) / (float) sSkyMarchStepTr);
}

float3 MarchSky(float3 earthPos, float earthRadius, float atmosphereThickness, float3 sunDir, float3 sunRadiance, float3 begin, float3 end, float3 dir)
{
    PhaseParam pp = DefaultPhaseParam();
    float3 deltaPos = (end - begin) / (float)sSkyMarchStep;
    float3 currentPos = begin;
    float cosTheta = dot(dir, sunDir);
    float3 Ir = 0.0f.xxx;
    float3 Im = 0.0f.xxx;
    for (uint i = 0; i < sSkyMarchStep; i++)
    {
        float3 sunRayIntersectionStart = 0.0f.xxx;
        float3 sunRayIntersectionFinish = 0.0f.xxx;
        IntersectSphere(currentPos, sunDir, earthPos, earthRadius + atmosphereThickness, sunRayIntersectionStart, sunRayIntersectionFinish);
        
        float height = length(currentPos - earthPos) - earthRadius;
        float3 trOut = Transmittance(earthPos, earthRadius, begin, currentPos, dir);
        float3 trIn = Transmittance(earthPos, earthRadius, currentPos, sunRayIntersectionFinish, sunDir);
        Ir += RayleighDensity(height) * trIn * trOut;
        Im += MieDensity(height) * trIn * trOut;
        
        currentPos += deltaPos;
    }
    Ir *= Rayleigh(cosTheta) * pp.rayleighScattering / (4.0f * PI * (float)sSkyMarchStep);
    Im *= Mie(cosTheta, sSkyScatterG) * pp.mieScattering / (4.0f * PI * (float)sSkyMarchStep);
    return (Ir + Im) * sunRadiance;
}

PS_OUTPUT main(VS_OUTPUT input)
{
    float3 earthPos = float3(0.0f, -EARTH_RADIUS, 0.0f);
    float earthRadius = EARTH_RADIUS;
    float atmosphereThickness = ATMOSPHERE_THICKNESS;
    float3 sunPos = GetSunPos(sSunAzimuth, sSunAltitude, SUN_DISTANCE, pEyePos);
    float3 sunDir = normalize(sunPos - pEyePos);
    float3 sunRadiance = float3(sSunRadiance.r, sSunRadiance.g, sSunRadiance.b) * SUN_RADIANCE_SCALING_FACTOR;
    float sunRadius = SUN_RADIUS;
        
    PS_OUTPUT output;
    
    float2 posNDC = ScreenToNDC(input.pos.xy, float2(pWidth, pHeight)); // flip y
    float4 posWorldNear = mul(pViewProjInv, float4(posNDC, 0.0f, 1.0f) * pNearClipPlane);   
    float3 dirWorldNear = normalize(posWorldNear.xyz - pEyePos);
    
    if (IntersectSphereFast(pEyePos, dirWorldNear, earthPos, earthRadius)) // earth
        output.col0 = float4(0.2f, 0.2f, 0.25f, 1.0f);
    else // sky
    {
        float3 atmosphereEntrance = 0.0f.xxx;
        float3 atmosphereExit = 0.0f.xxx;
        if (IntersectSphere(pEyePos, dirWorldNear, earthPos, earthRadius + atmosphereThickness, atmosphereEntrance, atmosphereExit))
            output.col0 = float4(MarchSky(earthPos, earthRadius, atmosphereThickness, sunDir, sunRadiance, pEyePos, atmosphereExit, dirWorldNear), 1.0f);
    }
    output.col0.xyz = GammaCorrect(output.col0.xyz);
    return output;
}
