#include "Hooks.h"

#include "../Assert/Assert.h"
#include "../../Core/Core.h"
#include "../../Hooks/Direct3DDevice9.h"

CHook::CHook(std::string sName, void* pInitFunc)
{
	m_pInitFunc = pInitFunc;
	U::Hooks.m_mHooks[sName] = this;
}

bool CHooks::Initialize()
{
	MH_Initialize();

	WndProc::Initialize();

	bool bFail{false};
	for (auto& [_, pHook] : m_mHooks)
	{
		if (!reinterpret_cast<bool(__cdecl*)()>(pHook->m_pInitFunc)())
			bFail = true;
	}

	m_bFailed = MH_EnableHook(MH_ALL_HOOKS) != MH_OK;
	if (m_bFailed)
		U::Core.AppendFailText("MinHook failed to enable all hooks!");
	return !m_bFailed;
}

bool CHooks::Unload()
{
	m_bFailed = MH_Uninitialize() != MH_OK;
	if (m_bFailed)
		U::Core.AppendFailText("MinHook failed to unload all hooks!");
	WndProc::Unload();
	return !m_bFailed;
}