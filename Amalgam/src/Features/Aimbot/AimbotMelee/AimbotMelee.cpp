#include "AimbotMelee.h"

#include "../Aimbot.h"
#include "../../Simulation/MovementSimulation/MovementSimulation.h"
#include "../../TickHandler/TickHandler.h"
#include "../../Visuals/Visuals.h"

std::vector<Target_t> CAimbotMelee::GetTargets(CTFPlayer* pLocal, CTFWeaponBase* pWeapon)
{
	std::vector<Target_t> vTargets;

	const Vec3 vLocalPos = F::Ticks.GetShootPos();
	const Vec3 vLocalAngles = I::EngineClient->GetViewAngles();

	if (Vars::Aimbot::General::Target.Value & Vars::Aimbot::General::TargetEnum::Players)
	{
		bool bDisciplinary = Vars::Aimbot::Melee::WhipTeam.Value && SDK::AttribHookValue(0, "speed_buff_ally", pWeapon) > 0;
		for (auto pEntity : H::Entities.GetGroup(bDisciplinary ? EGroupType::PLAYERS_ALL : EGroupType::PLAYERS_ENEMIES))
		{
			bool bTeammate = pEntity->m_iTeamNum() == pLocal->m_iTeamNum();
			if (F::AimbotGlobal.ShouldIgnore(pEntity, pLocal, pWeapon))
				continue;

			float flFOVTo; Vec3 vPos, vAngleTo;
			if (!F::AimbotGlobal.PlayerBoneInFOV(pEntity->As<CTFPlayer>(), vLocalPos, vLocalAngles, flFOVTo, vPos, vAngleTo))
				continue;

			float flDistTo = vLocalPos.DistTo(vPos);
			vTargets.emplace_back(pEntity, TargetEnum::Player, vPos, vAngleTo, flFOVTo, flDistTo, bTeammate ? 0 : F::AimbotGlobal.GetPriority(pEntity->entindex()));
		}
	}

	if (Vars::Aimbot::General::Target.Value)
	{
		bool bWrench = pWeapon->GetWeaponID() == TF_WEAPON_WRENCH;
		bool bDestroySapper = pWeapon->GetWeaponID() == TF_WEAPON_FIREAXE && SDK::AttribHookValue(0, "set_dmg_apply_to_sapper", pWeapon);

		for (auto pEntity : H::Entities.GetGroup(bWrench || bDestroySapper ? EGroupType::BUILDINGS_ALL : EGroupType::BUILDINGS_ENEMIES))
		{
			if (F::AimbotGlobal.ShouldIgnore(pEntity, pLocal, pWeapon))
				continue;

			if (pEntity->m_iTeamNum() == pLocal->m_iTeamNum() && (bWrench && !AimFriendlyBuilding(pEntity->As<CBaseObject>()) || bDestroySapper && !pEntity->As<CBaseObject>()->m_bHasSapper()))
				continue;

			Vec3 vPos = pEntity->GetCenter();
			Vec3 vAngleTo = Math::CalcAngle(vLocalPos, vPos);
			float flFOVTo = Math::CalcFov(vLocalAngles, vAngleTo);
			if (flFOVTo > Vars::Aimbot::General::AimFOV.Value)
				continue;

			float flDistTo = vLocalPos.DistTo(vPos);
			vTargets.emplace_back(pEntity, pEntity->IsSentrygun() ? TargetEnum::Sentry : pEntity->IsDispenser() ? TargetEnum::Dispenser : TargetEnum::Teleporter, vPos, vAngleTo, flFOVTo, flDistTo);
		}
	}

	if (Vars::Aimbot::General::Target.Value & Vars::Aimbot::General::TargetEnum::NPCs)
	{
		for (auto pEntity : H::Entities.GetGroup(EGroupType::WORLD_NPC))
		{
			if (F::AimbotGlobal.ShouldIgnore(pEntity, pLocal, pWeapon))
				continue;

			Vec3 vPos = pEntity->GetCenter();
			Vec3 vAngleTo = Math::CalcAngle(vLocalPos, vPos);
			float flFOVTo = Math::CalcFov(vLocalAngles, vAngleTo);
			if (flFOVTo > Vars::Aimbot::General::AimFOV.Value)
				continue;

			float flDistTo = vLocalPos.DistTo(vPos);
			vTargets.emplace_back(pEntity, TargetEnum::NPC, vPos, vAngleTo, flFOVTo, flDistTo);
		}
	}

	return vTargets;
}

bool CAimbotMelee::AimFriendlyBuilding(CBaseObject* pBuilding)
{
	if (!pBuilding->m_bMiniBuilding() && pBuilding->m_iUpgradeLevel() != 3 || pBuilding->m_iHealth() < pBuilding->m_iMaxHealth() || pBuilding->m_bHasSapper())
		return true;

	if (pBuilding->IsSentrygun())
	{
		int iShells, iMaxShells, iRockets, iMaxRockets; pBuilding->As<CObjectSentrygun>()->GetAmmoCount(iShells, iMaxShells, iRockets, iMaxRockets);
		if (iShells < iMaxShells || iRockets < iMaxRockets)
			return true;
	}

	return false;
}

std::vector<Target_t> CAimbotMelee::SortTargets(CTFPlayer* pLocal, CTFWeaponBase* pWeapon)
{
	auto vTargets = GetTargets(pLocal, pWeapon);
	F::AimbotGlobal.SortTargets(vTargets, Vars::Aimbot::General::TargetSelectionEnum::Distance);
	vTargets.resize(std::min(size_t(Vars::Aimbot::General::MaxTargets.Value), vTargets.size()));
	F::AimbotGlobal.SortPriority(vTargets);
	return vTargets;
}



int CAimbotMelee::GetSwingTime(CTFWeaponBase* pWeapon)
{
	if (pWeapon->GetWeaponID() == TF_WEAPON_KNIFE)
		return 0;
	return Vars::Aimbot::Melee::SwingTicks.Value;
}

