#include "CritHack.h"

#include "../TickHandler/TickHandler.h"
#include "NoPipeRot.h"

#define WEAPON_RANDOM_RANGE				10000
#define TF_DAMAGE_CRIT_MULTIPLIER		3.0f
#define TF_DAMAGE_CRIT_CHANCE			0.02f
#define TF_DAMAGE_CRIT_CHANCE_RAPID		0.02f
#define TF_DAMAGE_CRIT_CHANCE_MELEE		0.15f
#define TF_DAMAGE_CRIT_DURATION_RAPID	2.0f

void CCritHack::Fill(const CUserCmd* pCmd, int n)
{
	if (!m_iFillStart)
		m_iFillStart = pCmd->command_number;

	for (auto& [iSlot, tStorage] : m_mStorage)
	{
		if (!tStorage.m_bActive)
			continue;

		for (auto it = tStorage.m_vCritCommands.begin(); it != tStorage.m_vCritCommands.end();)
		{
			if (*it <= pCmd->command_number)
				it = tStorage.m_vCritCommands.erase(it);
			else
				++it;
		}
		for (auto it = tStorage.m_vSkipCommands.begin(); it != tStorage.m_vSkipCommands.end();)
		{
			if (*it <= pCmd->command_number)
				it = tStorage.m_vSkipCommands.erase(it);
			else
				++it;
		}
		/*tStorage.m_vCritCommands.clear( );
		tStorage.m_vSkipCommands.clear( );
		for ( int i = 0; i < ( sizeof( no_pipe_rotation_cmdnums ) / 4 ); i++ )
		{
			if ( tStorage.m_vCritCommands.size( ) >= unsigned( n ) )
				break;

			const int iCmdNum = no_pipe_rotation_cmdnums[ i ];
			if (IsCritCommand(iSlot, tStorage.m_iEntIndex, tStorage.m_flMultCritChance, iCmdNum))
				tStorage.m_vCritCommands.push_back(iCmdNum);
		}
		for ( int i = 0; i < ( sizeof( no_pipe_rotation_cmdnums ) / 4 ); i++ )
		{
			if ( tStorage.m_vSkipCommands.size( ) >= unsigned( n ) )
				break;

			const int iCmdNum = no_pipe_rotation_cmdnums[ i ];
			if ( IsCritCommand( iSlot, tStorage.m_iEntIndex, tStorage.m_flMultCritChance, iCmdNum, false ) )
				tStorage.m_vSkipCommands.push_back( iCmdNum );
		}*/
		for (int i = 0; i < n; i++)
		{
			if (tStorage.m_vCritCommands.size() >= unsigned(n))
				break;

			const int iCmdNum = m_iFillStart + i;
			if (IsCritCommand(iSlot, tStorage.m_iEntIndex, tStorage.m_flMultCritChance, iCmdNum))
				tStorage.m_vCritCommands.push_back(iCmdNum);
		}
		for (int i = 0; i < n; i++)
		{
			if (tStorage.m_vSkipCommands.size() >= unsigned(n))
				break;

			const int iCmdNum = m_iFillStart + i;
			if (IsCritCommand(iSlot, tStorage.m_iEntIndex, tStorage.m_flMultCritChance, iCmdNum, false))
				tStorage.m_vSkipCommands.push_back(iCmdNum);
		}
	}

	m_iFillStart += n;
}



bool CCritHack::IsCritCommand(int iSlot, int iIndex, float flMultCritChance, const i32 command_number, const bool bCrit, const bool bSafe)
{
	const auto uSeed = MD5_PseudoRandom(command_number) & 0x7FFFFFFF;
	CValve_Random* Random = new CValve_Random();
	Random->SetSeed(DecryptOrEncryptSeed(iSlot, iIndex, uSeed));
	//SDK::RandomSeed( DecryptOrEncryptSeed( iSlot, iIndex, uSeed ) );
	const int iRandom = Random->RandomInt(0, WEAPON_RANDOM_RANGE - 1);//SDK::RandomInt(0, WEAPON_RANDOM_RANGE - 1);

	if (bSafe)
	{
		int iLower, iUpper;
		if (iSlot == EWeaponSlot::SLOT_MELEE)
			iLower = 1500, iUpper = 6000;
		else
			iLower = 100, iUpper = 800;
		iLower *= flMultCritChance, iUpper *= flMultCritChance;
		if (bCrit ? iLower >= 0 : iUpper < WEAPON_RANDOM_RANGE)
			return bCrit ? iRandom < iLower : !(iRandom < iUpper);
	}
	const int iRange = m_flCritChance * WEAPON_RANDOM_RANGE;
	return bCrit ? iRandom < iRange : !(iRandom < iRange);
}

