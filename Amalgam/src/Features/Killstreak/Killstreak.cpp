#include "Killstreak.h"

// It only works on the hud kisstreak count for some reason
// Might be related to custom kill message stuff (?)

int CKillstreak::GetCurrentStreak()
{
	return m_iCurrentKillstreak;
}

void CKillstreak::ApplyKillstreak()
{
	if (const auto& pLocal = H::Entities.GetLocal())
	{
		if (const auto& pPR = H::Entities.GetPR())
		{
			const auto streaksResource = pPR->GetStreaks(I::EngineClient->GetLocalPlayer());
			if (streaksResource && *streaksResource != GetCurrentStreak())
			{
				streaksResource[kTFStreak_Kills] = GetCurrentStreak();
				streaksResource[kTFStreak_KillsAll] = GetCurrentStreak();
				//streaksResource[kTFStreak_Ducks] = GetCurrentStreak();
				//streaksResource[kTFStreak_Duck_levelup] = GetCurrentStreak();
			}

			pLocal->m_nStreaks(kTFStreak_Kills) = GetCurrentStreak();
			pLocal->m_nStreaks(kTFStreak_KillsAll) = GetCurrentStreak();
			//pLocal->m_nStreaks(kTFStreak_Ducks) = GetCurrentStreak();
			//pLocal->m_nStreaks(kTFStreak_Duck_levelup) = GetCurrentStreak();
		}
	}
}

void CKillstreak::PlayerDeath(IGameEvent* pEvent)
{
	if (!Vars::Visuals::Misc::KillstreakWeapons.Value)
		return;

	const int attacker = I::EngineClient->GetPlayerForUserID(pEvent->GetInt("attacker"));
	const int userid = I::EngineClient->GetPlayerForUserID(pEvent->GetInt("userid"));

	if (userid == I::EngineClient->GetLocalPlayer())
	{
		Reset();
		return;
	}

	auto pLocal = H::Entities.GetLocal();
	if (attacker != I::EngineClient->GetLocalPlayer() ||
		attacker == userid ||
		!pLocal || !pLocal->IsAlive())
		return;

	const auto wepID = pEvent->GetInt("weaponid");

	m_iCurrentKillstreak++;
	m_mKillstreakMap[wepID]++;

	pEvent->SetInt("kill_streak_total", GetCurrentStreak());
	pEvent->SetInt("kill_streak_wep", m_mKillstreakMap[wepID]);

	ApplyKillstreak();
}

void CKillstreak::PlayerSpawn(IGameEvent* pEvent)
{
	if (!Vars::Visuals::Misc::KillstreakWeapons.Value)
		return;

	const int userid = I::EngineClient->GetPlayerForUserID(pEvent->GetInt("userid"));

	if (userid == I::EngineClient->GetLocalPlayer())
		Reset();

	ApplyKillstreak();
}

void CKillstreak::Reset()
{
	m_iCurrentKillstreak = 0;
	m_mKillstreakMap.clear();
}
