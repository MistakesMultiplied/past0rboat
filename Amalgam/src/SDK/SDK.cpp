#include "SDK.h"

#include "../Features/EnginePrediction/EnginePrediction.h"
#include "../Features/Visuals/Notifications/Notifications.h"
#include "../Features/ImGui/Menu/Menu.h"
#include "../Features/Configs/Configs.h"
#include <random>
#include <fstream>

#pragma warning (disable : 6385)

static BOOL CALLBACK TeamFortressWindow(HWND hWindow, LPARAM lParam)
{
	char windowTitle[1024];
	GetWindowTextA(hWindow, windowTitle, sizeof(windowTitle));
	switch (FNV1A::Hash32(windowTitle))
	{
	case FNV1A::Hash32Const("Team Fortress 2 - Direct3D 9 - 64 Bit"):
	case FNV1A::Hash32Const("Team Fortress 2 - Vulkan - 64 Bit"):
		*reinterpret_cast<HWND*>(lParam) = hWindow;
	}

	return TRUE;
}




void SDK::Output(const char* cFunction, const char* cLog, Color_t tColor,
	bool bConsole, bool bDebug, bool bToast, bool bMenu, bool bChat, bool bParty, int iMessageBox,
	const char* sLeft, const char* sRight)
{
	if (cLog)
	{
		if (bConsole)
		{
			I::CVar->ConsoleColorPrintf(tColor, "%s%s%s ", sLeft, cFunction, sRight);
			I::CVar->ConsoleColorPrintf({}, "%s\n", cLog);
		}
		if (bDebug)
			OutputDebugString(std::format("{}{}{} {}\n", sLeft, cFunction, sRight, cLog).c_str());
		if (bToast)
			F::Notifications.Add(cLog, Vars::Logging::Lifetime.Value, 0.2f, tColor);
		if (bMenu)
			F::Menu.AddOutput(std::format("{}{}{}", sLeft, cFunction, sRight).c_str(), cLog, tColor);
		if (bChat)
			I::ClientModeShared->m_pChatElement->ChatPrintf(0, std::format("{}{}{}{}\x1 {}", tColor.ToHex(), sLeft, cFunction, sRight, cLog).c_str());
		if (bParty)
			I::TFPartyClient->SendPartyChat(cLog);
		if (iMessageBox != -1)
			MessageBox(nullptr, cLog, cFunction, iMessageBox);
	}
	else
	{
		if (bConsole)
			I::CVar->ConsoleColorPrintf(tColor, "%s\n", cFunction);
		if (bDebug)
			OutputDebugString(std::format("{}\n", cFunction).c_str());
		if (bToast)
			F::Notifications.Add(cFunction, Vars::Logging::Lifetime.Value, 0.2f, tColor);
		if (bMenu)
			F::Menu.AddOutput("", cFunction, tColor);
		if (bChat)
			I::ClientModeShared->m_pChatElement->ChatPrintf(0, std::format("{}{}\x1", tColor.ToHex(), cFunction).c_str());
		if (bParty)
			I::TFPartyClient->SendPartyChat(cFunction);
		if (iMessageBox != -1)
			MessageBox(nullptr, "", cFunction, iMessageBox);
	}
}

void SDK::SetClipboard(std::string sString)
{
	if (OpenClipboard(nullptr))
	{
		EmptyClipboard();

		if (HGLOBAL hMemory = GlobalAlloc(GMEM_DDESHARE, sString.length() + 1))
		{
			if (void* pData = GlobalLock(hMemory))
			{
				memset(pData, 0, sString.length() + 1);
				memcpy(pData, sString.c_str(), sString.length());
				GlobalUnlock(hMemory);
				SetClipboardData(CF_TEXT, hMemory);
			}
		}

		CloseClipboard();
	}
}

std::string SDK::GetClipboard()
{
	std::string sString = "";
	if (OpenClipboard(nullptr))
	{
		if (void* pData = GetClipboardData(CF_TEXT))
			sString = (char*)(pData);

		CloseClipboard();
	}
	return sString;
}

HWND SDK::GetTeamFortressWindow()
{
	static HWND hWindow = nullptr;
	if (!hWindow)
		EnumWindows(TeamFortressWindow, reinterpret_cast<LPARAM>(&hWindow));
	return hWindow;
}

bool SDK::IsGameWindowInFocus()
{
	HWND hWindow = GetTeamFortressWindow();
	return hWindow == GetForegroundWindow() || !hWindow;
}

std::wstring SDK::ConvertUtf8ToWide(const std::string& source)
{
	int size = MultiByteToWideChar(CP_UTF8, 0, source.data(), -1, nullptr, 0);
	std::wstring result(size, 0);
	MultiByteToWideChar(CP_UTF8, 0, source.data(), -1, result.data(), size);
	return result;
}

std::string SDK::ConvertWideToUTF8(const std::wstring& source)
{
	int size = WideCharToMultiByte(CP_UTF8, 0, source.data(), -1, nullptr, 0, nullptr, nullptr);
	std::string result(size, 0);
	WideCharToMultiByte(CP_UTF8, 0, source.data(), -1, result.data(), size, nullptr, nullptr);
	return result;
}

double SDK::PlatFloatTime()
{
	static auto Plat_FloatTime = U::Memory.GetModuleExport<double(*)()>("tier0.dll", "Plat_FloatTime");
	return Plat_FloatTime();
}

int SDK::StdRandomInt(int min, int max)
{
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> distr(min, max);
	return distr(gen);
}

float SDK::StdRandomFloat(float min, float max)
{
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_real_distribution<float> distr(min, max);
	return distr(gen);
}

int SDK::SeedFileLineHash(int seedvalue, const char* sharedname, int additionalSeed)
{
	CRC32_t retval;

	CRC32_Init(&retval);

	CRC32_ProcessBuffer(&retval, &seedvalue, sizeof(int));
	CRC32_ProcessBuffer(&retval, &additionalSeed, sizeof(int));
	CRC32_ProcessBuffer(&retval, sharedname, int(strlen(sharedname)));

	CRC32_Final(&retval);

	return static_cast<int>(retval);
}

