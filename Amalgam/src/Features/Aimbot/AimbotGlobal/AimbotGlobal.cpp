#include "AimbotGlobal.h"

#include "../../Players/PlayerUtils.h"
#include "../../Misc/NamedPipe/NamedPipe.h"
#include "../../TickHandler/TickHandler.h"

void CAimbotGlobal::SortTargets(std::vector<Target_t>* targets, int iMethod)
{	// Sort by preference
	std::sort(targets->begin(), targets->end(), [&](const Target_t& a, const Target_t& b) -> bool
		{
			switch (iMethod)
			{
			case Vars::Aimbot::General::TargetSelectionEnum::FOV: return a.m_flFOVTo < b.m_flFOVTo;
			case Vars::Aimbot::General::TargetSelectionEnum::Distance: return a.m_flDistTo < b.m_flDistTo;
			default: return false;
			}
		});
}

void CAimbotGlobal::SortPriority(std::vector<Target_t>* targets)
{	// Sort by priority
	std::sort(targets->begin(), targets->end(), [&](const Target_t& a, const Target_t& b) -> bool
		{
			return a.m_nPriority > b.m_nPriority;
		});
}

// this won't prevent shooting bones outside of fov
bool CAimbotGlobal::PlayerBoneInFOV(CTFPlayer* pTarget, Vec3 vLocalPos, Vec3 vLocalAngles, float& flFOVTo, Vec3& vPos, Vec3& vAngleTo, int iHitboxes)
{
	// Early out if FOV is set to 360 (any target within this range will be valid)
	if (Vars::Aimbot::General::AimFOV.Value >= 360.f)
	{
		vPos = pTarget->GetCenter();
		vAngleTo = Math::CalcAngle(vLocalPos, vPos);
		flFOVTo = 0.f;
		return true;
	}

	float flMinFOV = 180.f;
	uint32_t uModelHash = H::Entities.GetModel(pTarget->entindex());
	
	// Pre-check if hitboxes
	if (!(iHitboxes & (Vars::Aimbot::Hitscan::HitboxesEnum::Head | 
	                    Vars::Aimbot::Hitscan::HitboxesEnum::Body | 
	                    Vars::Aimbot::Hitscan::HitboxesEnum::Pelvis | 
	                    Vars::Aimbot::Hitscan::HitboxesEnum::Arms | 
	                    Vars::Aimbot::Hitscan::HitboxesEnum::Legs)))
	{
		return false;
	}

	// Prioritize head hitbox check first to potentially exit early
	if (iHitboxes & Vars::Aimbot::Hitscan::HitboxesEnum::Head)
	{
		int nHeadHitbox = (uModelHash == FNV1A::Hash32Const("models/vsh/player/saxton_hale.mdl")) ? 
		                  HITBOX_SAXTON_HEAD : HITBOX_HEAD;
		
		Vec3 vCurPos = pTarget->GetHitboxCenter(nHeadHitbox);
		Vec3 vCurAngleTo = Math::CalcAngle(vLocalPos, vCurPos);
		float flCurFOVTo = Math::CalcFov(vLocalAngles, vCurAngleTo);

		if (flCurFOVTo < Vars::Aimbot::General::AimFOV.Value)
		{
			vPos = vCurPos;
			vAngleTo = vCurAngleTo;
			flFOVTo = flCurFOVTo;
			return true;
		}
		
		if (flCurFOVTo < flMinFOV)
		{
			vPos = vCurPos;
			vAngleTo = vCurAngleTo;
			flFOVTo = flMinFOV = flCurFOVTo;
		}
	}

	// Then check body hitboxes as they're commonly targeted
	if (iHitboxes & Vars::Aimbot::Hitscan::HitboxesEnum::Body)
	{
		const int bodyHitboxes[] = {
			(uModelHash == FNV1A::Hash32Const("models/vsh/player/saxton_hale.mdl")) ? 
			HITBOX_SAXTON_BODY : HITBOX_BODY,
			(uModelHash == FNV1A::Hash32Const("models/vsh/player/saxton_hale.mdl")) ? 
			HITBOX_SAXTON_CHEST : HITBOX_CHEST
		};
		
		for (int nHitbox : bodyHitboxes)
		{
			Vec3 vCurPos = pTarget->GetHitboxCenter(nHitbox);
			Vec3 vCurAngleTo = Math::CalcAngle(vLocalPos, vCurPos);
			float flCurFOVTo = Math::CalcFov(vLocalAngles, vCurAngleTo);

			if (flCurFOVTo < Vars::Aimbot::General::AimFOV.Value)
			{
				vPos = vCurPos;
				vAngleTo = vCurAngleTo;
				flFOVTo = flCurFOVTo;
				return true;
			}
			
			if (flCurFOVTo < flMinFOV)
			{
				vPos = vCurPos;
				vAngleTo = vCurAngleTo;
				flFOVTo = flMinFOV = flCurFOVTo;
			}
		}
	}
	
	// Then check remaining hitboxes
	for (int nHitbox = 0; nHitbox < pTarget->GetNumOfHitboxes(); nHitbox++)
	{
		// Skip hitboxes we've already checked
		if ((uModelHash == FNV1A::Hash32Const("models/vsh/player/saxton_hale.mdl") && 
		    (nHitbox == HITBOX_SAXTON_HEAD || nHitbox == HITBOX_SAXTON_BODY || nHitbox == HITBOX_SAXTON_CHEST)) ||
		    (nHitbox == HITBOX_HEAD || nHitbox == HITBOX_BODY || nHitbox == HITBOX_CHEST))
		{
			continue;
		}
		
		if (!IsHitboxValid(uModelHash, nHitbox, iHitboxes))
			continue;

		Vec3 vCurPos = pTarget->GetHitboxCenter(nHitbox);
		Vec3 vCurAngleTo = Math::CalcAngle(vLocalPos, vCurPos);
		float flCurFOVTo = Math::CalcFov(vLocalAngles, vCurAngleTo);

		if (flCurFOVTo < Vars::Aimbot::General::AimFOV.Value)
		{
			vPos = vCurPos;
			vAngleTo = vCurAngleTo;
			flFOVTo = flCurFOVTo;
			return true;
		}
		
		if (flCurFOVTo < flMinFOV)
		{
			vPos = vCurPos;
			vAngleTo = vCurAngleTo;
			flFOVTo = flMinFOV = flCurFOVTo;
		}
	}

	return flMinFOV < Vars::Aimbot::General::AimFOV.Value;
}