u32 CCritHack::DecryptOrEncryptSeed(int iSlot, int iIndex, u32 uSeed)
{
	int iLeft = iSlot == SLOT_MELEE ? 8 : 0;
	unsigned int iMask = iIndex << (iLeft + 8) | I::EngineClient->GetLocalPlayer() << iLeft;
	return iMask ^ uSeed;
}



void CCritHack::GetTotalCrits(CTFPlayer* pLocal, CTFWeaponBase* pWeapon)
{
	int iSlot = pWeapon->GetSlot();
	if (!m_mStorage.contains(iSlot))
		return;

	auto& tStorage = m_mStorage[iSlot];
	if (!tStorage.m_bActive)
		return;

	static float flOldBucket = 0.f; static int iOldID = 0, iOldCritChecks = 0, iOldCritSeedRequests = 0;
	const float flBucket = pWeapon->m_flCritTokenBucket(); const int iID = pWeapon->GetWeaponID(), iCritChecks = pWeapon->m_nCritChecks(), iCritSeedRequests = pWeapon->m_nCritSeedRequests();
	const bool bShouldUpdate = !tStorage.m_flDamage || flOldBucket != flBucket || iOldID != iID || iOldCritChecks != iCritChecks || iOldCritSeedRequests != iCritSeedRequests;
	flOldBucket = flBucket; iOldID = iID, iOldCritChecks = iCritChecks, iOldCritSeedRequests = iCritSeedRequests;
	if (!bShouldUpdate)
		return;

	static auto tf_weapon_criticals_bucket_cap = U::ConVars.FindVar("tf_weapon_criticals_bucket_cap");
	const float flBucketCap = tf_weapon_criticals_bucket_cap ? tf_weapon_criticals_bucket_cap->GetFloat() : 1000.f;
	bool bRapidFire = pWeapon->IsRapidFire();
	float flFireRate = pWeapon->GetFireRate();

	float flDamage = pWeapon->GetDamage();
	int nProjectilesPerShot = pWeapon->GetBulletsPerShot(false);
	if (iSlot != SLOT_MELEE && nProjectilesPerShot > 0)
		nProjectilesPerShot = SDK::AttribHookValue(nProjectilesPerShot, "mult_bullets_per_shot", pWeapon);
	else
		nProjectilesPerShot = 1;
	float flBaseDamage = flDamage *= nProjectilesPerShot;
	if (bRapidFire)
	{
		flDamage *= TF_DAMAGE_CRIT_DURATION_RAPID / flFireRate;
		if (flDamage * TF_DAMAGE_CRIT_MULTIPLIER > flBucketCap)
			flDamage = flBucketCap / TF_DAMAGE_CRIT_MULTIPLIER;
	}

	float flMult = iSlot == SLOT_MELEE ? 0.5f : Math::RemapValClamped(float(iCritSeedRequests + 1) / (iCritChecks + 1), 0.1f, 1.f, 1.f, 3.f);
	float flCost = flDamage * TF_DAMAGE_CRIT_MULTIPLIER;

	int iPotentialCrits = (flBucketCap - flBaseDamage) / (TF_DAMAGE_CRIT_MULTIPLIER * flDamage / (iSlot == SLOT_MELEE ? 2 : 1) - flBaseDamage);
	int iAvailableCrits = 0;
	{
		int iTestShots = iCritChecks, iTestCrits = iCritSeedRequests;
		float flTestBucket = flBucket;
		int iAttempts = std::min(iPotentialCrits, 1000);
		for (int i = 0; i < iAttempts; i++)
		{
			iTestShots++; iTestCrits++;

			float flTestMult = iSlot == SLOT_MELEE ? 0.5f : Math::RemapValClamped(float(iTestCrits) / iTestShots, 0.1f, 1.f, 1.f, 3.f);
			flTestBucket = std::min(flTestBucket + flBaseDamage, flBucketCap) - flCost * flTestMult;
			if (flTestBucket < 0.f)
				break;

			iAvailableCrits++;
		}
	}

	int iNextCrit = 0;
	if (iAvailableCrits != iPotentialCrits)
	{
		int iTestShots = iCritChecks, iTestCrits = iCritSeedRequests;
		float flTestBucket = flBucket;
		float flTickBase = TICKS_TO_TIME(pLocal->m_nTickBase());
		float flLastRapidFireCritCheckTime = pWeapon->m_flLastRapidFireCritCheckTime();
		int iAttempts = std::min(iPotentialCrits, 1000);
		for (int i = 0; i < 1000; i++)
		{
			int iCrits = 0;
			{
				int iTestShots2 = iTestShots, iTestCrits2 = iTestCrits;
				float flTestBucket2 = flTestBucket;
				for (int j = 0; j < iAttempts; j++)
				{
					iTestShots2++; iTestCrits2++;

					float flTestMult = iSlot == SLOT_MELEE ? 0.5f : Math::RemapValClamped(float(iTestCrits2) / iTestShots2, 0.1f, 1.f, 1.f, 3.f);
					flTestBucket2 = std::min(flTestBucket2 + flBaseDamage, flBucketCap) - flCost * flTestMult;
					if (flTestBucket2 < 0.f)
						break;

					iCrits++;
				}
			}
			if (iAvailableCrits < iCrits)
				break;

			if (!bRapidFire)
				iTestShots++;
			else 
			{
				flTickBase += std::ceilf(flFireRate / TICK_INTERVAL) * TICK_INTERVAL;
				if (flTickBase >= flLastRapidFireCritCheckTime + 1.f || !i && flTestBucket == flBucketCap)
				{
					iTestShots++;
					flLastRapidFireCritCheckTime = flTickBase;
				}
			}

			flTestBucket = std::min(flTestBucket + flBaseDamage, flBucketCap);

			iNextCrit++;
		}
	}

	tStorage.m_flDamage = flBaseDamage;
	tStorage.m_flCost = flCost * flMult;
	tStorage.m_iPotentialCrits = iPotentialCrits;
	tStorage.m_iAvailableCrits = iAvailableCrits;
	tStorage.m_iNextCrit = iNextCrit;
}

