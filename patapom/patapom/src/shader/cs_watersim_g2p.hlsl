#define CUSTOM_PASS_UNIFORM

#include "ShaderInclude.hlsli"
#include "WaterSimUtil.hlsli"

StructuredBuffer<WaterSimCellFace> gWaterSimCellFaceBuffer : register(t0, SPACE(PASS));
RWStructuredBuffer<WaterSimCell> gWaterSimCellBuffer : register(u0, SPACE(PASS));
RWStructuredBuffer<WaterSimParticle> gWaterSimParticleBuffer : register(u1, SPACE(PASS));

#include "WaterSimResourceUtil.hlsli"

float3 ClipPosition(float3 pos, float3 velocity, float timeStep)
{
	float3 newPos = pos + velocity * timeStep;
	if (any(newPos <= 1.0f.xxx * uPass.mCellSize) || any(newPos >= (uPass.mCellCount - 1.0f) * uPass.mCellSize))
	{
		// simply clipping doesn't work, we need to find the closest contact point
		// assuming solid boundaries
		// return clamp(pos, 1.0f.xxx * uPass.mCellSize, (uPass.mCellCount - 1.0f) * uPass.mCellSize);
		// return clamp(pos, 1.001f.xxx * uPass.mCellSize, (uPass.mCellCount - 1.001f) * uPass.mCellSize);
		newPos = clamp(pos, (1.0f.xxx + EPSILON) * uPass.mCellSize, (uPass.mCellCount - (1.0f.xxx + EPSILON)) * uPass.mCellSize);

		//float3 disNeg = 1.0f.xxx * uPass.mCellSize - pos;
		//float3 disPos = (uPass.mCellCount - 1.0f.xxx) * uPass.mCellSize - pos;
		//float3 tNeg = max(0.0f, disNeg / velocity);
		//float3 tPos = max(0.0f, disPos / velocity);
		//float3 t = min(tNeg, tPos);
		//float newTimeStep = min(t.x, min(t.y, t.z));
		//newPos = pos + velocity * (newTimeStep - EPSILON);
		//newPos = clamp(newPos, 1.01f.xxx * uPass.mCellSize, (uPass.mCellCount - 1.01f) * uPass.mCellSize);
	}
	return newPos;
}