bool CAimbotGlobal::PlayerPosInFOV( CTFPlayer* pTarget, Vec3 vLocalPos, Vec3 vLocalAngles, float& flFOVTo, Vec3& vPos, Vec3& vAngleTo )
{
	vPos = pTarget->GetCenter();
	vAngleTo = Math::CalcAngle(vLocalPos, vPos);
	flFOVTo = Math::CalcFov(vLocalAngles, vAngleTo);

	return flFOVTo < Vars::Aimbot::General::AimFOV.Value || Vars::Aimbot::General::AimFOV.Value >= 360.f;
}

bool CAimbotGlobal::IsHitboxValid(uint32_t uHash, int nHitbox, int iHitboxes)
{
	switch (uHash)
	{
	case FNV1A::Hash32Const("models/vsh/player/saxton_hale.mdl"):
	{
		switch (nHitbox)
		{
		case -1: return true;
		case HITBOX_SAXTON_HEAD: return iHitboxes & Vars::Aimbot::Hitscan::HitboxesEnum::Head;
		case HITBOX_SAXTON_BODY:
		case HITBOX_SAXTON_THORAX:
		case HITBOX_SAXTON_CHEST:
		case HITBOX_SAXTON_UPPER_CHEST:
		case HITBOX_SAXTON_NECK: return iHitboxes & Vars::Aimbot::Hitscan::HitboxesEnum::Body;
		case HITBOX_SAXTON_PELVIS: return iHitboxes & Vars::Aimbot::Hitscan::HitboxesEnum::Pelvis;
		case HITBOX_SAXTON_LEFT_UPPER_ARM:
		case HITBOX_SAXTON_LEFT_FOREARM:
		case HITBOX_SAXTON_LEFT_HAND:
		case HITBOX_SAXTON_RIGHT_UPPER_ARM:
		case HITBOX_SAXTON_RIGHT_FOREARM:
		case HITBOX_SAXTON_RIGHT_HAND: return iHitboxes & Vars::Aimbot::Hitscan::HitboxesEnum::Arms;
		case HITBOX_SAXTON_LEFT_THIGH:
		case HITBOX_SAXTON_LEFT_CALF:
		case HITBOX_SAXTON_LEFT_FOOT:
		case HITBOX_SAXTON_RIGHT_THIGH:
		case HITBOX_SAXTON_RIGHT_CALF:
		case HITBOX_SAXTON_RIGHT_FOOT: return iHitboxes & Vars::Aimbot::Hitscan::HitboxesEnum::Legs;
		}
	}
	default:
	{
		switch (nHitbox)
		{
		case -1: return true;
		case HITBOX_HEAD: return iHitboxes & Vars::Aimbot::Hitscan::HitboxesEnum::Head;
		case HITBOX_BODY:
		case HITBOX_THORAX:
		case HITBOX_CHEST:
		case HITBOX_UPPER_CHEST: return iHitboxes & Vars::Aimbot::Hitscan::HitboxesEnum::Body;
		case HITBOX_PELVIS: return iHitboxes & Vars::Aimbot::Hitscan::HitboxesEnum::Pelvis;
		case HITBOX_LEFT_UPPER_ARM:
		case HITBOX_LEFT_FOREARM:
		case HITBOX_LEFT_HAND:
		case HITBOX_RIGHT_UPPER_ARM:
		case HITBOX_RIGHT_FOREARM:
		case HITBOX_RIGHT_HAND: return iHitboxes & Vars::Aimbot::Hitscan::HitboxesEnum::Arms;
		case HITBOX_LEFT_THIGH:
		case HITBOX_LEFT_CALF:
		case HITBOX_LEFT_FOOT:
		case HITBOX_RIGHT_THIGH:
		case HITBOX_RIGHT_CALF:
		case HITBOX_RIGHT_FOOT: return iHitboxes & Vars::Aimbot::Hitscan::HitboxesEnum::Legs;
		}
	}
	}

	return false;
}