void CCritHack::CanFireCritical(CTFPlayer* pLocal, CTFWeaponBase* pWeapon)
{
	m_bCritBanned = false;
	m_iDamageTilUnban = 0;

	if (pWeapon->GetSlot() == SLOT_MELEE)
		m_flCritChance = TF_DAMAGE_CRIT_CHANCE_MELEE * pLocal->GetCritMult();
	else if (pWeapon->IsRapidFire())
	{
		m_flCritChance = TF_DAMAGE_CRIT_CHANCE_RAPID * pLocal->GetCritMult();
		float flNonCritDuration = (TF_DAMAGE_CRIT_DURATION_RAPID / m_flCritChance) - TF_DAMAGE_CRIT_DURATION_RAPID;
		m_flCritChance = 1.f / flNonCritDuration;
	}
	else
		m_flCritChance = TF_DAMAGE_CRIT_CHANCE * pLocal->GetCritMult();
	m_flCritChance = SDK::AttribHookValue(m_flCritChance, "mult_crit_chance", pWeapon);

	if (!m_iAllDamage || !m_iCritDamage || pWeapon->GetSlot() == SLOT_MELEE)
		return;

	const float flNormalizedDamage = m_iCritDamage / TF_DAMAGE_CRIT_MULTIPLIER;
	const float flObservedCritChance = flNormalizedDamage / (flNormalizedDamage + m_iAllDamage - m_iCritDamage);
	float flCritChance = m_flCritChance + 0.1f;
	if (m_bCritBanned = flObservedCritChance > flCritChance)
		m_iDamageTilUnban = flNormalizedDamage / flCritChance + m_iCritDamage - flNormalizedDamage - m_iAllDamage;
}

