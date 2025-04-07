#include "Aimbot.h"

#include "AimbotHitscan/AimbotHitscan.h"
#include "AimbotProjectile/AimbotProjectile.h"
#include "AimbotMelee/AimbotMelee.h"
#include "AutoDetonate/AutoDetonate.h"
#include "AutoAirblast/AutoAirblast.h"
#include "AutoHeal/AutoHeal.h"
#include "AutoRocketJump/AutoRocketJump.h"
#include "../Misc/Misc.h"
#include "../Visuals/Visuals.h"
#include "../NavBot/NavEngine/NavEngine.h"

bool CAimbot::ShouldRun(CTFPlayer* pLocal, CTFWeaponBase* pWeapon)
{
	if (!pLocal || !pWeapon)
		return false;
		
	if (!pLocal->IsAlive() || pLocal->IsAGhost())
		return false;
		
	bool bInvalidConditions = pLocal->IsTaunting() || 
	                          pLocal->m_bFeignDeathReady() || 
	                          pLocal->InCond(TF_COND_PHASE) || 
	                          pLocal->InCond(TF_COND_STEALTHED) || 
	                          pLocal->InCond(TF_COND_HALLOWEEN_KART);
	if (bInvalidConditions)
		return false;
		
	if (pLocal->InCond(TF_COND_STUNNED) && 
	    pLocal->m_iStunFlags() & (TF_STUN_CONTROLS | TF_STUN_LOSER_STATE))
		return false;
	
	if (SDK::AttribHookValue(1, "mult_dmg", pWeapon) == 0)
		return false;


	return true;
}

void CAimbot::RunAimbot(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CUserCmd* pCmd, bool bSecondaryType)
{
	m_bRunningSecondary = bSecondaryType;
	EWeaponType eWeaponType = !m_bRunningSecondary ? G::PrimaryWeaponType : G::SecondaryWeaponType;

	bool bOriginal;
	if (m_bRunningSecondary)
		bOriginal = G::CanPrimaryAttack, G::CanPrimaryAttack = G::CanSecondaryAttack;

	switch (eWeaponType)
	{
	case EWeaponType::HITSCAN: F::AimbotHitscan.Run(pLocal, pWeapon, pCmd); break;
	case EWeaponType::PROJECTILE: F::AimbotProjectile.Run(pLocal, pWeapon, pCmd); break;
	case EWeaponType::MELEE: F::AimbotMelee.Run(pLocal, pWeapon, pCmd); break;
	}

	if (m_bRunningSecondary)
		G::CanPrimaryAttack = bOriginal;
}

void CAimbot::RunMain(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CUserCmd* pCmd)
{
	if (F::AimbotProjectile.m_iLastTickCancel)
	{
		pCmd->weaponselect = F::AimbotProjectile.m_iLastTickCancel;
		F::AimbotProjectile.m_iLastTickCancel = 0;
	}

	m_bRan = false;
	if (G::Target.first != 0)
	{
		bool bTargetValid = false;
		auto pEntity = I::ClientEntityList->GetClientEntity(G::Target.first);
		if (pEntity && !pEntity->IsDormant())
		{
			Target_t tempTarget(reinterpret_cast<CBaseEntity*>(pEntity), TargetEnum::Unknown, {}, {}, 0.0f, 0.0f);
			tempTarget.m_flLastValidTime = I::GlobalVars->curtime - (I::GlobalVars->tickcount - G::Target.second) * TICK_INTERVAL;
			bTargetValid = F::AimbotGlobal.IsTargetStillValid(tempTarget);
		}
		
		if (!bTargetValid || abs(G::Target.second - I::GlobalVars->tickcount) > 32)
			G::Target = { 0, 0 };
	}
	
	if (abs(G::AimPosition.second - I::GlobalVars->tickcount) > 32)
		G::AimPosition = { {}, 0 };

	if (pCmd->weaponselect)
		return;

	if (!ShouldRun(pLocal, pWeapon))
	{
		F::AutoRocketJump.Run(pLocal, pWeapon, pCmd);
		return;
	}

	F::AutoRocketJump.Run(pLocal, pWeapon, pCmd);
	F::AutoDetonate.Run(pLocal, pWeapon, pCmd);
	F::AutoAirblast.Run(pLocal, pWeapon, pCmd);
	F::AutoHeal.Run(pLocal, pWeapon, pCmd);

	RunAimbot(pLocal, pWeapon, pCmd);
	RunAimbot(pLocal, pWeapon, pCmd, true);
}

void CAimbot::Run(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CUserCmd* pCmd)
{
	RunMain(pLocal, pWeapon, pCmd);

	G::Attacking = SDK::IsAttacking(pLocal, pWeapon, pCmd, true);
}