void CAimbotMelee::SimulatePlayers(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, std::vector<Target_t> vTargets,
								   Vec3& vEyePos, std::unordered_map<int, std::deque<TickRecord>>& pRecordMap,
								   std::unordered_map<int, std::deque<Vec3>>& mPaths)
{
	// swing prediction / auto warp
	const int iSwingTicks = GetSwingTime(pWeapon);
	int iMax = (iDoubletapTicks && Vars::CL_Move::Doubletap::AntiWarp.Value && pLocal->m_hGroundEntity())
		? std::max(iSwingTicks - Vars::CL_Move::Doubletap::TickLimit.Value - 1, 0)
		: std::max(iSwingTicks, iDoubletapTicks);

	if ((Vars::Aimbot::Melee::SwingPrediction.Value || iDoubletapTicks) && pWeapon->m_flSmackTime() < 0.f && iMax)
	{
		PlayerStorage tStorage;
		std::unordered_map<int, PlayerStorage> mStorage;

		F::MoveSim.Initialize(pLocal, tStorage, false, iDoubletapTicks);
		for (auto& tTarget : vTargets)
			F::MoveSim.Initialize(tTarget.m_pEntity, mStorage[tTarget.m_pEntity->entindex()], false);

		for (int i = 0; i < iMax; i++) // intended for plocal to collide with targets
		{
			if (i < iMax)
			{
				if (pLocal->InCond(TF_COND_SHIELD_CHARGE) && iMax - i <= GetSwingTime(pWeapon)) // demo charge fix for swing pred
					tStorage.m_MoveData.m_flMaxSpeed = tStorage.m_MoveData.m_flClientMaxSpeed = SDK::MaxSpeed(pLocal, false, true);
				F::MoveSim.RunTick(tStorage);
			}
			if (i < iSwingTicks - iDoubletapTicks)
			{
				for (auto& tTarget : vTargets)
				{
					auto& tStorage = mStorage[tTarget.m_pEntity->entindex()];

					F::MoveSim.RunTick(tStorage);
					if (!tStorage.m_bFailed)
						pRecordMap[tTarget.m_pEntity->entindex()].emplace_front(
						!Vars::Aimbot::Melee::SwingPredictLag.Value || tStorage.m_bPredictNetworked ? tTarget.m_pEntity->m_flSimulationTime() + TICKS_TO_TIME(i + 1) : 0.f,
						BoneMatrix(),
						std::vector{ HitboxInfo() },
						Vars::Aimbot::Melee::SwingPredictLag.Value ? tStorage.m_vPredictedOrigin : tStorage.m_MoveData.m_vecAbsOrigin
						);
				}
			}
		}
		vEyePos = tStorage.m_MoveData.m_vecAbsOrigin + pLocal->m_vecViewOffset();

		if (Vars::Visuals::Simulation::SwingLines.Value && Vars::Visuals::Simulation::PlayerPath.Value)
		{
			const bool bAlwaysDraw = !Vars::Aimbot::General::AutoShoot.Value || Vars::Debug::Info.Value;
			if (!bAlwaysDraw)
			{
				mPaths[pLocal->entindex()] = tStorage.m_vPath;
				for (auto& tTarget : vTargets)
					mPaths[tTarget.m_pEntity->entindex()] = mStorage[tTarget.m_pEntity->entindex()].m_vPath;
			}
			else
			{
				G::LineStorage.clear();
				G::BoxStorage.clear();
				G::PathStorage.clear();
				if (Vars::Colors::PlayerPath.Value.a)
				{
					G::PathStorage.emplace_back(tStorage.m_vPath, I::GlobalVars->curtime + Vars::Visuals::Simulation::DrawDuration.Value, Vars::Colors::PlayerPath.Value, Vars::Visuals::Simulation::PlayerPath.Value);
					for (auto& tTarget : vTargets)
						G::PathStorage.emplace_back(mStorage[tTarget.m_pEntity->entindex()].m_vPath, I::GlobalVars->curtime + Vars::Visuals::Simulation::DrawDuration.Value, Vars::Colors::PlayerPath.Value, Vars::Visuals::Simulation::PlayerPath.Value);
				}
				if (Vars::Colors::PlayerPathClipped.Value.a)
				{
					G::PathStorage.emplace_back(tStorage.m_vPath, I::GlobalVars->curtime + Vars::Visuals::Simulation::DrawDuration.Value, Vars::Colors::PlayerPathClipped.Value, Vars::Visuals::Simulation::PlayerPath.Value, true);
					for (auto& tTarget : vTargets)
						G::PathStorage.emplace_back(mStorage[tTarget.m_pEntity->entindex()].m_vPath, I::GlobalVars->curtime + Vars::Visuals::Simulation::DrawDuration.Value, Vars::Colors::PlayerPathClipped.Value, Vars::Visuals::Simulation::PlayerPath.Value, true);
				}
			}
		}

		F::MoveSim.Restore(tStorage);
		for (auto& tTarget : vTargets)
			F::MoveSim.Restore(mStorage[tTarget.m_pEntity->entindex()]);
	}
}