bool CCritHack::WeaponCanCrit(CTFWeaponBase* pWeapon, bool bWeaponOnly)
{
	if (!bWeaponOnly && !pWeapon->AreRandomCritsEnabled() || SDK::AttribHookValue(1.f, "mult_crit_chance", pWeapon) <= 0.f)
		return false;

	switch (pWeapon->GetWeaponID())
	{
	case TF_WEAPON_PDA:
	case TF_WEAPON_PDA_ENGINEER_BUILD:
	case TF_WEAPON_PDA_ENGINEER_DESTROY:
	case TF_WEAPON_PDA_SPY:
	case TF_WEAPON_PDA_SPY_BUILD:
	case TF_WEAPON_BUILDER:
	case TF_WEAPON_INVIS:
	case TF_WEAPON_JAR_MILK:
	case TF_WEAPON_LUNCHBOX:
	case TF_WEAPON_BUFF_ITEM:
	case TF_WEAPON_FLAME_BALL:
	case TF_WEAPON_ROCKETPACK:
	case TF_WEAPON_JAR_GAS:
	case TF_WEAPON_LASER_POINTER:
	case TF_WEAPON_MEDIGUN:
	case TF_WEAPON_SNIPERRIFLE:
	case TF_WEAPON_SNIPERRIFLE_DECAP:
	case TF_WEAPON_SNIPERRIFLE_CLASSIC:
	case TF_WEAPON_COMPOUND_BOW:
	case TF_WEAPON_JAR:
	case TF_WEAPON_KNIFE:
		return false;
	}

	return true;
}



void CCritHack::ResetWeapons(CTFPlayer* pLocal)
{
	if (!pLocal->m_hMyWeapons())
		return;

	for (auto& [iSlot, tStorage] : m_mStorage)
		tStorage.m_bActive = false;

	for (int i = 0; i < MAX_WEAPONS; i++)
	{
		auto pWeapon = pLocal->GetWeaponFromSlot(i);
		if (!pWeapon || !WeaponCanCrit(pWeapon))
			continue;

		int iSlot = pWeapon->GetSlot();
		auto& tStorage = m_mStorage[iSlot];
		tStorage.m_bActive = true;

		int iEntIndex = pWeapon->entindex();
		int iDefIndex = pWeapon->m_iItemDefinitionIndex();
		float flMultCritChance = SDK::AttribHookValue(1.f, "mult_crit_chance", pWeapon);

		if (tStorage.m_iEntIndex != iEntIndex || tStorage.m_iDefIndex != iDefIndex || tStorage.m_flMultCritChance != flMultCritChance)
			tStorage = { iEntIndex, iDefIndex, flMultCritChance };
	}
}

void CCritHack::Reset()
{
	m_mStorage.clear();

	m_iFillStart = 0;

	m_iCritDamage = 0;
	m_iAllDamage = 0;

	m_bCritBanned = false;
	m_iDamageTilUnban = 0;
	m_flCritChance = 0.f;

	m_mHealthStorage.clear();
}



float CCritHack::GetCost( CTFWeaponBase* pWeapon )
{
	static auto tf_weapon_criticals_bucket_cap = U::ConVars.FindVar("tf_weapon_criticals_bucket_cap");
	const float flBucketCap = tf_weapon_criticals_bucket_cap ? tf_weapon_criticals_bucket_cap->GetFloat() : 1000.f;
	bool bRapidFire = pWeapon->IsRapidFire();
	float flFireRate = pWeapon->GetFireRate();

	float flDamage = pWeapon->GetDamage();
	int nProjectilesPerShot = pWeapon->GetBulletsPerShot(false);
	if (pWeapon->GetSlot() != SLOT_MELEE && nProjectilesPerShot > 0)
		nProjectilesPerShot = SDK::AttribHookValue(nProjectilesPerShot, "mult_bullets_per_shot", pWeapon);
	else
		nProjectilesPerShot = 1;
	float flBaseDamage = flDamage *= nProjectilesPerShot;
	if (bRapidFire)
	{
		flDamage *= TF_DAMAGE_CRIT_DURATION_RAPID / flFireRate;
		if (flDamage * TF_DAMAGE_CRIT_MULTIPLIER > flBucketCap)
			flDamage = flBucketCap / TF_DAMAGE_CRIT_MULTIPLIER;
	}

	return flDamage * TF_DAMAGE_CRIT_MULTIPLIER;
}

