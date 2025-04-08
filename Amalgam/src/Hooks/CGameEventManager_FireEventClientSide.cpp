#include "../SDK/SDK.h"
#include "../Features/Killstreak/Killstreak.h"

#ifndef TEXTMODE
MAKE_HOOK(CGameEventManager_FireEventClientSide, U::Memory.GetVFunc(I::GameEventManager, 8), bool,
	IGameEventManager2* rcx, IGameEvent* event)
{
	// We have to do it here because our own event listener runs after game event manager
	if (Vars::Visuals::Misc::KillstreakWeapons.Value)
	{
		auto uHash = FNV1A::Hash32(event->GetName());
		if (uHash == FNV1A::Hash32Const("player_death"))
			F::Killstreak.PlayerDeath(event);
	}

	return CALL_ORIGINAL(rcx, event);
}
#endif