bool CAimbotMelee::CanBackstab(CBaseEntity* pTarget, CTFPlayer* pLocal, Vec3 vEyeAngles)
{
	if (!pLocal || !pTarget)
		return false;

	if (Vars::Aimbot::Melee::IgnoreRazorback.Value)
	{
		CUtlVector<CBaseEntity*> itemList;
		int iBackstabShield = SDK::AttribHookValue(0, "set_blockbackstab_once", pTarget, &itemList);
		if (iBackstabShield && itemList.Count())
		{
			CBaseEntity* pEntity = itemList.Element(0);
			if (pEntity && pEntity->ShouldDraw())
				return false;
		}
	}

	Vec3 vToTarget = pTarget->GetAbsOrigin() - pLocal->m_vecOrigin();
	vToTarget.z = 0.f;
	const float flDist = vToTarget.Length();
	if (!flDist)
		return false;

	vToTarget.Normalize();
	float flTolerance = 0.0625f;
	float flExtra = 2.f * flTolerance / flDist; // account for origin tolerance

	float flPosVsTargetViewMinDot = 0.f + 0.0031f + flExtra;
	float flPosVsOwnerViewMinDot = 0.5f + flExtra;
	float flViewAnglesMinDot = -0.3f + 0.0031f; // 0.00306795676297 ?

	auto TestDots = [&](Vec3 vTargetAngles)
		{
			Vec3 vOwnerForward; Math::AngleVectors(vEyeAngles, &vOwnerForward);
			vOwnerForward.z = 0.f;
			vOwnerForward.Normalize();

			Vec3 vTargetForward; Math::AngleVectors(vTargetAngles, &vTargetForward);
			vTargetForward.z = 0.f;
			vTargetForward.Normalize();

			const float flPosVsTargetViewDot = vToTarget.Dot(vTargetForward); // Behind?
			const float flPosVsOwnerViewDot = vToTarget.Dot(vOwnerForward); // Facing?
			const float flViewAnglesDot = vTargetForward.Dot(vOwnerForward); // Facestab?

			return flPosVsTargetViewDot > flPosVsTargetViewMinDot && flPosVsOwnerViewDot > flPosVsOwnerViewMinDot && flViewAnglesDot > flViewAnglesMinDot;
		};

	Vec3 vTargetAngles = { 0.f, H::Entities.GetEyeAngles(pTarget->entindex()).y, 0.f };
	if (!Vars::Aimbot::Melee::BackstabAccountPing.Value)
	{
		if (!TestDots(vTargetAngles))
			return false;
	}
	else
	{
		if (Vars::Aimbot::Melee::BackstabDoubleTest.Value && !TestDots(vTargetAngles))
			return false;

		vTargetAngles.y += H::Entities.GetPingAngles(pTarget->entindex()).y;
		if (!TestDots(vTargetAngles))
			return false;
	}

	return true;
}

int CAimbotMelee::CanHit(Target_t& tTarget, CTFPlayer* pLocal, CTFWeaponBase* pWeapon, Vec3 vEyePos, std::deque<TickRecord>& vSimRecords)
{
	if (Vars::Aimbot::General::Ignore.Value & Vars::Aimbot::General::IgnoreEnum::Unsimulated && H::Entities.GetChoke(tTarget.m_pEntity->entindex()) > Vars::Aimbot::General::TickTolerance.Value)
		return false;

	float flHull = SDK::AttribHookValue(18, "melee_bounds_multiplier", pWeapon);
	float flRange = SDK::AttribHookValue(pWeapon->GetSwingRange(pLocal), "melee_range_multiplier", pWeapon);
	if (pLocal->m_flModelScale() > 1.0f)
	{
		flRange *= pLocal->m_flModelScale();
		flHull *= pLocal->m_flModelScale();
	}
	Vec3 vSwingMins = { -flHull, -flHull, -flHull };
	Vec3 vSwingMaxs = { flHull, flHull, flHull };

	std::deque<TickRecord> vRecords;
	{
		auto pRecords = F::Backtrack.GetRecords(tTarget.m_pEntity);
		if (pRecords)
		{
			if (Vars::Backtrack::Enabled.Value)
				vRecords = *pRecords;
			else
			{
				vRecords = F::Backtrack.GetValidRecords(pRecords, pLocal);
				if (!vRecords.empty())
					vRecords = { vRecords.front() };
			}

			if (vRecords.empty())
				return false;
		}
		if (!pRecords || vRecords.empty())
		{
			if (auto pBones = H::Entities.GetBones(tTarget.m_pEntity->entindex()))
				vRecords.emplace_front(tTarget.m_pEntity->m_flSimulationTime(), *reinterpret_cast<BoneMatrix*>(pBones), std::vector{ HitboxInfo() }, tTarget.m_pEntity->m_vecOrigin());
			else
			{
				matrix3x4 aBones[MAXSTUDIOBONES];
				if (!tTarget.m_pEntity->SetupBones(aBones, MAXSTUDIOBONES, BONE_USED_BY_ANYTHING, tTarget.m_pEntity->m_flSimulationTime()))
					return false;

				vRecords.emplace_front(tTarget.m_pEntity->m_flSimulationTime(), *reinterpret_cast<BoneMatrix*>(&aBones), std::vector{ HitboxInfo() }, tTarget.m_pEntity->m_vecOrigin());
			}
		}
	}
	if (!vSimRecords.empty())
	{
		for (TickRecord& tRecord : vSimRecords)
		{
			vRecords.pop_back();
			vRecords.emplace_front(tRecord.m_flSimTime, BoneMatrix(), std::vector{ HitboxInfo() }, tRecord.m_vOrigin);
		}
		for (TickRecord& tRecord : vRecords)
			tRecord.m_flSimTime -= TICKS_TO_TIME(vSimRecords.size());
	}
	vRecords = tTarget.m_iTargetType == TargetEnum::Player ? F::Backtrack.GetValidRecords(&vRecords, pLocal, true) : vRecords;
	if (!Vars::Backtrack::Enabled.Value && !vRecords.empty())
		vRecords = { vRecords.front() };

	CGameTrace trace = {};
	CTraceFilterHitscan filter = {}; filter.pSkip = pLocal;
	for (auto& tRecord : vRecords)
	{
		Vec3 vRestoreOrigin = tTarget.m_pEntity->GetAbsOrigin();
		Vec3 vRestoreMins = tTarget.m_pEntity->m_vecMins();
		Vec3 vRestoreMaxs = tTarget.m_pEntity->m_vecMaxs();
		tTarget.m_pEntity->SetAbsOrigin(tRecord.m_vOrigin);
		tTarget.m_pEntity->m_vecMins() = tRecord.m_vMins + 0.125f; // account for origin tolerance
		tTarget.m_pEntity->m_vecMaxs() = tRecord.m_vMaxs - 0.125f;

		Vec3 vDiff = { 0, 0, std::clamp(vEyePos.z - tRecord.m_vOrigin.z, tTarget.m_pEntity->m_vecMins().z, tTarget.m_pEntity->m_vecMaxs().z) };
		tTarget.m_vPos = tRecord.m_vOrigin + vDiff;
		Aim(G::CurrentUserCmd->viewangles, Math::CalcAngle(vEyePos, tTarget.m_vPos), tTarget.m_vAngleTo);

		Vec3 vForward; Math::AngleVectors(tTarget.m_vAngleTo, &vForward);
		Vec3 vTraceEnd = vEyePos + (vForward * flRange);

		SDK::TraceHull(vEyePos, vTraceEnd, {}, {}, MASK_SOLID, &filter, &trace);

		bool bReturn = trace.m_pEnt && trace.m_pEnt == tTarget.m_pEntity;
		if (!bReturn)
		{
			SDK::TraceHull(vEyePos, vTraceEnd, vSwingMins, vSwingMaxs, MASK_SOLID, &filter, &trace);
			bReturn = trace.m_pEnt && trace.m_pEnt == tTarget.m_pEntity;
		}

		if (bReturn && Vars::Aimbot::Melee::AutoBackstab.Value && pWeapon->GetWeaponID() == TF_WEAPON_KNIFE)
		{
			if (tTarget.m_iTargetType == TargetEnum::Player)
				bReturn = CanBackstab(tTarget.m_pEntity, pLocal, tTarget.m_vAngleTo);
			else
				bReturn = false;
		}

		tTarget.m_pEntity->SetAbsOrigin(vRestoreOrigin);
		tTarget.m_pEntity->m_vecMins() = vRestoreMins;
		tTarget.m_pEntity->m_vecMaxs() = vRestoreMaxs;

		if (bReturn)
		{
			tTarget.m_tRecord = tRecord;
			tTarget.m_bBacktrack = tTarget.m_iTargetType == TargetEnum::Player /*&& Vars::Backtrack::Enabled.Value*/;

			return true;
		}
		else if (Vars::Aimbot::General::AimType.Value == Vars::Aimbot::General::AimTypeEnum::Smooth
				 || Vars::Aimbot::General::AimType.Value == Vars::Aimbot::General::AimTypeEnum::Assistive)
		{
			auto vAngle = Math::CalcAngle(vEyePos, tTarget.m_vPos);

			Vec3 vForward = Vec3(); Math::AngleVectors(vAngle, &vForward);
			Vec3 vTraceEnd = vEyePos + (vForward * flRange);

			SDK::Trace(vEyePos, vTraceEnd, MASK_SHOT | CONTENTS_GRATE, &filter, &trace);
			if (trace.m_pEnt && trace.m_pEnt == tTarget.m_pEntity)
				return 2;
		}
	}

	return false;
}