int SDK::SharedRandomInt(unsigned iseed, const char* sharedname, int iMinVal, int iMaxVal, int additionalSeed)
{
	const int seed = SeedFileLineHash(iseed, sharedname, additionalSeed);
	I::UniformRandomStream->SetSeed(seed);
	return I::UniformRandomStream->RandomInt(iMinVal, iMaxVal);
}

void SDK::RandomSeed(int iSeed)
{
	static auto RandomSeed = U::Memory.GetModuleExport<void(*)(uint32_t)>("vstdlib.dll", "RandomSeed");
	RandomSeed(iSeed);
}

int SDK::RandomInt(int iMinVal, int iMaxVal)
{
	static auto RandomInt = U::Memory.GetModuleExport<int(*)(int, int)>("vstdlib.dll", "RandomInt");
	return RandomInt(iMinVal, iMaxVal);
}

float SDK::RandomFloat(float flMinVal, float flMaxVal)
{
	static auto RandomFloat = U::Memory.GetModuleExport<float(*)(float, float)>("vstdlib.dll", "RandomFloat");
	return RandomFloat(flMinVal, flMaxVal);
}

int SDK::HandleToIDX(unsigned int pHandle)
{
	return pHandle & 0xFFF;
}

bool SDK::W2S(const Vec3& vOrigin, Vec3& vScreen, bool bAlways)
{
	const auto& worldToScreen = H::Draw.m_WorldToProjection.As3x4();

	float flW = worldToScreen[3][0] * vOrigin.x + worldToScreen[3][1] * vOrigin.y + worldToScreen[3][2] * vOrigin.z + worldToScreen[3][3];
	vScreen.z = 0;

	bool bOnScreen = flW > 0.f;
	if (bAlways || bOnScreen)
	{
		const float fl1DBw = 1 / fabs(flW);
		vScreen.x = (H::Draw.m_nScreenW / 2.f) + ((worldToScreen[0][0] * vOrigin.x + worldToScreen[0][1] * vOrigin.y + worldToScreen[0][2] * vOrigin.z + worldToScreen[0][3]) * fl1DBw) * H::Draw.m_nScreenW / 2 + 0.5f;
		vScreen.y = (H::Draw.m_nScreenH / 2.f) - ((worldToScreen[1][0] * vOrigin.x + worldToScreen[1][1] * vOrigin.y + worldToScreen[1][2] * vOrigin.z + worldToScreen[1][3]) * fl1DBw) * H::Draw.m_nScreenH / 2 + 0.5f;
	}

	return bOnScreen;
}

bool SDK::IsOnScreen(CBaseEntity* pEntity, const matrix3x4& transform, float* pLeft, float* pRight, float* pTop, float* pBottom)
{
	Vec3 vMins = pEntity->m_vecMins(), vMaxs = pEntity->m_vecMaxs();

	bool bInit = false;
	float flLeft = 0.f, flRight = 0.f, flTop = 0.f, flBottom = 0.f;

	const Vec3 vPoints[] = {
		Vec3(0.f, 0.f, vMins.z),
		Vec3(0.f, 0.f, vMaxs.z),
		Vec3(vMins.x, vMins.y, (vMins.z + vMaxs.z) * 0.5f),
		Vec3(vMins.x, vMaxs.y, (vMins.z + vMaxs.z) * 0.5f),
		Vec3(vMaxs.x, vMins.y, (vMins.z + vMaxs.z) * 0.5f),
		Vec3(vMaxs.x, vMaxs.y, (vMins.z + vMaxs.z) * 0.5f)
	};
	for (int n = 0; n < 6; n++)
	{
		Vec3 vPoint; Math::VectorTransform(vPoints[n], transform, vPoint);

		Vec3 vScreenPos;
		if (!W2S(vPoint, vScreenPos))
			continue;

		flLeft = bInit ? std::min(flLeft, vScreenPos.x) : vScreenPos.x;
		flRight = bInit ? std::max(flRight, vScreenPos.x) : vScreenPos.x;
		flTop = bInit ? std::max(flTop, vScreenPos.y) : vScreenPos.y;
		flBottom = bInit ? std::min(flBottom, vScreenPos.y) : vScreenPos.y;
		bInit = true;
	}

	if (!bInit)
		return false;

	if (pLeft) *pLeft = flLeft;
	if (pRight) *pRight = flRight;
	if (pTop) *pTop = flTop;
	if (pBottom) *pBottom = flBottom;

	return !(flRight < 0 || flLeft > H::Draw.m_nScreenW || flTop < 0 || flBottom > H::Draw.m_nScreenH);
}

bool SDK::IsOnScreen(CBaseEntity* pEntity, Vec3 vOrigin)
{
	Vec3 vMins = pEntity->m_vecMins(), vMaxs = pEntity->m_vecMaxs();

	bool bInit = false;
	float flLeft = 0.f, flRight = 0.f, flTop = 0.f, flBottom = 0.f;

	const Vec3 vPoints[] = {
		Vec3(0.f, 0.f, vMins.z),
		Vec3(0.f, 0.f, vMaxs.z),
		Vec3(vMins.x, vMins.y, (vMins.z + vMaxs.z) * 0.5f),
		Vec3(vMins.x, vMaxs.y, (vMins.z + vMaxs.z) * 0.5f),
		Vec3(vMaxs.x, vMins.y, (vMins.z + vMaxs.z) * 0.5f),
		Vec3(vMaxs.x, vMaxs.y, (vMins.z + vMaxs.z) * 0.5f)
	};
	for (int n = 0; n < 6; n++)
	{
		Vec3 vPoint = vOrigin + vPoints[n];

		Vec3 vScreenPos;
		if (!W2S(vPoint, vScreenPos))
			continue;

		flLeft = bInit ? std::min(flLeft, vScreenPos.x) : vScreenPos.x;
		flRight = bInit ? std::max(flRight, vScreenPos.x) : vScreenPos.x;
		flTop = bInit ? std::max(flTop, vScreenPos.y) : vScreenPos.y;
		flBottom = bInit ? std::min(flBottom, vScreenPos.y) : vScreenPos.y;
		bInit = true;
	}

	if (!bInit)
		return false;

	return !(flRight < 0 || flLeft > H::Draw.m_nScreenW || flTop < 0 || flBottom > H::Draw.m_nScreenH);
}

