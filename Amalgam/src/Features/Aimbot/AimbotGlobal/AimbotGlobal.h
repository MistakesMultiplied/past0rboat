#pragma once
#include "../../../SDK/SDK.h"
#include "../../Backtrack/Backtrack.h"

Enum(Target, Unknown, Player, Sentry, Dispenser, Teleporter, Sticky, NPC, Bomb)

struct Target_t
{
	CBaseEntity* m_pEntity = nullptr;
	Vec3 m_vPos = {};
	Vec3 m_vAngleTo = {};
	float m_flFOVTo = std::numeric_limits<float>::max();
	float m_flDistTo = std::numeric_limits<float>::max();
	float m_flLastValidTime = 0.0f;
	

	int m_iTargetType = TargetEnum::Unknown;
	int m_nPriority = 0;
	int m_nAimedHitbox = -1;
	

	TickRecord m_tRecord = {};
	bool m_bBacktrack = false;
	Target_t(CBaseEntity* pEntity = nullptr, int iTargetType = TargetEnum::Unknown, 
	        Vec3 vPos = {}, Vec3 vAngleTo = {}, float flFOVTo = std::numeric_limits<float>::max(), 
	        float flDistTo = std::numeric_limits<float>::max(), int nPriority = 0, int nAimedHitbox = -1)
		: m_pEntity(pEntity), m_vPos(vPos), m_vAngleTo(vAngleTo), m_flFOVTo(flFOVTo), m_flDistTo(flDistTo),
		  m_flLastValidTime(I::GlobalVars->curtime), m_iTargetType(iTargetType), m_nPriority(nPriority), 
		  m_nAimedHitbox(nAimedHitbox), m_tRecord(), m_bBacktrack(false)
	{ }
	
	// Comparison operators for sorting (inline for better optimization)
	inline bool operator<(const Target_t& other) const { return m_flFOVTo < other.m_flFOVTo; }
	inline bool operator>(const Target_t& other) const { return m_nPriority > other.m_nPriority; }
};

class CAimbotGlobal
{
public:
	void SortTargets(std::vector<Target_t>&, int iMethod);
	void SortPriority(std::vector<Target_t>&);

	bool PlayerBoneInFOV(CTFPlayer* pTarget, Vec3 vLocalPos, Vec3 vLocalAngles, float& flFOVTo, Vec3& vPos, Vec3& vAngleTo, int iHitboxes = Vars::Aimbot::Hitscan::HitboxesEnum::Head | Vars::Aimbot::Hitscan::HitboxesEnum::Body | Vars::Aimbot::Hitscan::HitboxesEnum::Pelvis | Vars::Aimbot::Hitscan::HitboxesEnum::Arms | Vars::Aimbot::Hitscan::HitboxesEnum::Legs);
	bool PlayerPosInFOV(CTFPlayer* pTarget, Vec3 vLocalPos, Vec3 vLocalAngles, float& flFOVTo, Vec3& vPos, Vec3& vAngleTo);

	bool IsHitboxValid(uint32_t uHash, int nHitbox, int iHitboxes = Vars::Aimbot::Hitscan::HitboxesEnum::Head | Vars::Aimbot::Hitscan::HitboxesEnum::Body | Vars::Aimbot::Hitscan::HitboxesEnum::Pelvis | Vars::Aimbot::Hitscan::HitboxesEnum::Arms | Vars::Aimbot::Hitscan::HitboxesEnum::Legs);

	bool ShouldIgnore(CBaseEntity* pTarget, CTFPlayer* pLocal, CTFWeaponBase* pWeapon, bool bIgnoreDormant = true);
	int GetPriority(int targetIdx);

	bool IsTargetStillValid(const Target_t& target, float flMaxValidityTime = 0.2f);

	bool ValidBomb(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CBaseEntity* pBomb);
};

ADD_FEATURE(CAimbotGlobal, AimbotGlobal)