[numthreads(WATERSIM_PARTICLE_THREAD_PER_THREADGROUP_X, WATERSIM_PARTICLE_THREAD_PER_THREADGROUP_Y, WATERSIM_PARTICLE_THREAD_PER_THREADGROUP_Z)]
void main(uint3 gGroupID : SV_GroupID, uint gGroupIndex : SV_GroupIndex)
{
	uint particleIndex = GetParticleThreadIndex(gGroupID, gGroupIndex);
	if (particleIndex < uPass.mParticleCount)
	{
		// 3. G2P - interpolated delta velocity
		WaterSimParticle particle = gWaterSimParticleBuffer[particleIndex];
		if (particle.mAlive)
		{
			uint3 minFaceFlattenedIndices, maxFaceFlattenedIndices;
			float3 minFaceWeights, maxFaceWeights;
			uint cellIndex = GetCellFaceIndicesAndWeight(particle.mPos, minFaceFlattenedIndices, maxFaceFlattenedIndices, minFaceWeights, maxFaceWeights);
			uint3 cellIndexXYZ = DeflattenCellIndexXYZ(cellIndex);

			uint indexPosI = FlattenCellIndex(cellIndexXYZ + uint3(1, 0, 0));
			uint indexNegI = FlattenCellIndex(cellIndexXYZ - uint3(1, 0, 0));
			uint indexPosJ = FlattenCellIndex(cellIndexXYZ + uint3(0, 1, 0));
			uint indexNegJ = FlattenCellIndex(cellIndexXYZ - uint3(0, 1, 0));
			uint indexPosK = FlattenCellIndex(cellIndexXYZ + uint3(0, 0, 1));
			uint indexNegK = FlattenCellIndex(cellIndexXYZ - uint3(0, 0, 1));

			bool solidCellPosI = gWaterSimCellBuffer[indexPosI].mType == 1;
			bool solidCellNegI = gWaterSimCellBuffer[indexNegI].mType == 1;
			bool solidCellPosJ = gWaterSimCellBuffer[indexPosJ].mType == 1;
			bool solidCellNegJ = gWaterSimCellBuffer[indexNegJ].mType == 1;
			bool solidCellPosK = gWaterSimCellBuffer[indexPosK].mType == 1;
			bool solidCellNegK = gWaterSimCellBuffer[indexNegK].mType == 1;

			//if (solidCellPosI || solidCellNegI)
			//{
			//	if (solidCellPosI && solidCellNegI)
			//	{
			//		maxFaceWeights.x = 0.0f;
			//		minFaceWeights.x = 0.0f;
			//	}
			//	else if (solidCellPosI)
			//	{
			//		maxFaceWeights.x = 0.0f;
			//		minFaceWeights.x = 1.0f;
			//	}
			//	else
			//	{
			//		maxFaceWeights.x = 1.0f;
			//		minFaceWeights.x = 0.0f;
			//	}
			//}

			//if (solidCellPosJ || solidCellNegJ)
			//{
			//	if (solidCellPosJ && solidCellNegJ)
			//	{
			//		maxFaceWeights.y = 0.0f;
			//		minFaceWeights.y = 0.0f;
			//	}
			//	else if (solidCellPosJ)
			//	{
			//		maxFaceWeights.y = 0.0f;
			//		minFaceWeights.y = 1.0f;
			//	}
			//	else
			//	{
			//		maxFaceWeights.y = 1.0f;
			//		minFaceWeights.y = 0.0f;
			//	}
			//}

			//if (solidCellPosK || solidCellNegK)
			//{
			//	if (solidCellPosK && solidCellNegK)
			//	{
			//		maxFaceWeights.z = 0.0f;
			//		minFaceWeights.z = 0.0f;
			//	}
			//	else if (solidCellPosK)
			//	{
			//		maxFaceWeights.z = 0.0f;
			//		minFaceWeights.z = 1.0f;
			//	}
			//	else
			//	{
			//		maxFaceWeights.z = 1.0f;
			//		minFaceWeights.z = 0.0f;
			//	}
			//}

			InterlockedAdd(gWaterSimCellBuffer[cellIndex].mParticleCount, -1);
			float3 velocity =
				minFaceWeights * float3(gWaterSimCellFaceBuffer[minFaceFlattenedIndices.x].mVelocity,
					gWaterSimCellFaceBuffer[minFaceFlattenedIndices.y].mVelocity,
					gWaterSimCellFaceBuffer[minFaceFlattenedIndices.z].mVelocity) +
				maxFaceWeights * float3(gWaterSimCellFaceBuffer[maxFaceFlattenedIndices.x].mVelocity,
					gWaterSimCellFaceBuffer[maxFaceFlattenedIndices.y].mVelocity,
					gWaterSimCellFaceBuffer[maxFaceFlattenedIndices.z].mVelocity);
			float3 oldVelocity = 
				minFaceWeights * float3(gWaterSimCellFaceBuffer[minFaceFlattenedIndices.x].mOldVelocity,
					gWaterSimCellFaceBuffer[minFaceFlattenedIndices.y].mOldVelocity,
					gWaterSimCellFaceBuffer[minFaceFlattenedIndices.z].mOldVelocity) +
				maxFaceWeights * float3(gWaterSimCellFaceBuffer[maxFaceFlattenedIndices.x].mOldVelocity,
					gWaterSimCellFaceBuffer[maxFaceFlattenedIndices.y].mOldVelocity,
					gWaterSimCellFaceBuffer[maxFaceFlattenedIndices.z].mOldVelocity);
			
			// debug
			// float3 velocity = InterpolateVelocity(particle.mPos);
			
			particle.mVelocity = (1.0f - uPass.mFlipScale) * velocity + uPass.mFlipScale * (particle.mVelocity + velocity - oldVelocity);

			// for debug purposes
			particle.mMinFaceWeights = minFaceWeights;
			particle.mMaxFaceWeights = maxFaceWeights;
			particle.mCellFaceMinIndexXYZ = minFaceFlattenedIndices;
			particle.mCellFaceMaxIndexXYZ = maxFaceFlattenedIndices;
			particle.mCellIndexXYZ = cellIndexXYZ;

			// 4. move particles
			particle.mPos = ClipPosition(particle.mPos, particle.mVelocity, WaterSimTimeStep);

			// write back
			gWaterSimParticleBuffer[particleIndex] = particle;
		}
	}
}