bool CAimbotGlobal::ShouldIgnore(CBaseEntity* pEntity, CTFPlayer* pLocal, CTFWeaponBase* pWeapon, bool bIgnoreDormant)
{
	if (bIgnoreDormant && pEntity->IsDormant())
		return true;

	if (auto pGameRules = I::TFGameRules())
	{
		if (pGameRules->m_bTruceActive() && pLocal->m_iTeamNum() != pEntity->m_iTeamNum())
			return true;
	}

	switch (pEntity->GetClassID())
	{
	case ETFClassID::CTFPlayer:
	{
		auto pPlayer = pEntity->As<CTFPlayer>();
		if (pPlayer == pLocal || !pPlayer->IsAlive() || pPlayer->IsAGhost())
			return true;

		if (pLocal->m_iTeamNum() == pEntity->m_iTeamNum())
			return false;

		// pipe local playa
		PlayerInfo_t pi{};
		if (I::EngineClient->GetPlayerInfo(pPlayer->entindex(), &pi) && F::NamedPipe::IsLocalBot(pi.friendsID))
			return true;

		if (F::PlayerUtils.IsIgnored(pPlayer->entindex()))
			return true;

		if (Vars::Aimbot::General::Ignore.Value & Vars::Aimbot::General::IgnoreEnum::Friends && H::Entities.IsFriend(pPlayer->entindex())
			|| Vars::Aimbot::General::Ignore.Value & Vars::Aimbot::General::IgnoreEnum::Party && H::Entities.InParty(pPlayer->entindex())
			|| Vars::Aimbot::General::Ignore.Value & Vars::Aimbot::General::IgnoreEnum::Invulnerable && pPlayer->IsInvulnerable() && SDK::AttribHookValue(0, "crit_forces_victim_to_laugh", pWeapon) <= 0
			|| Vars::Aimbot::General::Ignore.Value & Vars::Aimbot::General::IgnoreEnum::Cloaked && pPlayer->IsInvisible() && pPlayer->GetInvisPercentage() >= Vars::Aimbot::General::IgnoreCloakPercentage.Value
			|| Vars::Aimbot::General::Ignore.Value & Vars::Aimbot::General::IgnoreEnum::DeadRinger && pPlayer->m_bFeignDeathReady()
			|| Vars::Aimbot::General::Ignore.Value & Vars::Aimbot::General::IgnoreEnum::Taunting && pPlayer->IsTaunting()
			|| Vars::Aimbot::General::Ignore.Value & Vars::Aimbot::General::IgnoreEnum::Disguised && pPlayer->InCond(TF_COND_DISGUISED))
			return true;
		if (Vars::Aimbot::General::Ignore.Value & Vars::Aimbot::General::IgnoreEnum::Vaccinator)
		{
			switch (G::PrimaryWeaponType)
			{
			case EWeaponType::HITSCAN:
				if (pPlayer->InCond(TF_COND_MEDIGUN_UBER_BULLET_RESIST) && SDK::AttribHookValue(0, "mod_pierce_resists_absorbs", pWeapon) != 0)
					return true;
				break;
			case EWeaponType::PROJECTILE:
				switch (pWeapon->GetWeaponID())
				{
				case TF_WEAPON_FLAMETHROWER:
				case TF_WEAPON_FLAREGUN:
					if (pPlayer->InCond(TF_COND_MEDIGUN_UBER_FIRE_RESIST))
						return true;
					break;
				case TF_WEAPON_COMPOUND_BOW:
					if (pPlayer->InCond(TF_COND_MEDIGUN_UBER_BULLET_RESIST))
						return true;
					break;
				default:
					if (pPlayer->InCond(TF_COND_MEDIGUN_UBER_BLAST_RESIST))
						return true;
				}
			}
		}

		return false;
	}
	case ETFClassID::CObjectSentrygun:
	case ETFClassID::CObjectDispenser:
	case ETFClassID::CObjectTeleporter:
	{
		auto pBuilding = pEntity->As<CBaseObject>();

		if (!(Vars::Aimbot::General::Target.Value & Vars::Aimbot::General::TargetEnum::Sentry) && pBuilding->IsSentrygun()
			|| !(Vars::Aimbot::General::Target.Value & Vars::Aimbot::General::TargetEnum::Dispenser) && pBuilding->IsDispenser()
			|| !(Vars::Aimbot::General::Target.Value & Vars::Aimbot::General::TargetEnum::Teleporter) && pBuilding->IsTeleporter())
			return true;

		if (pLocal->m_iTeamNum() == pEntity->m_iTeamNum())
			return false;

		auto pOwner = pBuilding->m_hBuilder().Get();
		if (pOwner)
		{
			if (F::PlayerUtils.IsIgnored(pOwner->entindex()))
				return true;

			if (Vars::Aimbot::General::Ignore.Value & Vars::Aimbot::General::IgnoreEnum::Friends && H::Entities.IsFriend(pOwner->entindex())
				|| Vars::Aimbot::General::Ignore.Value & Vars::Aimbot::General::IgnoreEnum::Party && H::Entities.InParty(pOwner->entindex()))
				return true;
		}

		return false;
	}
	case ETFClassID::CTFGrenadePipebombProjectile:
	{
		auto pProjectile = pEntity->As<CTFGrenadePipebombProjectile>();

		if (!(Vars::Aimbot::General::Target.Value & Vars::Aimbot::General::TargetEnum::Stickies))
			return true;

		if (pLocal->m_iTeamNum() == pEntity->m_iTeamNum())
			return true;

		auto pOwner = pProjectile->m_hThrower().Get();
		if (pOwner && F::PlayerUtils.IsIgnored(pOwner->entindex()))
			return true;

		if (pProjectile->m_iType() != TF_GL_MODE_REMOTE_DETONATE || !pProjectile->m_bTouched())
			return true;

		return false;
	}
	case ETFClassID::CEyeballBoss:
	case ETFClassID::CHeadlessHatman:
	case ETFClassID::CMerasmus:
	case ETFClassID::CTFBaseBoss:
	{
		if (!(Vars::Aimbot::General::Target.Value & Vars::Aimbot::General::TargetEnum::NPCs))
			return true;

		if (pEntity->m_iTeamNum() != TF_TEAM_HALLOWEEN)
			return true;

		return false;
	}
	case ETFClassID::CTFTankBoss:
	case ETFClassID::CZombie:
	{

		if (!(Vars::Aimbot::General::Target.Value & Vars::Aimbot::General::TargetEnum::NPCs))
			return true;

		if (pLocal->m_iTeamNum() == pEntity->m_iTeamNum())
			return true;

		return false;
	}
	case ETFClassID::CTFPumpkinBomb:
	case ETFClassID::CTFGenericBomb:
	{
		if (!(Vars::Aimbot::General::Target.Value & Vars::Aimbot::General::TargetEnum::Bombs))
			return true;

		return false;
	}
	}

	return true;
}