bool SDK::IsOnScreen(CBaseEntity* pEntity, bool bShouldGetOwner)
{
	if (bShouldGetOwner)
	{
		if (auto pOwner = pEntity->m_hOwnerEntity().Get())
			pEntity = pOwner;
	}

	return IsOnScreen(pEntity, pEntity->entindex() == I::EngineClient->GetLocalPlayer() && !I::EngineClient->IsPlayingDemo() ? F::EnginePrediction.m_vOrigin : pEntity->GetAbsOrigin());
}

void SDK::Trace(const Vec3& vecStart, const Vec3& vecEnd, unsigned int nMask, ITraceFilter* pFilter, CGameTrace* pTrace)
{
	Ray_t ray;
	ray.Init(vecStart, vecEnd);
	I::EngineTrace->TraceRay(ray, nMask, pFilter, pTrace);

#ifdef DEBUG_TRACES
	if (Vars::Debug::VisualizeTraces.Value)
		G::LineStorage.emplace_back(std::pair<Vec3, Vec3>(vecStart, Vars::Debug::VisualizeTraceHits.Value ? pTrace->endpos : vecEnd), I::GlobalVars->curtime + 0.015f, Color_t(), bool(GetAsyncKeyState(VK_MENU) & 0x8000));
#endif
}

void SDK::TraceHull(const Vec3& vecStart, const Vec3& vecEnd, const Vec3& vecHullMin, const Vec3& vecHullMax, unsigned int nMask, ITraceFilter* pFilter, CGameTrace* pTrace)
{
	Ray_t ray;
	ray.Init(vecStart, vecEnd, vecHullMin, vecHullMax);
	I::EngineTrace->TraceRay(ray, nMask, pFilter, pTrace);

#ifdef DEBUG_TRACES
	if (Vars::Debug::VisualizeTraces.Value)
	{
		G::LineStorage.emplace_back(std::pair<Vec3, Vec3>(vecStart, Vars::Debug::VisualizeTraceHits.Value ? pTrace->endpos : vecEnd), I::GlobalVars->curtime + 0.015f, Color_t(), bool(GetAsyncKeyState(VK_MENU) & 0x8000));
		if (!(vecHullMax - vecHullMin).IsZero())
		{
			G::BoxStorage.emplace_back(vecStart, vecHullMin, vecHullMax, Vec3(), I::GlobalVars->curtime + 0.015f, Color_t(), Color_t(0, 0, 0, 0), bool(GetAsyncKeyState(VK_MENU) & 0x8000));
			G::BoxStorage.emplace_back(Vars::Debug::VisualizeTraceHits.Value ? pTrace->endpos : vecEnd, vecHullMin, vecHullMax, Vec3(), I::GlobalVars->curtime + 0.015f, Color_t(), Color_t(0, 0, 0, 0), bool(GetAsyncKeyState(VK_MENU) & 0x8000));
		}
	}
#endif
}

bool SDK::VisPos(CBaseEntity* pSkip, const CBaseEntity* pEntity, const Vec3& from, const Vec3& to, unsigned int nMask)
{
	CGameTrace trace = {};
	CTraceFilterHitscan filter = {}; filter.pSkip = pSkip;
	Trace(from, to, nMask, &filter, &trace);
	if (trace.DidHit())
		return trace.m_pEnt && trace.m_pEnt == pEntity;
	return true;
}
bool SDK::VisPosProjectile(CBaseEntity* pSkip, const CBaseEntity* pEntity, const Vec3& from, const Vec3& to, unsigned int nMask)
{
	CGameTrace trace = {};
	CTraceFilterProjectile filter = {}; filter.pSkip = pSkip;
	Trace(from, to, nMask, &filter, &trace);
	if (trace.DidHit())
		return trace.m_pEnt && trace.m_pEnt == pEntity;
	return true;
}
bool SDK::VisPosWorld(CBaseEntity* pSkip, const CBaseEntity* pEntity, const Vec3& from, const Vec3& to, unsigned int nMask)
{
	CGameTrace trace = {};
	CTraceFilterWorldAndPropsOnly filter = {}; filter.pSkip = pSkip;
	Trace(from, to, nMask, &filter, &trace);
	if (trace.DidHit())
		return trace.m_pEnt && trace.m_pEnt == pEntity;
	return true;
}

int SDK::GetRoundState()
{
	if (auto pGameRules = I::TFGameRules())
		return pGameRules->m_iRoundState();
	return 0;
}

int SDK::GetWinningTeam()
{
	if (auto pGameRules = I::TFGameRules())
		return pGameRules->m_iWinningTeam();
	return 0;
}