bool CAimbotMelee::Aim(Vec3 vCurAngle, Vec3 vToAngle, Vec3& vOut, int iMethod)
{
	if (Vec3* pDoubletapAngle = F::Ticks.GetShootAngle())
	{
		vOut = *pDoubletapAngle;
		return true;
	}

	Math::ClampAngles(vToAngle);

	switch (iMethod)
	{
	case Vars::Aimbot::General::AimTypeEnum::Plain:
	case Vars::Aimbot::General::AimTypeEnum::Silent:
	case Vars::Aimbot::General::AimTypeEnum::Locking:
		vOut = vToAngle;
		return false;
	case Vars::Aimbot::General::AimTypeEnum::Smooth:
		vOut = vCurAngle.LerpAngle(vToAngle, Vars::Aimbot::General::AssistStrength.Value / 100.f);
		return true;
	case Vars::Aimbot::General::AimTypeEnum::Assistive:
		Vec3 vMouseDelta = G::CurrentUserCmd->viewangles.DeltaAngle(G::LastUserCmd->viewangles);
		Vec3 vTargetDelta = vToAngle.DeltaAngle(G::LastUserCmd->viewangles);
		float flMouseDelta = vMouseDelta.Length2D(), flTargetDelta = vTargetDelta.Length2D();
		vTargetDelta = vTargetDelta.Normalized() * std::min(flMouseDelta, flTargetDelta);
		vOut = vCurAngle - vMouseDelta + vMouseDelta.LerpAngle(vTargetDelta, Vars::Aimbot::General::AssistStrength.Value / 100.f);
		return true;
	}

	return false;
}

// assume angle calculated outside with other overload
void CAimbotMelee::Aim(CUserCmd* pCmd, Vec3& vAngle)
{
	switch (Vars::Aimbot::General::AimType.Value)
	{
	case Vars::Aimbot::General::AimTypeEnum::Plain:
	case Vars::Aimbot::General::AimTypeEnum::Smooth:
	case Vars::Aimbot::General::AimTypeEnum::Assistive:
		pCmd->viewangles = vAngle;
		I::EngineClient->SetViewAngles(vAngle);
		break;
	case Vars::Aimbot::General::AimTypeEnum::Silent:
	{
		bool bDoubleTap = F::Ticks.m_bDoubletap || F::Ticks.GetTicks(H::Entities.GetWeapon()) || F::Ticks.m_bSpeedhack;
		if (G::Attacking == 1 || bDoubleTap)
		{
			SDK::FixMovement(pCmd, vAngle);
			pCmd->viewangles = vAngle;
			G::PSilentAngles = true;
		}
		break;
	}
	case Vars::Aimbot::General::AimTypeEnum::Locking:
	{
		SDK::FixMovement(pCmd, vAngle);
		pCmd->viewangles = vAngle;
	}
	}
}

