#pragma once
#include "../../../SDK/SDK.h"

class CAntiAim
{
private:
	void FakeShotAngles(CTFPlayer* pLocal, CUserCmd* pCmd);

	float EdgeDistance(CTFPlayer* pEntity, float flEdgeRayYaw, float flOffset);
	void RunOverlapping(CTFPlayer* pEntity, CUserCmd* pCmd, float& flRealYaw, bool bFake, float flEpsilon = 45.f);
	float GetYawOffset(CTFPlayer* pEntity, bool bFake);
	float GetBaseYaw(CTFPlayer* pLocal, CUserCmd* pCmd, bool bFake);
	float GetYaw(CTFPlayer* pLocal, CUserCmd* pCmd, bool bFake);

	float GetPitch(float flCurPitch);
	void MinWalk(CTFPlayer* pLocal, CUserCmd* pCmd);
	
	float GetBigRandomYaw();
	float GetBigRandomPitch();

	float GetFlipPitch();
	float m_flLastFlipTime = 0.0f;
	bool m_bFlipState = false; // false = up, true = down
	
	float m_flBigRandomYawSpeed = 0.0f;
	float m_flBigRandomPitchSpeed = 0.0f;
	float m_flNextBigRandomUpdate = 0.0f;

public:
	// Constructor
	CAntiAim();
	
	bool AntiAimOn();
	bool YawOn();
	bool ShouldRun(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CUserCmd* pCmd);

	int GetEdge(CTFPlayer* pEntity, float flEdgeOrigYaw, bool bUpPitch);
	void Run(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CUserCmd* pCmd, bool bSendPacket);

	inline int AntiAimTicks() { return 3; }

	Vec2 vFakeAngles = {};
	Vec2 vRealAngles = {};
	std::vector<std::pair<Vec3, Vec3>> vEdgeTrace = {};
};

ADD_FEATURE(CAntiAim, AntiAim)