int CAimbotGlobal::GetPriority(int targetIdx)
{
	return F::PlayerUtils.GetPriority(targetIdx);
}

// will not predict for projectile weapons
bool CAimbotGlobal::ValidBomb(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CBaseEntity* pBomb)
{
	if (G::PrimaryWeaponType == EWeaponType::PROJECTILE)
		return false;

	Vec3 vOrigin = pBomb->m_vecOrigin();

	CBaseEntity* pEntity;
	for (CEntitySphereQuery sphere(vOrigin, 300.f);
		(pEntity = sphere.GetCurrentEntity()) != nullptr;
		sphere.NextEntity())
	{
		if (!pEntity || pEntity == pLocal || pEntity->IsPlayer() && (!pEntity->As<CTFPlayer>()->IsAlive() || pEntity->As<CTFPlayer>()->IsAGhost()) || pEntity->m_iTeamNum() == pLocal->m_iTeamNum())
			continue;

		Vec3 vPos = {}; reinterpret_cast<CCollisionProperty*>(pEntity->GetCollideable())->CalcNearestPoint(vOrigin, &vPos);
		if (vOrigin.DistTo(vPos) > 300.f)
			continue;

		bool isPlayer = pEntity->IsPlayer() && Vars::Aimbot::General::Target.Value & Vars::Aimbot::General::TargetEnum::Players;
		bool isSentry = pEntity->IsSentrygun() && Vars::Aimbot::General::Target.Value & Vars::Aimbot::General::TargetEnum::Sentry;
		bool isDispenser = pEntity->IsDispenser() && Vars::Aimbot::General::Target.Value & Vars::Aimbot::General::TargetEnum::Dispenser;
		bool isTeleporter = pEntity->IsTeleporter() && Vars::Aimbot::General::Target.Value & Vars::Aimbot::General::TargetEnum::Teleporter;
		bool isNPC = pEntity->IsNPC() && Vars::Aimbot::General::Target.Value & Vars::Aimbot::General::TargetEnum::NPCs;
		if (isPlayer || isSentry || isDispenser || isTeleporter || isNPC)
		{
			if (isPlayer && ShouldIgnore(pEntity->As<CTFPlayer>(), pLocal, pWeapon))
				continue;

			if (!SDK::VisPosProjectile(pBomb, pEntity, vOrigin, isPlayer ? pEntity->m_vecOrigin() + pEntity->As<CTFPlayer>()->GetViewOffset() : pEntity->GetCenter(), MASK_SHOT))
				continue;

			return true;
		}
	}

	return false;
}