EWeaponType SDK::GetWeaponType(CTFWeaponBase* pWeapon, EWeaponType* pSecondaryType)
{
	if (pSecondaryType)
		*pSecondaryType = EWeaponType::UNKNOWN;
	if (!pWeapon)
		return EWeaponType::UNKNOWN;

	if (pSecondaryType)
	{
		switch (pWeapon->GetWeaponID())
		{
		case TF_WEAPON_BAT_WOOD:
		case TF_WEAPON_BAT_GIFTWRAP:
			if (pWeapon->HasPrimaryAmmoForShot())
				*pSecondaryType = EWeaponType::PROJECTILE;
		}
	}

	if (pWeapon->GetSlot() == EWeaponSlot::SLOT_MELEE || pWeapon->GetWeaponID() == TF_WEAPON_BUILDER)
		return EWeaponType::MELEE;

	switch (pWeapon->m_iItemDefinitionIndex())
	{
	case Soldier_s_TheBuffBanner:
	case Soldier_s_FestiveBuffBanner:
	case Soldier_s_TheBattalionsBackup:
	case Soldier_s_TheConcheror:
	case Scout_s_BonkAtomicPunch:
	case Scout_s_CritaCola:
		EWeaponType::UNKNOWN;
	}

	switch (pWeapon->GetWeaponID())
	{
	case TF_WEAPON_PDA:
	case TF_WEAPON_PDA_ENGINEER_BUILD:
	case TF_WEAPON_PDA_ENGINEER_DESTROY:
	case TF_WEAPON_PDA_SPY:
	case TF_WEAPON_PDA_SPY_BUILD:
	case TF_WEAPON_INVIS:
	case TF_WEAPON_BUFF_ITEM:
	case TF_WEAPON_GRAPPLINGHOOK:
	case TF_WEAPON_ROCKETPACK:
		return EWeaponType::UNKNOWN;
	case TF_WEAPON_CLEAVER:
	case TF_WEAPON_ROCKETLAUNCHER:
	case TF_WEAPON_ROCKETLAUNCHER_DIRECTHIT:
	case TF_WEAPON_PARTICLE_CANNON:
	case TF_WEAPON_RAYGUN:
	case TF_WEAPON_FLAMETHROWER:
	case TF_WEAPON_FLAME_BALL:
	case TF_WEAPON_FLAREGUN:
	case TF_WEAPON_FLAREGUN_REVENGE:
	case TF_WEAPON_GRENADELAUNCHER:
	case TF_WEAPON_CANNON:
	case TF_WEAPON_PIPEBOMBLAUNCHER:
	case TF_WEAPON_SHOTGUN_BUILDING_RESCUE:
	case TF_WEAPON_DRG_POMSON:
	case TF_WEAPON_CROSSBOW:
	case TF_WEAPON_SYRINGEGUN_MEDIC:
	case TF_WEAPON_COMPOUND_BOW:
	case TF_WEAPON_JAR:
	case TF_WEAPON_JAR_MILK:
	case TF_WEAPON_JAR_GAS:
	case TF_WEAPON_LUNCHBOX:
		return EWeaponType::PROJECTILE;
	}

	return EWeaponType::HITSCAN;
}

const char* SDK::GetClassByIndex(const int nClass)
{
	static const char* szClasses[] = {
		"unknown",
		"scout",
		"sniper",
		"soldier",
		"demoman",
		"medic",
		"heavy",
		"pyro",
		"spy",
		"engineer"
	};

	return (nClass < 10 && nClass > 0) ? szClasses[nClass] : szClasses[0];
}