void CCritHack::Run(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CUserCmd* pCmd)
{
	if (!pLocal || !pWeapon || !pLocal->IsAlive() || !I::EngineClient->IsInGame())
		return;

	int iSlot = pWeapon->GetSlot();
	ResetWeapons(pLocal);
	Fill(pCmd, 25);
	GetTotalCrits(pLocal, pWeapon);
	CanFireCritical(pLocal, pWeapon);
	if (pLocal->IsCritBoosted() || pWeapon->m_flCritTime() > I::GlobalVars->curtime || !m_mStorage.contains(iSlot))
		return;

	auto& tStorage = m_mStorage[iSlot];
	if (!tStorage.m_bActive)
		return;

	if (pWeapon->GetWeaponID() == TF_WEAPON_MINIGUN && pCmd->buttons & IN_ATTACK)
		pCmd->buttons &= ~IN_ATTACK2;

	bool bAttacking = G::Attacking /*== 1*/ || F::Ticks.m_bDoubletap || F::Ticks.m_bSpeedhack;
	if (G::PrimaryWeaponType == EWeaponType::MELEE)
	{
		bAttacking = G::CanPrimaryAttack && pCmd->buttons & IN_ATTACK;
		if (!bAttacking && pWeapon->GetWeaponID() == TF_WEAPON_FISTS)
			bAttacking = G::CanPrimaryAttack && pCmd->buttons & IN_ATTACK2;
	}
	else if (pWeapon->GetWeaponID() == TF_WEAPON_MINIGUN && !(G::LastUserCmd->buttons & IN_ATTACK))
		bAttacking = false;

	bool bRapidFire = pWeapon->IsRapidFire();
	bool bStreamWait = bRapidFire && TICKS_TO_TIME(pLocal->m_nTickBase()) < pWeapon->m_flLastRapidFireCritCheckTime() + 1.f;

	int iClosestCrit = !tStorage.m_vCritCommands.empty() ? tStorage.m_vCritCommands.front() : 0;
	int iClosestSkip = !tStorage.m_vSkipCommands.empty() ? tStorage.m_vSkipCommands.front() : 0;

	if (bAttacking)
	{
		const bool bCanCrit = tStorage.m_iAvailableCrits > 0 && (!m_bCritBanned || iSlot == SLOT_MELEE) && !bStreamWait;
		const bool bPressed = Vars::CritHack::ForceCrits.Value || iSlot == SLOT_MELEE && Vars::CritHack::AlwaysMeleeCrit.Value && (Vars::Aimbot::General::AutoShoot.Value ? pCmd->buttons & IN_ATTACK && !(G::OriginalMove.m_iButtons & IN_ATTACK) : Vars::Aimbot::General::AimType.Value);
		if (!Vars::Misc::Game::AntiCheatCompatibility.Value)
		{
			if (bCanCrit && bPressed && iClosestCrit)
				pCmd->command_number = iClosestCrit;
			else if (Vars::CritHack::AvoidRandom.Value && iClosestSkip)
				pCmd->command_number = iClosestSkip;
		}
		else if (Vars::Misc::Game::AntiCheatCritHack.Value)
		{
			bool bShouldAvoid = false;

			bool bCritCommand = IsCritCommand(iSlot, tStorage.m_iEntIndex, tStorage.m_flMultCritChance, pCmd->command_number, true, false);
			if (bCanCrit && bPressed && !bCritCommand
				|| Vars::CritHack::AvoidRandom.Value && !bPressed && bCritCommand)
				bShouldAvoid = true;

			if (bShouldAvoid)
			{
				pCmd->buttons &= ~IN_ATTACK;
				pCmd->viewangles = G::OriginalMove.m_vView;
				G::PSilentAngles = false;
			}
		}
	}

	m_iWishRandomSeed = MD5_PseudoRandom(pCmd->command_number) & std::numeric_limits<int>::max();

	if (pCmd->command_number == iClosestCrit)
		tStorage.m_vCritCommands.pop_front();
	else if (pCmd->command_number == iClosestSkip)
		tStorage.m_vSkipCommands.pop_front();
}