void CAimbotMelee::Run(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CUserCmd* pCmd)
{
	static int iAimType = 0;
	if (pWeapon->m_flSmackTime() < 0.f)
		iAimType = Vars::Aimbot::General::AimType.Value;
	else if (iAimType)
		Vars::Aimbot::General::AimType.Value = iAimType;

	if (!F::Aimbot.m_bRunningSecondary && Vars::Aimbot::General::AimHoldsFire.Value == Vars::Aimbot::General::AimHoldsFireEnum::Always && !G::CanPrimaryAttack && G::LastUserCmd->buttons & IN_ATTACK && Vars::Aimbot::General::AimType.Value)
		pCmd->buttons |= IN_ATTACK;
	if (!Vars::Aimbot::General::AimType.Value || Vars::Aimbot::General::AimType.Value == Vars::Aimbot::General::AimTypeEnum::Silent && !G::CanPrimaryAttack && pWeapon->m_flSmackTime() < 0.f)
		return;

	if (AutoEngie(pLocal, pWeapon, pCmd))
		return;

	if (RunSapper(pLocal, pWeapon, pCmd))
		return;

	auto vTargets = SortTargets(pLocal, pWeapon);
	if (vTargets.empty())
		return;

	iDoubletapTicks = F::Ticks.GetTicks(pWeapon);
	const bool bShouldSwing = iDoubletapTicks <= (GetSwingTime(pWeapon) ? 14 : 0) || Vars::CL_Move::Doubletap::AntiWarp.Value && pLocal->m_hGroundEntity();

	Vec3 vEyePos = pLocal->GetShootPos();
	std::unordered_map<int, std::deque<TickRecord>> pRecordMap;
	std::unordered_map<int, std::deque<Vec3>> mPaths;
	SimulatePlayers(pLocal, pWeapon, vTargets, vEyePos, pRecordMap, mPaths);

	for (auto& tTarget : vTargets)
	{
		const auto iResult = CanHit(tTarget, pLocal, pWeapon, vEyePos, pRecordMap[tTarget.m_pEntity->entindex()]);
		if (!iResult) continue;
		if (iResult == 2)
		{
			G::Target = { tTarget.m_pEntity->entindex(), I::GlobalVars->tickcount };
			Aim(pCmd, tTarget.m_vAngleTo);
			break;
		}

		G::Target = { tTarget.m_pEntity->entindex(), I::GlobalVars->tickcount };
		G::AimPosition = { tTarget.m_vPos, I::GlobalVars->tickcount };

		if (Vars::Aimbot::General::AutoShoot.Value && pWeapon->m_flSmackTime() < 0.f)
		{
			if (bShouldSwing)
				pCmd->buttons |= IN_ATTACK;
			if (iDoubletapTicks)
				F::Ticks.m_bDoubletap = true;
		}

		G::Attacking = SDK::IsAttacking(pLocal, pWeapon, pCmd, true);

		if (G::Attacking == 1)
		{
			if (tTarget.m_bBacktrack)
				pCmd->tick_count = TIME_TO_TICKS(tTarget.m_tRecord.m_flSimTime + F::Backtrack.GetFakeInterp());
			// bug: fast old records seem to be progressively more unreliable ?

		}
		else
		{
			Vec3 vEyePos = pLocal->GetShootPos();
			Aim(G::CurrentUserCmd->viewangles, Math::CalcAngle(vEyePos, tTarget.m_vPos), tTarget.m_vAngleTo);
		}

		bool bPath = Vars::Visuals::Simulation::SwingLines.Value && Vars::Visuals::Simulation::PlayerPath.Value && Vars::Aimbot::General::AutoShoot.Value && !Vars::Debug::Info.Value;
		bool bLine = Vars::Visuals::Line::Enabled.Value;
		bool bBoxes = Vars::Visuals::Hitbox::BonesEnabled.Value & Vars::Visuals::Hitbox::BonesEnabledEnum::OnShot;
		if (pCmd->buttons & IN_ATTACK && pWeapon->m_flSmackTime() < 0.f && bPath)
		{
			G::LineStorage.clear();
			G::BoxStorage.clear();
			G::PathStorage.clear();

			if (bPath)
			{
				if (Vars::Colors::PlayerPath.Value.a)
				{
					G::PathStorage.emplace_back(mPaths[pLocal->entindex()], I::GlobalVars->curtime + Vars::Visuals::Simulation::DrawDuration.Value, Vars::Colors::PlayerPath.Value, Vars::Visuals::Simulation::PlayerPath.Value);
					G::PathStorage.emplace_back(mPaths[tTarget.m_pEntity->entindex()], I::GlobalVars->curtime + Vars::Visuals::Simulation::DrawDuration.Value, Vars::Colors::PlayerPath.Value, Vars::Visuals::Simulation::PlayerPath.Value);
				}
				if (Vars::Colors::PlayerPathClipped.Value.a)
				{
					G::PathStorage.emplace_back(mPaths[pLocal->entindex()], I::GlobalVars->curtime + Vars::Visuals::Simulation::DrawDuration.Value, Vars::Colors::PlayerPathClipped.Value, Vars::Visuals::Simulation::PlayerPath.Value, true);
					G::PathStorage.emplace_back(mPaths[tTarget.m_pEntity->entindex()], I::GlobalVars->curtime + Vars::Visuals::Simulation::DrawDuration.Value, Vars::Colors::PlayerPathClipped.Value, Vars::Visuals::Simulation::PlayerPath.Value, true);
				}
			}
		}

		if (G::Attacking == 1 && (bLine || bBoxes))
		{
			G::LineStorage.clear();
			G::BoxStorage.clear();
			if (bLine)
			{
				Vec3 vEyePos = pLocal->GetShootPos();
				float flDist = vEyePos.DistTo(tTarget.m_vPos);
				Vec3 vForward; Math::AngleVectors(tTarget.m_vAngleTo + pLocal->m_vecPunchAngle(), &vForward);
				if (Vars::Colors::Line.Value.a)
					G::LineStorage.emplace_back(std::pair<Vec3, Vec3>(vEyePos, vEyePos + vForward * flDist), I::GlobalVars->curtime + Vars::Visuals::Simulation::DrawDuration.Value, Vars::Colors::Line.Value);
				if (Vars::Colors::LineClipped.Value.a)
					G::LineStorage.emplace_back(std::pair<Vec3, Vec3>(vEyePos, vEyePos + vForward * flDist), I::GlobalVars->curtime + Vars::Visuals::Simulation::DrawDuration.Value, Vars::Colors::LineClipped.Value, true);
			}
			if (bBoxes)
			{
				auto vBoxes = F::Visuals.GetHitboxes(tTarget.m_tRecord.m_BoneMatrix.m_aBones, tTarget.m_pEntity->As<CBaseAnimating>());
				G::BoxStorage.insert(G::BoxStorage.end(), vBoxes.begin(), vBoxes.end());
			}
		}

		Aim(pCmd, tTarget.m_vAngleTo);
		break;
	}
}

