#pragma once
#include "../../SDK/SDK.h"

class CIncomingSequence
{
public:
	int m_nInReliableState;
	int m_nSequenceNr;
	float m_flTime;

	CIncomingSequence(int iState, int iSequence, float flTime)
	{
		m_nInReliableState = iState;
		m_nSequenceNr = iSequence;
		m_flTime = flTime;
	}
};

struct BoneMatrix
{
	matrix3x4 m_aBones[128];
};

struct HitboxInfo
{
	int m_iBone = -1, m_nHitbox = -1;
	Vec3 m_vCenter = {};
	Vec3 m_iMin = {}, m_iMax = {};
};

struct TickRecord
{
	float m_flSimTime = 0.f;
	BoneMatrix m_BoneMatrix = {};
	std::vector<HitboxInfo> m_vHitboxInfos{};

	Vec3 m_vOrigin = {};
	Vec3 m_vMins = {};
	Vec3 m_vMaxs = {};
	bool m_bOnShot = false;
	Vec3 m_vBreak = {};
	bool m_bInvalid = false;
};

class CBacktrack
{
	void SendLerp();
	void UpdateDatagram();
	void MakeRecords();
	void CleanRecords();

	std::unordered_map<CBaseEntity*, std::deque<TickRecord>> m_mRecords;
	std::unordered_map<int, bool> m_mDidShoot;

	std::deque<CIncomingSequence> m_dSequences;
	int m_iLastInSequence = 0;
	int m_nOldInSequenceNr = 0;
	int m_nOldInReliableState = 0;
	int m_nLastInSequenceNr = 0;
	int m_nOldTickBase = 0;

	struct CrosshairRecordInfo_t
	{
		float flMinDist{ -1.f };
		float flFov{ -1.f };
		bool bInsideThisRecord{ false };
	};
	std::optional<TickRecord> GetHitRecord(CBaseEntity* pEntity, CTFWeaponBase* pWeapon, CUserCmd* pCmd, CrosshairRecordInfo_t& InfoOut, const Vec3 vAngles, const Vec3 vPos);

public:
	float GetLerp();
	float GetFake();
	float GetReal(int iFlow = MAX_FLOWS, bool bNoFake = true);
	float GetFakeInterp();
	int GetAnticipatedChoke(int iMethod = Vars::Aimbot::General::AimType.Value);

	std::deque<TickRecord>* GetRecords(CBaseEntity* pEntity);
	std::deque<TickRecord> GetValidRecords(std::deque<TickRecord>* pRecords, CTFPlayer* pLocal = nullptr, bool bDistance = false);

	void Store();
	void Run(CUserCmd* pCmd);
	void Reset();
	void SetLerp(IGameEvent* pEvent);
	void ResolverUpdate(CBaseEntity* pEntity);
	void ReportShot(int iIndex);
	void AdjustPing(CNetChannel* netChannel);
	void RestorePing(CNetChannel* netChannel);
	void BacktrackToCrosshair(CUserCmd* pCmd);

	int m_iTickCount = 0;
	float m_flMaxUnlag = 1.f;

	float m_flFakeLatency = 0.f;
	float m_flFakeInterp = 0.015f;
	float m_flWishInterp = -1.f;
};

ADD_FEATURE(CBacktrack, Backtrack)