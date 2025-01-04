#include "Events.h"

#include "../../Features/Backtrack/Backtrack.h"
#include "../../Features/CheaterDetection/CheaterDetection.h"
#include "../../Features/CritHack/CritHack.h"
#include "../../Features/Misc/Misc.h"
#include "../../Features/PacketManip/AntiAim/AntiAim.h"
#include "../../Features/Records/Records.h"
#include "../../Features/Resolver/Resolver.h"
#include "../../Features/Visuals/Visuals.h"

bool CEventListener::Initialize()
{
	std::vector<const char*> vEvents = { 
		"client_beginconnect", "client_connected", "client_disconnect", "game_newmap", "teamplay_round_start", "player_connect_client", "player_spawn", "player_changeclass", "player_hurt", "vote_cast", "item_pickup", "revive_player_notify"
	};

	bool bFail{false};
	for (auto szEvent : vEvents)
	{
		I::GameEventManager->AddListener(this, szEvent, false);

		if (!I::GameEventManager->FindListener(this, szEvent))
		{
			SDK::Output("Amalgam", std::format("Failed to add listener: {}", szEvent).c_str(), { 255, 150, 175, 255 });
			bFail = true;
		}
	}
	return !bFail;
}

void CEventListener::Unload()
{
	I::GameEventManager->RemoveListener(this);
}

void CEventListener::FireGameEvent(IGameEvent* pEvent)
{
	if (!pEvent || G::Unload)
		return;

	auto pLocal = H::Entities.GetLocal();
	auto uHash = FNV1A::Hash32(pEvent->GetName());

	F::Records.Event(pEvent, uHash, pLocal);
	if (I::EngineClient->IsPlayingTimeDemo())
		return;

	F::CritHack.Event(pEvent, uHash, pLocal);
	F::Misc.Event(pEvent, uHash);
	F::Visuals.Event(pEvent, uHash);
	switch (uHash)
	{
	case FNV1A::Hash32Const("player_hurt"):
		F::Resolver.OnPlayerHurt(pEvent);
		F::CheaterDetection.ReportDamage(pEvent);
		break;
	case FNV1A::Hash32Const("player_spawn"):
		F::Backtrack.SetLerp(pEvent);
		break;
	case FNV1A::Hash32Const("revive_player_notify"):
	{
		if (!Vars::Misc::MannVsMachine::InstantRevive.Value || pEvent->GetInt("entindex") != I::EngineClient->GetLocalPlayer())
			break;

		KeyValues* kv = new KeyValues("MVM_Revive_Response");
		kv->SetInt("accepted", 1);
		I::EngineClient->ServerCmdKeyValues(kv);
	}
	}

	return;
}