static inline int GetAttachment(CBaseObject* pBuilding, int i)
{
	int iAttachment = pBuilding->GetBuildPointAttachmentIndex(i);
	if (pBuilding->IsSentrygun() && pBuilding->m_iUpgradeLevel() > 1) // idk why i need this
		iAttachment = 3;
	return iAttachment;
}

bool CAimbotMelee::FindNearestBuildPoint(CBaseObject* pBuilding, CTFPlayer* pLocal, Vec3& vPoint)
{
	bool bFoundPoint = false;

	static auto tf_obj_max_attach_dist = U::ConVars.FindVar("tf_obj_max_attach_dist");
	float flNearestPoint = tf_obj_max_attach_dist->GetFloat();
	for (int i = 0; i < pBuilding->GetNumBuildPoints(); i++)
	{
		int v = GetAttachment(pBuilding, i);

		Vec3 vOrigin;
		if (pBuilding->GetAttachment(v, vOrigin)) // issues using pBuilding->GetBuildPoint i on sentries above level 1 for some reason
		{
			if (!SDK::VisPos(pLocal, pBuilding, pLocal->GetShootPos(), vOrigin))
				continue;

			float flDist = (vOrigin - pLocal->GetAbsOrigin()).Length();
			if (flDist < flNearestPoint)
			{
				flNearestPoint = flDist;
				vPoint = vOrigin;
				bFoundPoint = true;
			}
		}
	}

	return bFoundPoint;
}

bool CAimbotMelee::RunSapper(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CUserCmd* pCmd)
{
	if (pWeapon->GetWeaponID() != TF_WEAPON_BUILDER)
		return false;

	std::vector<Target_t> vValidTargets;

	const Vec3 vLocalPos = pLocal->GetShootPos();
	const Vec3 vLocalAngles = I::EngineClient->GetViewAngles();
	for (auto pEntity : H::Entities.GetGroup(EGroupType::BUILDINGS_ENEMIES))
	{
		auto pBuilding = pEntity->As<CBaseObject>();
		if (pBuilding->m_bHasSapper() || pBuilding->m_iTeamNum() != TF_TEAM_BLUE && pBuilding->m_iTeamNum() != TF_TEAM_RED)
			continue;

		Vec3 vPoint;
		if (!FindNearestBuildPoint(pBuilding, pLocal, vPoint))
			continue;

		Vec3 vAngleTo = Math::CalcAngle(vLocalPos, vPoint);
		const float flFOVTo = Math::CalcFov(vLocalAngles, vAngleTo);
		const float flDistTo = vLocalPos.DistTo(vPoint);

		if (flFOVTo > Vars::Aimbot::General::AimFOV.Value)
			continue;

		vValidTargets.emplace_back(pBuilding, TargetEnum::Unknown, vPoint, vAngleTo, flFOVTo, flDistTo);
	}

	F::AimbotGlobal.SortTargets(vValidTargets, Vars::Aimbot::General::TargetSelectionEnum::Distance);
	for (auto& tTarget : vValidTargets)
	{
		static int iLastRun = 0;

		bool bShouldAim = (Vars::Aimbot::General::AimType.Value == Vars::Aimbot::General::AimTypeEnum::Silent ? iLastRun != I::GlobalVars->tickcount - 1 || G::PSilentAngles && !F::Ticks.CanChoke() : true) && Vars::Aimbot::General::AutoShoot.Value;
		pCmd->buttons |= IN_ATTACK;

		if (bShouldAim)
		{
			G::Attacking = true;
			Aim(pCmd->viewangles, Math::CalcAngle(vLocalPos, tTarget.m_vPos), tTarget.m_vAngleTo);
			tTarget.m_vAngleTo.x = pCmd->viewangles.x; // we don't need to care about pitch
			Aim(pCmd, tTarget.m_vAngleTo);

			iLastRun = I::GlobalVars->tickcount;
		}

		break;
	}

	return true;
}