int CCritHack::PredictCmdNum(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CUserCmd* pCmd)
{
	if (!pLocal || !pWeapon || !pLocal->IsAlive() || !I::EngineClient->IsInGame())
		return pCmd->command_number;

	int iSlot = pWeapon->GetSlot();
	if (pLocal->IsCritBoosted() || pWeapon->m_flCritTime() > I::GlobalVars->curtime || !m_mStorage.contains(iSlot))
		return pCmd->command_number;

	auto& tStorage = m_mStorage[iSlot];
	bool bRapidFire = pWeapon->IsRapidFire();
	bool bStreamWait = bRapidFire && TICKS_TO_TIME(pLocal->m_nTickBase()) < pWeapon->m_flLastRapidFireCritCheckTime() + 1.f;

	int closestCrit = !tStorage.m_vCritCommands.empty() ? tStorage.m_vCritCommands.front() : 0;
	int closestSkip = !tStorage.m_vSkipCommands.empty() ? tStorage.m_vSkipCommands.front() : 0;

	const bool bCanCrit = tStorage.m_iAvailableCrits > 0 && (!m_bCritBanned || pWeapon->GetSlot() == SLOT_MELEE) && !bStreamWait;
	const bool bPressed = Vars::CritHack::ForceCrits.Value || pWeapon->GetSlot() == SLOT_MELEE && Vars::CritHack::AlwaysMeleeCrit.Value && (Vars::Aimbot::General::AutoShoot.Value ? pCmd->buttons & IN_ATTACK && !(G::OriginalMove.m_iButtons & IN_ATTACK) : Vars::Aimbot::General::AimType.Value);
	if (bCanCrit && bPressed && closestCrit)
		return closestCrit;
	else if (Vars::CritHack::AvoidRandom.Value && closestSkip)
		return closestSkip;

	return pCmd->command_number;
}

bool CCritHack::CalcIsAttackCriticalHandler(CTFPlayer* pLocal, CTFWeaponBase* pWeapon)
{
	if (!I::Prediction->m_bFirstTimePredicted || !pLocal || !pWeapon)
		return false;

	if (m_iWishRandomSeed)
	{
		*G::RandomSeed() = m_iWishRandomSeed;
		m_iWishRandomSeed = 0;
	}

	return true;
}

void CCritHack::Event(IGameEvent* pEvent, uint32_t uHash, CTFPlayer* pLocal)
{
	switch (uHash)
	{
	case FNV1A::Hash32Const("player_hurt"):
	{
		if (!pLocal || !pLocal->m_hMyWeapons())
			return;

		int iVictim = I::EngineClient->GetPlayerForUserID(pEvent->GetInt("userid"));
		int iAttacker = I::EngineClient->GetPlayerForUserID(pEvent->GetInt("attacker"));
		bool bCrit = pEvent->GetBool("crit") || pEvent->GetBool("minicrit");
		int iDamage = pEvent->GetInt("damageamount");
		int iHealth = pEvent->GetInt("health");
		int iWeaponID = pEvent->GetInt("weaponid");

		if (m_mHealthStorage.contains(iVictim) && !iHealth)
			iDamage = std::min(iDamage, m_mHealthStorage[iVictim]);
		if (iHealth)
		{
			if (m_mHealthStorage.contains(iVictim))
				m_mHealthStorage[iVictim] = std::min(iHealth, m_mHealthStorage[iVictim]);
			else
				m_mHealthStorage[iVictim] = iHealth;
		}

		if (iVictim == iAttacker || iAttacker != pLocal->entindex())
			return;

		CTFWeaponBase* pWeapon = nullptr;
		for (int i = 0; i < MAX_WEAPONS; i++)
		{
			auto pWeapon2 = pLocal->GetWeaponFromSlot(i);
			if (!pWeapon2 || pWeapon2->GetWeaponID() != iWeaponID)
				continue;

			pWeapon = pWeapon2;
			break;
		}
		if (!pWeapon || pWeapon->GetSlot() == SLOT_MELEE)
			return;

		m_iAllDamage += iDamage;
		if (bCrit && !pLocal->IsCritBoosted())
			m_iCritDamage += iDamage;

		return;
	}
	case FNV1A::Hash32Const("teamplay_round_start"):
		m_iAllDamage = m_iCritDamage = 0;
		return;
	case FNV1A::Hash32Const("client_beginconnect"):
	case FNV1A::Hash32Const("client_disconnect"):
	case FNV1A::Hash32Const("game_newmap"):
		Reset();
	}
}