int SDK::IsAttacking(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, const CUserCmd* pCmd, bool bTickBase)
{
	if (!pLocal || !pWeapon || pCmd->weaponselect)
		return false;

	int iTickBase = bTickBase ? I::GlobalVars->tickcount : pLocal->m_nTickBase();
	float flTickBase = bTickBase ? I::GlobalVars->curtime : TICKS_TO_TIME(iTickBase);

	if (pWeapon->GetSlot() == SLOT_MELEE)
	{
		switch (pWeapon->GetWeaponID())
		{
		case TF_WEAPON_KNIFE:
			return G::CanPrimaryAttack && pCmd->buttons & IN_ATTACK;
		case TF_WEAPON_BAT_WOOD:
		case TF_WEAPON_BAT_GIFTWRAP:
		{
			static int iThrowTick = 0;
			if (G::CanSecondaryAttack && pWeapon->HasPrimaryAmmoForShot() && pCmd->buttons & IN_ATTACK2 && iThrowTick + 5 < iTickBase)
				iThrowTick = iTickBase + 11;
			if (iThrowTick >= iTickBase)
				G::CanPrimaryAttack = G::CanSecondaryAttack = G::Throwing = true;
			if (iThrowTick == iTickBase)
				return true;
		}
		}

		return TIME_TO_TICKS(pWeapon->m_flSmackTime()) == iTickBase - 1;
	}

	const int iWeaponID = pWeapon->GetWeaponID();
	switch (iWeaponID)
	{
	case TF_WEAPON_COMPOUND_BOW:
		return !(pCmd->buttons & IN_ATTACK) && pWeapon->As<CTFPipebombLauncher>()->m_flChargeBeginTime() > 0.f;
	case TF_WEAPON_PIPEBOMBLAUNCHER:
	case TF_WEAPON_STICKY_BALL_LAUNCHER:
	case TF_WEAPON_GRENADE_STICKY_BALL:
	{
		float flCharge = pWeapon->As<CTFPipebombLauncher>()->m_flChargeBeginTime() > 0.f ? flTickBase - pWeapon->As<CTFPipebombLauncher>()->m_flChargeBeginTime() : 0.f;
		const float flAmount = Math::RemapVal(flCharge, 0.f, SDK::AttribHookValue(4.f, "stickybomb_charge_rate", pWeapon), 0.f, 1.f);
		return !(pCmd->buttons & IN_ATTACK) && flAmount > 0.f || flAmount == 1.f;
	}
	case TF_WEAPON_CANNON:
	{
		float flMortar = SDK::AttribHookValue(0.f, "grenade_launcher_mortar_mode", pWeapon);
		if (!flMortar)
			return G::CanPrimaryAttack && pCmd->buttons & IN_ATTACK ? 1 : G::Reloading && pCmd->buttons & IN_ATTACK ? 2 : 0;

		float flCharge = pWeapon->As<CTFGrenadeLauncher>()->m_flDetonateTime() > 0.f ? flMortar - (pWeapon->As<CTFGrenadeLauncher>()->m_flDetonateTime() - flTickBase) : 0.f;
		const float flAmount = Math::RemapVal(flCharge, 0.f, SDK::AttribHookValue(0.f, "grenade_launcher_mortar_mode", pWeapon), 0.f, 1.f);
		return !(pCmd->buttons & IN_ATTACK) && flAmount > 0.f || flAmount == 1.f;
	}
	case TF_WEAPON_SNIPERRIFLE_CLASSIC:
		return !(pCmd->buttons & IN_ATTACK) && pWeapon->As<CTFSniperRifle>()->m_flChargedDamage() > 0.f;
	case TF_WEAPON_PARTICLE_CANNON:
	{
		float flChargeBeginTime = pWeapon->As<CTFParticleCannon>()->m_flChargeBeginTime();
		if (flChargeBeginTime > 0)
		{
			float flTotalChargeTime = flTickBase - flChargeBeginTime;
			if (flTotalChargeTime >= TF_PARTICLE_MAX_CHARGE_TIME)
				return 1;
		}
		break;
	}
	case TF_WEAPON_CLEAVER: // we can randomly use attack2 to fire
	{
		static int iThrowTick = 0;
		if (G::CanPrimaryAttack && pWeapon->HasPrimaryAmmoForShot() && pCmd->buttons & (IN_ATTACK | IN_ATTACK2) && iThrowTick < iTickBase)
			iThrowTick = iTickBase + 11;
		if (iThrowTick >= iTickBase)
			G::CanPrimaryAttack = G::Throwing = true;
		return iThrowTick == iTickBase;
	}
	case TF_WEAPON_JAR:
	case TF_WEAPON_JAR_MILK:
	case TF_WEAPON_JAR_GAS:
	{
		static int iThrowTick = 0;
		if (G::CanPrimaryAttack && pWeapon->HasPrimaryAmmoForShot() && pCmd->buttons & IN_ATTACK && iThrowTick < iTickBase)
			iThrowTick = iTickBase + 11;
		if (iThrowTick >= iTickBase)
			G::CanPrimaryAttack = G::Throwing = true;
		return iThrowTick == iTickBase;
	}
	case TF_WEAPON_GRAPPLINGHOOK:
	{
		if (G::LastUserCmd->buttons & IN_ATTACK || !(pCmd->buttons & IN_ATTACK))
			return false;

		if (pLocal->InCond(TF_COND_GRAPPLINGHOOK))
			return false;

		Vec3 vPos, vAngle; GetProjectileFireSetup(pLocal, pCmd->viewangles, { 23.5f, -8.f, -3.f }, vPos, vAngle, false);
		Vec3 vForward; Math::AngleVectors(vAngle, &vForward);

		CGameTrace trace = {};
		CTraceFilterHitscan filter = {}; filter.pSkip = pLocal;
		static auto tf_grapplinghook_max_distance = U::ConVars.FindVar("tf_grapplinghook_max_distance");
		const float flGrappleDistance = tf_grapplinghook_max_distance ? tf_grapplinghook_max_distance->GetFloat() : 2000.f;
		Trace(vPos, vPos + vForward * flGrappleDistance, MASK_SOLID, &filter, &trace);
		return trace.DidHit() && !(trace.surface.flags & SURF_SKY);
	}
	case TF_WEAPON_MINIGUN:
		switch (pWeapon->As<CTFMinigun>()->m_iWeaponState())
		{
		case AC_STATE_FIRING:
		case AC_STATE_SPINNING:
			if (pWeapon->HasPrimaryAmmoForShot())
				return G::CanPrimaryAttack && pCmd->buttons & IN_ATTACK ? 1 : G::Reloading && pCmd->buttons & IN_ATTACK ? 2 : 0;
		}
		return false;
	case TF_WEAPON_LUNCHBOX:
		if (G::PrimaryWeaponType == EWeaponType::PROJECTILE && G::CanSecondaryAttack && pWeapon->HasPrimaryAmmoForShot() && pCmd->buttons & IN_ATTACK2)
			return 1;
		return false;
	case TF_WEAPON_FLAMETHROWER:
	case TF_WEAPON_FLAME_BALL:
	case TF_WEAPON_MECHANICAL_ARM:
		if (G::CanSecondaryAttack && pCmd->buttons & IN_ATTACK2)
			return 1;
		break;
	}

	if (pWeapon->m_iItemDefinitionIndex() == Soldier_m_TheBeggarsBazooka)
	{
		static bool bLoading = false, bFiring = false;

		bool bAmmo = pWeapon->HasPrimaryAmmoForShot();
		if (!bAmmo)
			bLoading = false,
			bFiring = false;
		else if (!bFiring)
			bLoading = true;

		if ((bFiring || bLoading && !(pCmd->buttons & IN_ATTACK)) && bAmmo)
		{
			bFiring = true;
			bLoading = false;
			return G::CanPrimaryAttack ? 1 : G::Reloading ? 2 : 0;
		}

		return false;
	}

	return G::CanPrimaryAttack && pCmd->buttons & IN_ATTACK ? 1 : G::Reloading && pCmd->buttons & IN_ATTACK ? 2 : 0;
}

float SDK::MaxSpeed(CTFPlayer* pPlayer, bool bIncludeCrouch, bool bIgnoreSpecialAbility)
{
	float flSpeed = pPlayer->TeamFortress_CalculateMaxSpeed(bIgnoreSpecialAbility);

	if (pPlayer->InCond(TF_COND_SPEED_BOOST) ||
		pPlayer->InCond(TF_COND_HALLOWEEN_SPEED_BOOST))
		flSpeed *= 1.35f;
	if (bIncludeCrouch && pPlayer->IsDucking() && pPlayer->IsOnGround())
		flSpeed /= 3;

	return flSpeed;
}

void SDK::FixMovement(CUserCmd* pCmd, const Vec3& vCurAngle, const Vec3& vTargetAngle)
{
	bool bCurOOB = fabsf(Math::NormalizeAngle(vCurAngle.x)) > 90.f;
	bool bTargetOOB = fabsf(Math::NormalizeAngle(vTargetAngle.x)) > 90.f;

	Vec3 vMove = { pCmd->forwardmove, pCmd->sidemove * (bCurOOB ? -1 : 1), pCmd->upmove };
	float flSpeed = vMove.Length2D();
	Vec3 vMoveAng = {}; Math::VectorAngles(vMove, vMoveAng);

	float flCurYaw = vCurAngle.y + (bCurOOB ? 180.f : 0.f);
	float flTargetYaw = vTargetAngle.y + (bTargetOOB ? 180.f : 0.f);
	float flYaw = DEG2RAD(flTargetYaw - flCurYaw + vMoveAng.y);

	pCmd->forwardmove = cos(flYaw) * flSpeed;
	pCmd->sidemove = sin(flYaw) * flSpeed * (bTargetOOB ? -1 : 1);
}