bool CAimbotGlobal::IsTargetStillValid(const Target_t& target, float flMaxValidityTime)
{
	if (!target.m_pEntity)
		return false;
	float flCurrentTime = I::GlobalVars->curtime;
	float flValidityTime = flMaxValidityTime > 0.0f ? flMaxValidityTime : Vars::Aimbot::General::TargetValidityTime.Value;
	if (flCurrentTime - target.m_flLastValidTime > flValidityTime)
		return false;
		
	switch (target.m_iTargetType)
	{
	case TargetEnum::Player:
	{
		auto pPlayer = target.m_pEntity->As<CTFPlayer>();
		if (!pPlayer || !pPlayer->IsAlive() || pPlayer->IsAGhost())
			return false;
		break;
	}
	case TargetEnum::Sentry:
	case TargetEnum::Dispenser:
	case TargetEnum::Teleporter:
	{
		auto pBuilding = target.m_pEntity->As<CBaseObject>();
		if (!pBuilding || pBuilding->m_bPlacing() || pBuilding->m_bCarried())
			return false;
		break;
	}
	case TargetEnum::Sticky:
	{
		auto pSticky = target.m_pEntity->As<CTFGrenadePipebombProjectile>();
		if (!pSticky || pSticky->m_iType() != TF_GL_MODE_REMOTE_DETONATE || !pSticky->m_bTouched())
			return false;
		break;
	}
	case TargetEnum::NPC:
	{
		// NPCs only need to be alive/active - no specific checks needed beyond the base pointer check
		break;
	}
	case TargetEnum::Bomb:
	{
		// For bombs, just ensure the entity still exists (handled by null check above)
		break;
	}
	default:
		break;
	}

	return true;
}