void CCritHack::Store()
{
	auto pResource = H::Entities.GetPR();
	if (!pResource)
		return;

	for (auto& pEntity : H::Entities.GetGroup(EGroupType::PLAYERS_ALL))
	{
		auto pPlayer = pEntity->As<CTFPlayer>();
		if (pPlayer->IsAlive() && !pPlayer->IsAGhost())
			m_mHealthStorage[pPlayer->entindex()] = pPlayer->m_iHealth();
	}
}

/*
MAKE_SIGNATURE(CTFGameStats_FindPlayerStats, "server.dll", "4C 8B C1 48 85 D2 75", 0x0);
MAKE_SIGNATURE(GetServerAnimating, "server.dll", "48 83 EC ? 8B D1 85 C9 7E ? 48 8B 05", 0x0);

static void* pCTFGameStats = nullptr;
MAKE_HOOK(CTFGameStats_FindPlayerStats, S::CTFGameStats_FindPlayerStats(), void*,
	void* rcx, CBasePlayer* pPlayer)
{
	pCTFGameStats = rcx;
	return CALL_ORIGINAL(rcx, pPlayer);
}
*/

void CCritHack::Draw(CTFPlayer* pLocal)
{
	if (!(Vars::Menu::Indicators.Value & Vars::Menu::IndicatorsEnum::CritHack) || !I::EngineClient->IsInGame())
		return;

	auto pWeapon = H::Entities.GetWeapon();
	if (!pWeapon || !pLocal->IsAlive() || pLocal->IsAGhost())
		return;

	int x = Vars::Menu::CritsDisplay.Value.x;
	int y = Vars::Menu::CritsDisplay.Value.y + 8;
	const auto& fFont = H::Fonts.GetFont(FONT_INDICATORS);
	const int nTall = fFont.m_nTall + H::Draw.Scale(1);

	EAlign align = ALIGN_TOP;
	if (x <= 100 + H::Draw.Scale(50, Scale_Round))
	{
		x -= H::Draw.Scale(42, Scale_Round);
		align = ALIGN_TOPLEFT;
	}
	else if (x >= H::Draw.m_nScreenW - 100 - H::Draw.Scale(50, Scale_Round))
	{
		x += H::Draw.Scale(42, Scale_Round);
		align = ALIGN_TOPRIGHT;
	}

	auto iSlot = pWeapon->GetSlot();
	if (!m_mStorage.contains(iSlot))
	{
		if (WeaponCanCrit(pWeapon, true))
			H::Draw.StringOutlined(fFont, x, y, Vars::Colors::IndicatorTextBad.Value, Vars::Menu::Theme::Background.Value, align, "Random crits disabled");
	}
	else
	{
		y -= nTall;

		auto& tStorage = m_mStorage[iSlot];
		if (!tStorage.m_bActive)
			return;

		auto bRapidFire = pWeapon->IsRapidFire();
		float flTickBase = TICKS_TO_TIME(pLocal->m_nTickBase());

		if (Vars::Misc::Game::AntiCheatCompatibility.Value)
			H::Draw.StringOutlined(fFont, x, y += nTall, Vars::Colors::IndicatorTextBad.Value, Vars::Menu::Theme::Background.Value, align, "Anticheat compatibility");

		if (tStorage.m_flDamage > 0)
		{
			if (pLocal->IsCritBoosted())
				H::Draw.StringOutlined(fFont, x, y += nTall, Vars::Colors::IndicatorTextMisc.Value, Vars::Menu::Theme::Background.Value, align, "Crit Boosted");
			else if (pWeapon->m_flCritTime() > flTickBase)
			{
				float flTime = pWeapon->m_flCritTime() - flTickBase;
				H::Draw.StringOutlined(fFont, x, y += nTall, Vars::Colors::IndicatorTextMisc.Value, Vars::Menu::Theme::Background.Value, align, std::format("Streaming crits {:.1f}s", flTime).c_str());
			}
			else if (!m_bCritBanned)
			{
				if (tStorage.m_iAvailableCrits > 0)
				{
					if (bRapidFire && flTickBase < pWeapon->m_flLastRapidFireCritCheckTime() + 1.f)
					{
						float flTime = pWeapon->m_flLastRapidFireCritCheckTime() + 1.f - flTickBase;
						H::Draw.StringOutlined(fFont, x, y += nTall, Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value, align, std::format("Wait {:.1f}s", flTime).c_str());
					}
					else
						H::Draw.StringOutlined(fFont, x, y += nTall, Vars::Colors::IndicatorTextGood.Value, Vars::Menu::Theme::Background.Value, align, "Crit Ready");
				}
				else
				{
					int iShots = tStorage.m_iNextCrit;
					H::Draw.StringOutlined(fFont, x, y += nTall, Vars::Colors::IndicatorTextBad.Value, Vars::Menu::Theme::Background.Value, align, std::format("Crit in {}{} shot{}", iShots, iShots == 1000 ? "+" : "", iShots == 1 ? "" : "s").c_str());
				}
			}
			else
				H::Draw.StringOutlined(fFont, x, y += nTall, Vars::Colors::IndicatorTextBad.Value, Vars::Menu::Theme::Background.Value, align, std::format("Deal {} damage", m_iDamageTilUnban).c_str());

			int iCrits = tStorage.m_iAvailableCrits;
			H::Draw.StringOutlined(fFont, x, y += nTall, Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value, align, std::format("{}{} / {} potential crits", iCrits, iCrits == 1000 ? "+" : "", tStorage.m_iPotentialCrits).c_str());

			if (iCrits && tStorage.m_iNextCrit)
			{
				int iShots = tStorage.m_iNextCrit;
				H::Draw.StringOutlined(fFont, x, y += nTall, Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value, align, std::format("Next in {}{} shot{}", iShots, iShots == 1000 ? "+" : "", iShots == 1 ? "" : "s").c_str());
			}
		}
		else
			H::Draw.StringOutlined(fFont, x, y += nTall, Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value, align, "Calculating");

		if (Vars::Debug::Info.Value)
		{
			H::Draw.StringOutlined(fFont, x, y += nTall * 2, Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value, align, std::format("AllDamage: {}, CritDamage: {}", m_iAllDamage, m_iCritDamage).c_str());
			H::Draw.StringOutlined(fFont, x, y += nTall, Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value, align, std::format("Bucket: {}, Shots: {}, Crits: {}", pWeapon->m_flCritTokenBucket(), pWeapon->m_nCritChecks(), pWeapon->m_nCritSeedRequests()).c_str());
			H::Draw.StringOutlined(fFont, x, y += nTall, Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value, align, std::format("Damage: {}, Cost: {}", tStorage.m_flDamage, tStorage.m_flCost).c_str());
			H::Draw.StringOutlined(fFont, x, y += nTall, Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value, align, std::format("CritChance: {:.2f} ({:.2f})", m_flCritChance, m_flCritChance + 0.1f).c_str());
			H::Draw.StringOutlined(fFont, x, y += nTall, Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value, align, std::format("Force: {}, Skip: {}", tStorage.m_vCritCommands.size(), tStorage.m_vSkipCommands.size()).c_str());

			/*
			if (pCTFGameStats)
			{
				if (auto pServerAnimating = S::GetServerAnimating.Call<void*>(pLocal->entindex()))
				{
					if (void* pPlayerStats = S::CTFGameStats_FindPlayerStats.Call<void*>(pCTFGameStats, pServerAnimating))
					{
						H::Draw.StringOutlined(fFont, x, y += nTall * 2, Vars::Menu::Theme::Active.Value, Vars::Menu::Theme::Background.Value, align,
							std::format(
								"AllDamage: {}, CritDamage: {}",
								*reinterpret_cast<int*>(uintptr_t(pPlayerStats) + 324), *reinterpret_cast<int*>(uintptr_t(pPlayerStats) + 328)
							).c_str()
						);
					}
				}
			}
			*/
		}
	}
}