void SDK::FixMovement(CUserCmd* pCmd, const Vec3& vTargetAngle)
{
	FixMovement(pCmd, pCmd->viewangles, vTargetAngle);
}

bool SDK::StopMovement(CTFPlayer* pLocal, CUserCmd* pCmd)
{
	if (pLocal->m_vecVelocity().IsZero())
	{
		pCmd->forwardmove = 0.f;
		pCmd->sidemove = 0.f;
		return false;
	}

	if (G::Attacking != 1)
	{
		float flDirection = Math::VelocityToAngles(pLocal->m_vecVelocity() * -1).y;
		pCmd->viewangles = { 90, flDirection, 0 };
		pCmd->sidemove = 0; pCmd->forwardmove = 0;
		return true;
	}
	else
	{
		Vec3 vDirection = pLocal->m_vecVelocity().ToAngle();
		vDirection.y = pCmd->viewangles.y - vDirection.y;
		Vec3 vNegatedDirection = vDirection.FromAngle() * -pLocal->m_vecVelocity().Length2D();
		pCmd->forwardmove = vNegatedDirection.x;
		pCmd->sidemove = vNegatedDirection.y;
		return false;
	}
}

Vec3 SDK::ComputeMove(const CUserCmd* pCmd, CTFPlayer* pLocal, Vec3& vFrom, Vec3& vTo)
{
	const Vec3 vDiff = vTo - vFrom;
	if (!vDiff.Length())
		return {};

	Vec3 vSilent = { vDiff.x, vDiff.y, 0 };
	Vec3 vAngle; Math::VectorAngles(vSilent, vAngle);
	const float flYaw = DEG2RAD(vAngle.y - pCmd->viewangles.y);
	const float flPitch = DEG2RAD(vAngle.x - pCmd->viewangles.x);

	Vec3 vMove = { cos(flYaw) * 450.f, -sin(flYaw) * 450.f, -cos(flPitch) * 450.f };
	if (!(I::EngineTrace->GetPointContents(pLocal->GetShootPos()) & CONTENTS_WATER)) // only apply upmove in water
		vMove.z = pCmd->upmove;

	return vMove;
}

void SDK::WalkTo(CUserCmd* pCmd, CTFPlayer* pLocal, Vec3& vFrom, Vec3& vTo, float flScale)
{
	const auto vResult = ComputeMove(pCmd, pLocal, vFrom, vTo);

	pCmd->forwardmove = vResult.x * flScale;
	pCmd->sidemove = vResult.y * flScale;
	pCmd->upmove = vResult.z * flScale;
}

void SDK::WalkTo(CUserCmd* pCmd, CTFPlayer* pLocal, Vec3 vTo, float flScale)
{
	Vec3 vLocalPos = pLocal->m_vecOrigin();
	WalkTo(pCmd, pLocal, vLocalPos, vTo, flScale);
}

void SDK::WalkToFixAntiAim(CUserCmd* pCmd, const Vec3& vTargetAngle)
{
	// This special function specifically preserves NavBot's movement direction
	// while still applying the necessary angle corrections for AntiAim
	
	float flOriginalForward = pCmd->forwardmove;
	float flOriginalSide = pCmd->sidemove;
	
	if (flOriginalForward == 0.0f && flOriginalSide == 0.0f)
		return;
	
	float flOriginalYaw = DEG2RAD(pCmd->viewangles.y);
	float flNewYaw = DEG2RAD(vTargetAngle.y);

	pCmd->forwardmove = (flOriginalForward * cos(flNewYaw - flOriginalYaw)) + (flOriginalSide * sin(flNewYaw - flOriginalYaw));
	pCmd->sidemove = (flOriginalSide * cos(flNewYaw - flOriginalYaw)) - (flOriginalForward * sin(flNewYaw - flOriginalYaw));
	
	bool bCurPitchFlipped = fabsf(Math::NormalizeAngle(pCmd->viewangles.x)) > 90.f;
	bool bTargetPitchFlipped = fabsf(Math::NormalizeAngle(vTargetAngle.x)) > 90.f;
	
	if (bCurPitchFlipped != bTargetPitchFlipped)
	{
		pCmd->forwardmove *= -1.0f;
	}
}

class CTraceFilterSetup : public ITraceFilter // trace filter solely for GetProjectileFireSetup
{
public:
	bool ShouldHitEntity(IHandleEntity* pServerEntity, int nContentsMask) override;
	TraceType_t GetTraceType() const override;
	CBaseEntity* pSkip = nullptr;
};
bool CTraceFilterSetup::ShouldHitEntity(IHandleEntity* pServerEntity, int nContentsMask)
{
	if (!pServerEntity || pServerEntity == pSkip)
		return false;

	auto pEntity = reinterpret_cast<CBaseEntity*>(pServerEntity);

	switch (pEntity->GetClassID())
	{
	case ETFClassID::CTFAmmoPack:
	case ETFClassID::CFuncAreaPortalWindow:
	case ETFClassID::CFuncRespawnRoomVisualizer:
	case ETFClassID::CTFReviveMarker: return false;
	case ETFClassID::CTFMedigunShield:
	case ETFClassID::CObjectSentrygun:
	case ETFClassID::CObjectDispenser:
	case ETFClassID::CObjectTeleporter: return true;
	case ETFClassID::CTFPlayer:
	{
		auto pLocal = H::Entities.GetLocal();
		const int iTargetTeam = pEntity->m_iTeamNum(), iLocalTeam = pLocal ? pLocal->m_iTeamNum() : iTargetTeam;
		return iTargetTeam != iLocalTeam;
	}
	}

	return true;
}
TraceType_t CTraceFilterSetup::GetTraceType() const
{
	return TRACE_EVERYTHING;
}

