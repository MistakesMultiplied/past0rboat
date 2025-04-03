#include "../SDK/SDK.h"

#include "../Features/Commands/Commands.h"
#include <boost/algorithm/string/split.hpp>

class split_q
{
public:
	split_q() : in_q(false) {}

	bool operator()(char ch) const
	{
		if (ch == '\"')
		{
			in_q = !in_q;
		}
		return !in_q && ch == ' ';
	}

private:
	mutable bool in_q;
};

MAKE_HOOK(IVEngineClient_ClientCmd_Unrestricted, U::Memory.GetVFunc(I::EngineClient, 106), void,
	void* rcx, const char* szCmdString)
{
#ifdef DEBUG_HOOKS
	if (!Vars::Hooks::IVEngineClient_ClientCmd_Unrestricted[DEFAULT_BIND])
		return CALL_ORIGINAL(rcx, szCmdString);
#endif

	if (!G::Unload)
	{
		std::string sCmdString = szCmdString;
		std::transform(sCmdString.begin(), sCmdString.end(), sCmdString.begin(), ::tolower);
		//if ( sCmdString.rfind("say", 0) != 0 )
		{
			std::deque<std::string> vArgs;
			boost::split( vArgs, sCmdString, split_q( ) );

			if ( !vArgs.empty( ) )
			{
				std::string sCommand = vArgs.front( );
				vArgs.pop_front( );

				if ( F::Commands.Run( sCommand, vArgs ) )
					return;
			}
		}
		/*else
		{
			SDK::Output( "ClientCmd_Unrestricted", szCmdString, Vars::Menu::Theme::Accent.Value );
		}*/
	}

	CALL_ORIGINAL(rcx, szCmdString);
}