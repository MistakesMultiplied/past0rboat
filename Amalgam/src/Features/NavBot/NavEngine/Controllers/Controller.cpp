#include "Controller.h"
#include "CPController/CPController.h"
#include "FlagController/FlagController.h"
#include "PLController/PLController.h"

ETFGameType GetGameType()
{
	// Check if we're on doomsday
	auto map_name = std::string(I::EngineClient->GetLevelName());
	F::GameObjectiveController.m_bDoomsday = map_name.find("sd_doomsday") != std::string::npos;

	int iType = TF_GAMETYPE_UNDEFINED;
	if (auto pGameRules = I::TFGameRules())
		iType = pGameRules->m_nGameType();

	return static_cast<ETFGameType>(iType);
}

void CGameObjectiveController::Update()
{
	static Timer tUpdateGameType;
	if (m_eGameMode || tUpdateGameType.Run(1.f))
	{
		m_eGameMode = GetGameType();
		SDK::Output("CGameObjectiveController", std::format("Current game mode: {}", (int)m_eGameMode).c_str(), { 60, 255, 60 }, Vars::Debug::Logging.Value, Vars::Debug::Logging.Value);
	}

	switch (m_eGameMode)
	{
	case TF_GAMETYPE_CTF:
		F::FlagController.Update();
		break;
	case TF_GAMETYPE_CP:
		F::CPController.Update();
		break;
	case TF_GAMETYPE_ESCORT:
		F::PLController.Update();
		break;
	default:
		if (m_bDoomsday)
		{
			F::FlagController.Update();
			F::CPController.Update();
		}
		break;
	}
}

void CGameObjectiveController::Reset()
{
	m_eGameMode = TF_GAMETYPE_UNDEFINED;
	F::FlagController.Init();
	F::PLController.Init();
	F::CPController.Init();
}