void SDK::GetProjectileFireSetup(CTFPlayer* pPlayer, const Vec3& vAngIn, Vec3 vOffset, Vec3& vPosOut, Vec3& vAngOut, bool bPipes, bool bInterp)
{
	static auto cl_flipviewmodels = U::ConVars.FindVar("cl_flipviewmodels");
	if (cl_flipviewmodels && cl_flipviewmodels->GetBool())
		vOffset.y *= -1.f;

	const Vec3 vShootPos = bInterp ? pPlayer->GetEyePosition() : pPlayer->GetShootPos();

	Vec3 vForward, vRight, vUp; Math::AngleVectors(vAngIn, &vForward, &vRight, &vUp);
	vPosOut = vShootPos + (vForward * vOffset.x) + (vRight * vOffset.y) + (vUp * vOffset.z);

	if (bPipes)
		vAngOut = vAngIn;
	else
	{
		Vec3 vEndPos = vShootPos + (vForward * 2000.f);

		CGameTrace trace = {};
		CTraceFilterSetup filter = {};
		Trace(vShootPos, vEndPos, MASK_SOLID, &filter, &trace);
		if (trace.DidHit() && trace.fraction > 0.1f)
			vEndPos = trace.endpos;

		Math::VectorAngles(vEndPos - vPosOut, vAngOut);
	}
}

void SDK::GetProjectileFireSetupAirblast(CTFPlayer* pPlayer, const Vec3& vAngIn, Vec3 vPosIn, Vec3& vAngOut, bool bInterp)
{
	const Vec3 vShootPos = bInterp ? pPlayer->GetEyePosition() : pPlayer->GetShootPos();

	Vec3 vForward; Math::AngleVectors(vAngIn, &vForward);

	Vec3 vEndPos = vShootPos + (vForward * MAX_TRACE_LENGTH);
	CGameTrace trace = {};
	CTraceFilterWorldAndPropsOnly filter = {};
	Trace(vShootPos, vEndPos, MASK_SOLID, &filter, &trace);

	Math::VectorAngles(trace.endpos - vPosIn, vAngOut);
}

//Pasted from somewhere in the valves tf2 server code
float SDK::CalculateSplashRadiusDamageFalloff(CTFWeaponBase* pWeapon, CTFPlayer* pAttacker, CTFWeaponBaseGrenadeProj* pProjectile, float flRadius)
{
	float flFalloff{ 0.0f };
	const int dmgType = pProjectile->GetDamageType();

	if (dmgType & DMG_RADIUS_MAX)
		flFalloff = 0.0f;
	else if (dmgType & DMG_HALF_FALLOFF)
		flFalloff = 0.5f;
	else if (flRadius)
		flFalloff = pProjectile->m_flDamage() / flRadius;
	else
		flFalloff = 1.0f;

	if (pWeapon)
	{
		float flFalloffMod = 1.f;
		AttribHookValue(flFalloffMod, "mult_dmg_falloff", pWeapon);
		if (flFalloffMod != 1.f)
			flFalloff += flFalloffMod;
	}

	if (pAttacker && pAttacker->InCond(TF_COND_RUNE_PRECISION))
		flFalloff = 1.0f;
	return flFalloff;
}

float SDK::CalculateSplashRadiusDamage(CTFWeaponBase* pWeapon, CTFPlayer* pAttacker, CTFWeaponBaseGrenadeProj* pProjectile, float flRadius, float flDist, float& flDamageNoBuffs, bool bSelf)
{
	float flFalloff{ CalculateSplashRadiusDamageFalloff(pWeapon, pAttacker, pProjectile, flRadius) };
	float flDamage{ Math::RemapVal(flDist, 0.f, flRadius, pProjectile->m_flDamage(), pProjectile->m_flDamage() * flFalloff) };
	flDamageNoBuffs = flDamage;

	bool bCrit{ (pProjectile->GetDamageType() & DMG_CRITICAL) > 0 };
	if (bCrit || pAttacker->IsMiniCritBoosted())
	{
		float flDamageBonus{ 0.f };
		if (bCrit)
		{
			flDamageBonus = (TF_DAMAGE_CRIT_MULTIPLIER - 1.f) * flDamage;
		}
		else
		{
			flDamageBonus = (TF_DAMAGE_MINICRIT_MULTIPLIER - 1.f) * flDamage;
		}

		flDamage += flDamageBonus;
	}

	// Grenades & Pipebombs do less damage to ourselves.
	if (bSelf && pWeapon)
	{
		switch (pWeapon->GetWeaponID())
		{
		case TF_WEAPON_PIPEBOMBLAUNCHER:
		case TF_WEAPON_GRENADELAUNCHER:
		case TF_WEAPON_CANNON:
		case TF_WEAPON_STICKBOMB:
			flDamage *= 0.75f;
			flDamageNoBuffs *= 0.75f;
			break;
		}
	}
	return flDamage;
}

bool SDK::WeaponDoesNotUseAmmo(CTFWeaponBase* pWeapon, bool bIncludeInfiniteAmmo)
{
	if (!pWeapon)
		return false;

	if (pWeapon->GetSlot() == SLOT_MELEE)
		return false;

	switch (pWeapon->m_iItemDefinitionIndex())
	{
	case Soldier_s_TheBuffBanner:
	case Soldier_s_FestiveBuffBanner:
	case Soldier_s_TheBattalionsBackup:
	case Soldier_s_TheConcheror:
	case Demoman_s_TheTideTurner:
	case Demoman_s_TheCharginTarge:
	case Demoman_s_TheSplendidScreen:
	case Demoman_s_FestiveTarge:
	case Demoman_m_TheBootlegger:
	case Demoman_m_AliBabasWeeBooties:
	case Engi_s_TheWrangler:
	case Engi_s_FestiveWrangler:
	case Sniper_s_CozyCamper:
	case Sniper_s_DarwinsDangerShield:
	case Sniper_s_TheRazorback:
	case Pyro_s_ThermalThruster: return true;
	default:
	{
		switch (pWeapon->GetWeaponID())
		{
		case TF_WEAPON_PARTICLE_CANNON:
		case TF_WEAPON_RAYGUN:
		case TF_WEAPON_DRG_POMSON: return bIncludeInfiniteAmmo;
		case TF_WEAPON_FLAREGUN_REVENGE:
		case TF_WEAPON_MEDIGUN:
		case TF_WEAPON_PDA:
		case TF_WEAPON_PDA_ENGINEER_BUILD:
		case TF_WEAPON_PDA_ENGINEER_DESTROY:
		case TF_WEAPON_PDA_SPY:
		case TF_WEAPON_PDA_SPY_BUILD:
		case TF_WEAPON_BUILDER:
		case TF_WEAPON_LUNCHBOX:
		case TF_WEAPON_THROWABLE:
		case TF_WEAPON_JAR:
		case TF_WEAPON_JAR_GAS:
		case TF_WEAPON_JAR_MILK:
		case TF_WEAPON_GRAPPLINGHOOK: return true;
		default: return false;
		}
		break;
	}
	}
}