bool CAimbotMelee::AimFriendlyBuilding(CTFPlayer* pLocal, CBaseObject* pBuilding)
{
	// Current Metal
	int iCurrMetal = pLocal->m_iMetalCount();

	// Autorepair is on
	bool bShouldRepair = false;
	switch (pBuilding->GetClassID())
	{
	case ETFClassID::CObjectSentrygun:
	{
		if (Vars::Aimbot::Melee::AutoEngie::AutoRepair.Value & Vars::Aimbot::Melee::AutoEngie::AutoRepairEnum::Sentry)
		{
			// Current sentry ammo
			int iSentryAmmo = pBuilding->As<CObjectSentrygun>()->m_iAmmoShells();
			// Max Sentry ammo
			int iMaxAmmo = 0;

			// Set Ammo depending on level
			switch (pBuilding->m_iUpgradeLevel())
			{
			case 1:
				iMaxAmmo = 150;
				break;
			case 2:
			case 3:
				iMaxAmmo = 200;
			}

			// Sentry needs ammo
			if (iSentryAmmo < iMaxAmmo)
				return true;

			bShouldRepair = true;
			break;
		}
	}
	case ETFClassID::CObjectDispenser:
	{
		if (Vars::Aimbot::Melee::AutoEngie::AutoRepair.Value & Vars::Aimbot::Melee::AutoEngie::AutoRepairEnum::Dispenser)
			bShouldRepair = true;
		break;
	}
	case ETFClassID::CObjectTeleporter:
	{
		if (Vars::Aimbot::Melee::AutoEngie::AutoRepair.Value & Vars::Aimbot::Melee::AutoEngie::AutoRepairEnum::Teleporter)
			bShouldRepair = true;
		break;
	}
	default:
		break;
	}

	// Buildings needs to be repaired
	if (iCurrMetal && bShouldRepair && pBuilding->m_iHealth() != pBuilding->m_iMaxHealth())
		return true;

	// Autoupgrade is on
	if (Vars::Aimbot::Melee::AutoEngie::AutoUpgrade.Value)
	{
		// Upgrade lvel
		int iUpgradeLevel = pBuilding->m_iUpgradeLevel();

		// Don't upgrade mini sentries
		if (pBuilding->m_bMiniBuilding())
			return false;

		int iLevel = 0;
		// Pick The right rvar to check depending on building type
		switch (pBuilding->GetClassID())
		{

		case ETFClassID::CObjectSentrygun:
			// Enabled check
			if (!(Vars::Aimbot::Melee::AutoEngie::AutoUpgrade.Value & Vars::Aimbot::Melee::AutoEngie::AutoUpgradeEnum::Sentry))
				return false;
			iLevel = Vars::Aimbot::Melee::AutoEngie::AutoUpgradeSentryLVL.Value;
			break;

		case ETFClassID::CObjectDispenser:
			// Enabled check
			if (!(Vars::Aimbot::Melee::AutoEngie::AutoUpgrade.Value & Vars::Aimbot::Melee::AutoEngie::AutoUpgradeEnum::Dispenser))
				return false;
			iLevel = Vars::Aimbot::Melee::AutoEngie::AutoUpgradeDispenserLVL.Value;
			break;

		case ETFClassID::CObjectTeleporter:
			// Enabled check
			if (!(Vars::Aimbot::Melee::AutoEngie::AutoUpgrade.Value & Vars::Aimbot::Melee::AutoEngie::AutoUpgradeEnum::Teleporter))
				return false;
			iLevel = Vars::Aimbot::Melee::AutoEngie::AutoUpgradeTeleporterLVL.Value;
			break;
		}

		// Can be upgraded
		if (iUpgradeLevel < iLevel && iCurrMetal)
			return true;
	}
	return false;
}

bool ShouldWrenchBuilding(ETFClassID id)
{
	switch (id)
	{
	case ETFClassID::CObjectSentrygun:
		// Repair sentries check
		if (!(Vars::Aimbot::Melee::AutoEngie::AutoRepair.Value & Vars::Aimbot::Melee::AutoEngie::AutoRepairEnum::Sentry) && !(Vars::Aimbot::Melee::AutoEngie::AutoUpgrade.Value & Vars::Aimbot::Melee::AutoEngie::AutoUpgradeEnum::Sentry))
			return false;
		break;
	case ETFClassID::CObjectDispenser:
		// Repair Dispensers check
		if (!(Vars::Aimbot::Melee::AutoEngie::AutoRepair.Value & Vars::Aimbot::Melee::AutoEngie::AutoRepairEnum::Dispenser) && !(Vars::Aimbot::Melee::AutoEngie::AutoUpgrade.Value & Vars::Aimbot::Melee::AutoEngie::AutoUpgradeEnum::Dispenser))
			return false;
		break;
	case ETFClassID::CObjectTeleporter:
		// Repair Teleporters check
		if (!(Vars::Aimbot::Melee::AutoEngie::AutoRepair.Value & Vars::Aimbot::Melee::AutoEngie::AutoRepairEnum::Teleporter) && !(Vars::Aimbot::Melee::AutoEngie::AutoUpgrade.Value & Vars::Aimbot::Melee::AutoEngie::AutoUpgradeEnum::Teleporter))
			return false;
		break;
	default:
		return false;
	}
	return true;
}

std::vector<Target_t> CAimbotMelee::GetTargetBuilding(CTFPlayer* pLocal, CTFWeaponBase* pWeapon)
{
	std::vector<Target_t> vValidTargets;

	const Vec3 vLocalPos = pLocal->GetShootPos();
	const Vec3 vLocalAngles = I::EngineClient->GetViewAngles();

	for (auto pEntity : H::Entities.GetGroup(EGroupType::BUILDINGS_TEAMMATES))
	{
		if (!pEntity->IsBuilding())
			continue;

		if (!ShouldWrenchBuilding(pEntity->GetClassID()))
			continue;

		Vec3 vPos = pEntity->GetCenter();
		Vec3 vAngleTo = Math::CalcAngle(vLocalPos, vPos);
		const float flFOVTo = Math::CalcFov(vLocalAngles, vAngleTo);
		const float flDistTo = vLocalPos.DistTo(vPos);

		if (flFOVTo > Vars::Aimbot::General::AimFOV.Value)
			continue;

		vValidTargets.push_back({ pEntity, TargetEnum::Dispenser, vPos, vAngleTo, flFOVTo, flDistTo });
	}

	std::sort(vValidTargets.begin(), vValidTargets.end(), [&](const Target_t& a, const Target_t& b) -> bool
			  {
				  const auto a_iClassID = a.m_pEntity->GetClassID();
				  const auto b_iClassID = b.m_pEntity->GetClassID();
				  switch (a_iClassID)
				  {
				  case ETFClassID::CObjectSentrygun:
				  {
					  if (Vars::Aimbot::Melee::AutoEngie::AutoRepairPrio.Value == Vars::Aimbot::Melee::AutoEngie::AutoRepairPrioEnum::Sentry)
						  return a_iClassID != b_iClassID;
					  break;
				  }
				  case ETFClassID::CObjectDispenser:
				  {
					  if (Vars::Aimbot::Melee::AutoEngie::AutoRepairPrio.Value == Vars::Aimbot::Melee::AutoEngie::AutoRepairPrioEnum::Dispenser)
						  return a_iClassID != b_iClassID;
					  break;
				  }
				  case ETFClassID::CObjectTeleporter:
				  {
					  if (Vars::Aimbot::Melee::AutoEngie::AutoRepairPrio.Value == Vars::Aimbot::Melee::AutoEngie::AutoRepairPrioEnum::Teleporter)
						  return a_iClassID != b_iClassID;
					  break;
				  }
				  default: break;
				  }
				  return false;
			  });

	return vValidTargets;
}

