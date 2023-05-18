#ifndef WATERSIM_CELL_RESOURCES_UTIL_H
#define WATERSIM_CELL_RESOURCES_UTIL_H

void InterLockedAddCellVelocityX(uint cellIndex, float velocity)
{
	[allow_uav_condition]
	while (true)
	{
		uint oldVelocityU32 = gWaterSimCellBuffer[cellIndex].mVelocityXU32;
		float oldVelocity = asfloat(oldVelocityU32);
		if (abs(oldVelocity + velocity) > WATERSIM_VELOCITY_MAX)
			break;
		uint originalVelocityU32;
		InterlockedCompareExchange(gWaterSimCellBuffer[cellIndex].mVelocityXU32, oldVelocityU32, asuint(oldVelocity + velocity), originalVelocityU32);
		if (originalVelocityU32 == oldVelocityU32) // chosen
			break;
	}
}

void InterLockedAddCellVelocityY(uint cellIndex, float velocity)
{
	[allow_uav_condition]
	while (true)
	{
		uint oldVelocityU32 = gWaterSimCellBuffer[cellIndex].mVelocityYU32;
		float oldVelocity = asfloat(oldVelocityU32);
		if (abs(oldVelocity + velocity) > WATERSIM_VELOCITY_MAX)
			break;
		uint originalVelocityU32;
		InterlockedCompareExchange(gWaterSimCellBuffer[cellIndex].mVelocityYU32, oldVelocityU32, asuint(oldVelocity + velocity), originalVelocityU32);
		if (originalVelocityU32 == oldVelocityU32) // chosen
			break;
	}
}

void InterLockedAddCellVelocityZ(uint cellIndex, float velocity)
{
	[allow_uav_condition]
	while (true)
	{
		uint oldVelocityU32 = gWaterSimCellBuffer[cellIndex].mVelocityZU32;
		float oldVelocity = asfloat(oldVelocityU32);
		if (abs(oldVelocity + velocity) > WATERSIM_VELOCITY_MAX)
			break;
		uint originalVelocityU32;
		InterlockedCompareExchange(gWaterSimCellBuffer[cellIndex].mVelocityZU32, oldVelocityU32, asuint(oldVelocity + velocity), originalVelocityU32);
		if (originalVelocityU32 == oldVelocityU32) // chosen
			break;
	}
}

void InterLockedAddCellMass(uint cellIndex, float mass)
{
	while (true)
	{
		uint oldMassU32 = gWaterSimCellBuffer[cellIndex].mMassU32;
		float oldMass = asfloat(oldMassU32);
		uint originalMassU32;
		InterlockedCompareExchange(gWaterSimCellBuffer[cellIndex].mMassU32, oldMassU32, asuint(oldMass + mass), originalMassU32);
		if (originalMassU32 == oldMassU32) // chosen
			break;
	}
}

#endif // WATERSIM_CELL_RESOURCES_UTIL_H
