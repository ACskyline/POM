#ifndef WATERSIM_CELLFACE_RESOURCES_UTIL_H
#define WATERSIM_CELLFACE_RESOURCES_UTIL_H

#define DEF_INTERPOLATE_CELL_FACE(AXIS) \
WaterSimCellFace InterpolateCellFaceInternal##AXIS##(\
	float3 pos, \
	uint3 closestFloorCellFaceIndexXYZ_O, \
	uint3 closestFloorCellFaceIndexXYZ_X, \
	uint3 closestFloorCellFaceIndexXYZ_Y, \
	uint3 closestFloorCellFaceIndexXYZ_Z, \
	float3 closestFloorCellFaceCenterPosXYZ_O, \
	float3 closestFloorCellFaceCenterPosXYZ_X, \
	float3 closestFloorCellFaceCenterPosXYZ_Y, \
	float3 closestFloorCellFaceCenterPosXYZ_Z) \
{ \
	uint3 closestFloorCellFaceIndexXYZ_XY = clamp(closestFloorCellFaceIndexXYZ_O + uint3(1, 1, 0), 0, uint3(uPass.mCellCount /*+ 1*/)); \
	uint3 closestFloorCellFaceIndexXYZ_XZ = clamp(closestFloorCellFaceIndexXYZ_O + uint3(1, 0, 1), 0, uint3(uPass.mCellCount /*+ 1*/)); \
	uint3 closestFloorCellFaceIndexXYZ_YZ = clamp(closestFloorCellFaceIndexXYZ_O + uint3(0, 1, 1), 0, uint3(uPass.mCellCount /*+ 1*/)); \
	uint3 closestFloorCellFaceIndexXYZ_XYZ = clamp(closestFloorCellFaceIndexXYZ_O + uint3(1, 1, 1), 0, uint3(uPass.mCellCount /*+ 1*/)); \
	WaterSimCellFace source_O = gWaterSimCellFaceBuffer[FlattenCell##AXIS##FaceIndex(closestFloorCellFaceIndexXYZ_O)]; \
	WaterSimCellFace source_X = gWaterSimCellFaceBuffer[FlattenCell##AXIS##FaceIndex(closestFloorCellFaceIndexXYZ_X)]; \
	WaterSimCellFace source_Y = gWaterSimCellFaceBuffer[FlattenCell##AXIS##FaceIndex(closestFloorCellFaceIndexXYZ_Y)]; \
	WaterSimCellFace source_Z = gWaterSimCellFaceBuffer[FlattenCell##AXIS##FaceIndex(closestFloorCellFaceIndexXYZ_Z)]; \
	WaterSimCellFace source_XY = gWaterSimCellFaceBuffer[FlattenCell##AXIS##FaceIndex(closestFloorCellFaceIndexXYZ_XY)]; \
	WaterSimCellFace source_XZ = gWaterSimCellFaceBuffer[FlattenCell##AXIS##FaceIndex(closestFloorCellFaceIndexXYZ_XZ)]; \
	WaterSimCellFace source_YZ = gWaterSimCellFaceBuffer[FlattenCell##AXIS##FaceIndex(closestFloorCellFaceIndexXYZ_YZ)]; \
	WaterSimCellFace source_XYZ = gWaterSimCellFaceBuffer[FlattenCell##AXIS##FaceIndex(closestFloorCellFaceIndexXYZ_XYZ)]; \
	float weightX = saturate((pos.x - closestFloorCellFaceCenterPosXYZ_O.x) / (closestFloorCellFaceCenterPosXYZ_X.x - closestFloorCellFaceCenterPosXYZ_O.x)); \
	WaterSimCellFace O_X = LerpCellFace(source_O, source_X, weightX); \
	WaterSimCellFace Z_XZ = LerpCellFace(source_Z, source_XZ, weightX); \
	WaterSimCellFace Y_XY = LerpCellFace(source_Y, source_XY, weightX); \
	WaterSimCellFace YZ_XYZ = LerpCellFace(source_YZ, source_XYZ, weightX); \
	float weightY = saturate((pos.y - closestFloorCellFaceCenterPosXYZ_O.y) / (closestFloorCellFaceCenterPosXYZ_Y.y - closestFloorCellFaceCenterPosXYZ_O.y)); \
	WaterSimCellFace OX_XY = LerpCellFace(O_X, Y_XY, weightY); \
	WaterSimCellFace ZXZ_YZXYZ = LerpCellFace(Z_XZ, YZ_XYZ, weightY); \
	float weightZ = saturate((pos.z - closestFloorCellFaceCenterPosXYZ_O.z) / (closestFloorCellFaceCenterPosXYZ_Z.z - closestFloorCellFaceCenterPosXYZ_O.z)); \
	WaterSimCellFace result = LerpCellFace(OX_XY, ZXZ_YZXYZ, weightZ); \
	return result; \
} \
WaterSimCellFace InterpolateCell##AXIS##Face(float3 pos) \
{ \
	uint closestFloorCellFaceIndex_O = 0; \
	uint3 closestFloorCellFaceIndexXYZ_O = GetClosestFloorCell##AXIS##FaceIndex(pos, closestFloorCellFaceIndex_O); \
	uint3 closestFloorCellFaceIndexXYZ_X = clamp(closestFloorCellFaceIndexXYZ_O + uint3(1, 0, 0), 0, uPass.mCellCount /*+ 1*/); \
	uint3 closestFloorCellFaceIndexXYZ_Y = clamp(closestFloorCellFaceIndexXYZ_O + uint3(0, 1, 0), 0, uPass.mCellCount /*+ 1*/); \
	uint3 closestFloorCellFaceIndexXYZ_Z = clamp(closestFloorCellFaceIndexXYZ_O + uint3(0, 0, 1), 0, uPass.mCellCount /*+ 1*/); \
	float3 closestFloorCellFaceCenterPosXYZ_O = GetCell##AXIS##FaceCenterPos(closestFloorCellFaceIndexXYZ_O); \
	float3 closestFloorCellFaceCenterPosXYZ_X = GetCell##AXIS##FaceCenterPos(closestFloorCellFaceIndexXYZ_X); \
	float3 closestFloorCellFaceCenterPosXYZ_Y = GetCell##AXIS##FaceCenterPos(closestFloorCellFaceIndexXYZ_Y); \
	float3 closestFloorCellFaceCenterPosXYZ_Z = GetCell##AXIS##FaceCenterPos(closestFloorCellFaceIndexXYZ_Z); \
	return InterpolateCellFaceInternal##AXIS##( \
		pos, \
		closestFloorCellFaceIndexXYZ_O, \
		closestFloorCellFaceIndexXYZ_X, \
		closestFloorCellFaceIndexXYZ_Y, \
		closestFloorCellFaceIndexXYZ_Z, \
		closestFloorCellFaceCenterPosXYZ_O, \
		closestFloorCellFaceCenterPosXYZ_X, \
		closestFloorCellFaceCenterPosXYZ_Y, \
		closestFloorCellFaceCenterPosXYZ_Z); \
}

DEF_INTERPOLATE_CELL_FACE(X)
DEF_INTERPOLATE_CELL_FACE(Y)
DEF_INTERPOLATE_CELL_FACE(Z)

#define InterpolateCellFace(AXIS, pos) InterpolateCell##AXIS##Face(pos)

float3 InterpolateVelocity(float3 pos)
{
	return float3(InterpolateCellFace(X, pos).mVelocity, InterpolateCellFace(Y, pos).mVelocity, InterpolateCellFace(Z, pos).mVelocity);
}

float3 GetCellCenterVelocity(uint3 cellIndexXYZ)
{
	return InterpolateVelocity(GetCellCenterPos(cellIndexXYZ));
}

WaterSimCell InitCell()
{
	WaterSimCell result;
	result.mParticleCount = 0;
	result.mPressure = 0.0f;
	result.mType = 0.0f;
	result.mSolidVelocity = 0.0f.xxx;
	result.mIteration = 0;
	result.mReadByNeighbor = 0;
	return result;
}

WaterSimCell LerpCell(WaterSimCell A, WaterSimCell B, float weight)
{
	// TODO: interpolate the necessary attributes, e.g. density, temperature
	return InitCell();
}

WaterSimCell InterpolateCell(float3 pos)
{
	uint closetFloorCellIndex_O = 0;
	uint3 closetFloorCellIndexXYZ_O = GetFloorCellCenterIndex(pos, closetFloorCellIndex_O);
	uint3 closetFloorCellIndexXYZ_X = clamp(closetFloorCellIndexXYZ_O + uint3(1, 0, 0), 0, uint3(uPass.mCellCount));
	uint3 closetFloorCellIndexXYZ_Y = clamp(closetFloorCellIndexXYZ_O + uint3(0, 1, 0), 0, uint3(uPass.mCellCount));
	uint3 closetFloorCellIndexXYZ_Z = clamp(closetFloorCellIndexXYZ_O + uint3(0, 0, 1), 0, uint3(uPass.mCellCount));
	uint3 closetFloorCellIndexXYZ_XY = clamp(closetFloorCellIndexXYZ_O + uint3(1, 1, 0), 0, uint3(uPass.mCellCount));
	uint3 closetFloorCellIndexXYZ_XZ = clamp(closetFloorCellIndexXYZ_O + uint3(1, 0, 1), 0, uint3(uPass.mCellCount));
	uint3 closetFloorCellIndexXYZ_YZ = clamp(closetFloorCellIndexXYZ_O + uint3(0, 1, 1), 0, uint3(uPass.mCellCount));
	uint3 closetFloorCellIndexXYZ_XYZ = clamp(closetFloorCellIndexXYZ_O + uint3(1, 1, 1), 0, uint3(uPass.mCellCount));
	WaterSimCell source_O = gWaterSimCellBuffer[closetFloorCellIndex_O];
	WaterSimCell source_X = gWaterSimCellBuffer[FlattenCellIndexClamp(closetFloorCellIndexXYZ_X)];
	WaterSimCell source_Y = gWaterSimCellBuffer[FlattenCellIndexClamp(closetFloorCellIndexXYZ_Y)];
	WaterSimCell source_Z = gWaterSimCellBuffer[FlattenCellIndexClamp(closetFloorCellIndexXYZ_Z)];
	WaterSimCell source_XY = gWaterSimCellBuffer[FlattenCellIndexClamp(closetFloorCellIndexXYZ_XY)];
	WaterSimCell source_XZ = gWaterSimCellBuffer[FlattenCellIndexClamp(closetFloorCellIndexXYZ_XZ)];
	WaterSimCell source_YZ = gWaterSimCellBuffer[FlattenCellIndexClamp(closetFloorCellIndexXYZ_YZ)];
	WaterSimCell source_XYZ = gWaterSimCellBuffer[FlattenCellIndexClamp(closetFloorCellIndexXYZ_XYZ)];
	// tri linear interpolation
	// x axis
	float3 closetFloorCellCenterPosXYZ_O = GetCellCenterPos(closetFloorCellIndexXYZ_O);
	float3 closetFloorCellCenterPosXYZ_X = GetCellCenterPos(closetFloorCellIndexXYZ_X);
	float3 closetFloorCellCenterPosXYZ_Y = GetCellCenterPos(closetFloorCellIndexXYZ_Y);
	float3 closetFloorCellCenterPosXYZ_Z = GetCellCenterPos(closetFloorCellIndexXYZ_Z);
	float weightX = saturate((pos.x - closetFloorCellCenterPosXYZ_O.x) / (closetFloorCellCenterPosXYZ_X.x - closetFloorCellCenterPosXYZ_O.x));
	WaterSimCell O_X = LerpCell(source_O, source_X, weightX);
	WaterSimCell Z_XZ = LerpCell(source_Z, source_XZ, weightX);
	WaterSimCell Y_XY = LerpCell(source_Y, source_XY, weightX);
	WaterSimCell YZ_XYZ = LerpCell(source_YZ, source_XYZ, weightX);
	// y axis
	float weightY = saturate((pos.y - closetFloorCellCenterPosXYZ_O.y) / (closetFloorCellCenterPosXYZ_Y.y - closetFloorCellCenterPosXYZ_O.y));
	WaterSimCell OX_XY = LerpCell(O_X, Y_XY, weightY);
	WaterSimCell ZXZ_YZXYZ = LerpCell(Z_XZ, YZ_XYZ, weightY);
	// z axis
	float weightZ = saturate((pos.z - closetFloorCellCenterPosXYZ_O.z) / (closetFloorCellCenterPosXYZ_Z.z - closetFloorCellCenterPosXYZ_O.z));
	WaterSimCell result = LerpCell(OX_XY, ZXZ_YZXYZ, weightZ);
	return result;
}

float3 GetCellVelocityNeg(uint3 cellIndexXYZ)
{
	uint3 cellFaceIndices_Neg = FlattenCellFaceIndices(cellIndexXYZ, cellIndexXYZ, cellIndexXYZ);
	return float3(gWaterSimCellFaceBuffer[cellFaceIndices_Neg.x].mVelocity,
		gWaterSimCellFaceBuffer[cellFaceIndices_Neg.y].mVelocity,
		gWaterSimCellFaceBuffer[cellFaceIndices_Neg.z].mVelocity);
}

float3 GetCellVelocityPos(uint3 cellIndexXYZ)
{
	uint3 cellFaceIndices_Pos = FlattenCellFaceIndices(cellIndexXYZ + uint3(1, 0, 0), cellIndexXYZ + uint3(0, 1, 0), cellIndexXYZ + uint3(0, 0, 1));
	return float3(gWaterSimCellFaceBuffer[cellFaceIndices_Pos.x].mVelocity,
		gWaterSimCellFaceBuffer[cellFaceIndices_Pos.y].mVelocity,
		gWaterSimCellFaceBuffer[cellFaceIndices_Pos.z].mVelocity);
}


void InterLockedAddCellFaceVelocity(uint faceCellIndex, float velocity)
{
	while (true)
	{
		uint oldVelocityU32 = gWaterSimCellFaceBuffer[faceCellIndex].mVelocityU32;
		float oldVelocity = asfloat(oldVelocityU32);
		uint originalVelocityU32;
		InterlockedCompareExchange(gWaterSimCellFaceBuffer[faceCellIndex].mVelocityU32, oldVelocityU32, asuint(oldVelocity + velocity), originalVelocityU32);
		if (originalVelocityU32 == oldVelocityU32) // chosen
			break;
	}
}

void InterLockedAddCellFaceWeight(uint faceCellIndex, float weight)
{
	while (true)
	{
		uint oldWeightU32 = gWaterSimCellFaceBuffer[faceCellIndex].mWeightU32;
		float oldWeight = asfloat(oldWeightU32);
		uint originalWeightU32;
		InterlockedCompareExchange(gWaterSimCellFaceBuffer[faceCellIndex].mWeightU32, oldWeightU32, asuint(oldWeight + weight), originalWeightU32);
		if (originalWeightU32 == oldWeightU32) // chosen
			break;
	}
}
#endif // WATERSIM_CELLFACE_RESOURCES_UTIL_H