bool SDK::WeaponDoesNotUseAmmo(int WeaponID, int DefIdx, bool bIncludeInfiniteAmmo)
{
	switch (DefIdx)
	{
	case Soldier_s_TheBuffBanner:
	case Soldier_s_FestiveBuffBanner:
	case Soldier_s_TheBattalionsBackup:
	case Soldier_s_TheConcheror:
	case Demoman_s_TheTideTurner:
	case Demoman_s_TheCharginTarge:
	case Demoman_s_TheSplendidScreen:
	case Demoman_s_FestiveTarge:
	case Demoman_m_TheBootlegger:
	case Demoman_m_AliBabasWeeBooties:
	case Engi_s_TheWrangler:
	case Engi_s_FestiveWrangler:
	case Sniper_s_CozyCamper:
	case Sniper_s_DarwinsDangerShield:
	case Sniper_s_TheRazorback:
	case Pyro_s_ThermalThruster: return true;
	default:
	{
		switch (WeaponID)
		{
		case TF_WEAPON_PARTICLE_CANNON:
		case TF_WEAPON_RAYGUN:
		case TF_WEAPON_DRG_POMSON: return bIncludeInfiniteAmmo;
		case TF_WEAPON_FLAREGUN_REVENGE:
		case TF_WEAPON_MEDIGUN:
		case TF_WEAPON_PDA:
		case TF_WEAPON_PDA_ENGINEER_BUILD:
		case TF_WEAPON_PDA_ENGINEER_DESTROY:
		case TF_WEAPON_PDA_SPY:
		case TF_WEAPON_PDA_SPY_BUILD:
		case TF_WEAPON_BUILDER:
		case TF_WEAPON_LUNCHBOX:
		case TF_WEAPON_THROWABLE:
		case TF_WEAPON_JAR:
		case TF_WEAPON_JAR_GAS:
		case TF_WEAPON_JAR_MILK:
		case TF_WEAPON_GRAPPLINGHOOK: return true;
		default: return false;
		}
		break;
	}
	}
}


// Is there a way of doing this without hardcoded numbers???
int SDK::GetWeaponMaxReserveAmmo(int WeaponID, int DefIdx)
{
	switch (DefIdx)
	{
	case Engi_m_TheWidowmaker:
	case Engi_s_TheShortCircuit:
		return 200;
	case Scout_m_ForceANature:
	case Scout_m_FestiveForceANature:
	case Scout_m_BackcountryBlaster:
		return 32;
	case Demoman_s_TheQuickiebombLauncher:
		return 24;
	case Demoman_s_TheScottishResistance:
		return 36;
	case Demoman_s_StickyJumper:
		return 72;
	case Soldier_m_RocketJumper:
		return 60;
	default:
	{
		switch (WeaponID)
		{
		case TF_WEAPON_MINIGUN:
		case TF_WEAPON_PISTOL:
		case TF_WEAPON_FLAMETHROWER:
			return 200;
		case TF_WEAPON_SYRINGEGUN_MEDIC:
			return 150;
		case TF_WEAPON_SMG:
			return 75;
		case TF_WEAPON_FLAME_BALL:
			return 40;
		case TF_WEAPON_CROSSBOW:
			return 38;
		case TF_WEAPON_HANDGUN_SCOUT_SECONDARY:
		case TF_WEAPON_PISTOL_SCOUT:
		case TF_WEAPON_HANDGUN_SCOUT_PRIMARY:
			return 36;
		case TF_WEAPON_SCATTERGUN:
		case TF_WEAPON_PEP_BRAWLER_BLASTER:
		case TF_WEAPON_SODA_POPPER:
		case TF_WEAPON_SHOTGUN_HWG:
		case TF_WEAPON_SHOTGUN_PRIMARY:
		case TF_WEAPON_SHOTGUN_PYRO:
		case TF_WEAPON_SHOTGUN_SOLDIER:
			return 32;
		case TF_WEAPON_SNIPERRIFLE:
		case TF_WEAPON_SNIPERRIFLE_CLASSIC:
		case TF_WEAPON_SNIPERRIFLE_DECAP:
			return 25;
		case TF_WEAPON_STICKBOMB:
		case TF_WEAPON_STICKY_BALL_LAUNCHER:
		case TF_WEAPON_REVOLVER:
			return 24;
		case TF_WEAPON_ROCKETLAUNCHER:
		case TF_WEAPON_ROCKETLAUNCHER_DIRECTHIT:
			return 20;
		case TF_WEAPON_CANNON:
		case TF_WEAPON_SHOTGUN_BUILDING_RESCUE:
		case TF_WEAPON_GRENADELAUNCHER:
		case TF_WEAPON_FLAREGUN:
			return 16;
		case TF_WEAPON_COMPOUND_BOW:
			return 12;
		default:
			break;
		}
		break;
	}
	}

	return 32;
}

std::string SDK::GetLevelName()
{
	const std::string name = I::EngineClient->GetLevelName();
	const char* data = name.data();
	const size_t length = name.length();
	size_t slash = 0;
	size_t bsp = length;

	for (size_t i = length - 1; i != std::string::npos; --i)
	{
		if (data[i] == '/')
		{
			slash = i + 1;
			break;
		}
		if (data[i] == '.')
			bsp = i;
	}

	return { data + slash, bsp - slash };
}