bool CAimbotMelee::AutoEngie(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CUserCmd* pCmd)
{
	if (pLocal->m_iClass() != TF_CLASS_ENGINEER || !Vars::Aimbot::Melee::AutoEngie::AutoUpgrade.Value && !Vars::Aimbot::Melee::AutoEngie::AutoRepair.Value)
		return false;

	auto vTargets = GetTargetBuilding(pLocal, pWeapon);
	if (vTargets.empty())
		return false;

	iDoubletapTicks = F::Ticks.GetTicks(pWeapon);
	const bool bShouldSwing = iDoubletapTicks <= (GetSwingTime(pWeapon) ? 14 : 0) || Vars::CL_Move::Doubletap::AntiWarp.Value && pLocal->m_hGroundEntity();

	Vec3 vEyePos = pLocal->GetShootPos();
	std::unordered_map<int, std::deque<TickRecord>> pRecordMap;
	std::unordered_map<int, std::deque<Vec3>> mPaths;
	SimulatePlayers(pLocal, pWeapon, vTargets, vEyePos, pRecordMap, mPaths);

	for (auto& tTarget : vTargets)
	{
		if (!AimFriendlyBuilding(pLocal, tTarget.m_pEntity->As<CBaseObject>()))
			continue;

		const auto iResult = CanHit(tTarget, pLocal, pWeapon, vEyePos, pRecordMap[tTarget.m_pEntity->entindex()]);
		if (!iResult) continue;
		if (iResult == 2)
		{
			G::Target = { tTarget.m_pEntity->entindex(), I::GlobalVars->tickcount };
			Aim(pCmd, tTarget.m_vAngleTo);
			return true;
		}

		G::Target = { tTarget.m_pEntity->entindex(), I::GlobalVars->tickcount };
		G::AimPosition = { tTarget.m_vPos, I::GlobalVars->tickcount };

		if (Vars::Aimbot::General::AutoShoot.Value && pWeapon->m_flSmackTime() < 0.f)
		{
			if (bShouldSwing)
				pCmd->buttons |= IN_ATTACK;
			if (iDoubletapTicks)
				F::Ticks.m_bDoubletap = true;
		}

		G::Attacking = SDK::IsAttacking(pLocal, pWeapon, pCmd, true);

		bool bPath = Vars::Visuals::Simulation::SwingLines.Value && Vars::Visuals::Simulation::PlayerPath.Value && Vars::Aimbot::General::AutoShoot.Value && !Vars::Debug::Info.Value;
		bool bLine = Vars::Visuals::Line::Enabled.Value;
		bool bBoxes = Vars::Visuals::Hitbox::BonesEnabled.Value & Vars::Visuals::Hitbox::BonesEnabledEnum::OnShot;
		if (pCmd->buttons & IN_ATTACK && pWeapon->m_flSmackTime() < 0.f && bPath)
		{
			G::LineStorage.clear();
			G::BoxStorage.clear();
			G::PathStorage.clear();

			if (bPath)
			{
				if (Vars::Colors::PlayerPath.Value.a)
				{
					G::PathStorage.push_back({ mPaths[pLocal->entindex()], I::GlobalVars->curtime + Vars::Visuals::Simulation::DrawDuration.Value, Vars::Colors::PlayerPath.Value, Vars::Visuals::Simulation::PlayerPath.Value });
				}
				if (Vars::Colors::PlayerPathClipped.Value.a)
				{
					G::PathStorage.push_back({ mPaths[pLocal->entindex()], I::GlobalVars->curtime + Vars::Visuals::Simulation::DrawDuration.Value, Vars::Colors::PlayerPathClipped.Value, Vars::Visuals::Simulation::PlayerPath.Value, true });
				}
			}
		}

		if (G::Attacking == 1 && (bLine || bBoxes))
		{
			G::LineStorage.clear();
			G::BoxStorage.clear();
			if (bLine)
			{
				Vec3 vEyePos = pLocal->GetShootPos();
				float flDist = vEyePos.DistTo(tTarget.m_vPos);
				Vec3 vForward; Math::AngleVectors(tTarget.m_vAngleTo + pLocal->m_vecPunchAngle(), &vForward);
				if (Vars::Colors::Line.Value.a)
					G::LineStorage.push_back({ { vEyePos, vEyePos + vForward * flDist }, I::GlobalVars->curtime + Vars::Visuals::Simulation::DrawDuration.Value, Vars::Colors::Line.Value });
				if (Vars::Colors::LineClipped.Value.a)
					G::LineStorage.push_back({ { vEyePos, vEyePos + vForward * flDist }, I::GlobalVars->curtime + Vars::Visuals::Simulation::DrawDuration.Value, Vars::Colors::LineClipped.Value, true });
			}
			if (bBoxes)
			{
				auto vBoxes = F::Visuals.GetHitboxes(tTarget.m_tRecord.m_BoneMatrix.m_aBones, tTarget.m_pEntity->As<CBaseAnimating>());
				G::BoxStorage.insert(G::BoxStorage.end(), vBoxes.begin(), vBoxes.end());
			}
		}

		if (G::Attacking != 1)
		{
			Vec3 vEyePos = pLocal->GetShootPos();
			Aim(G::CurrentUserCmd->viewangles, Math::CalcAngle(vEyePos, tTarget.m_vPos), tTarget.m_vAngleTo);
		}

		Aim(pCmd, tTarget.m_vAngleTo);
		return true;
	}
	return false;
}
