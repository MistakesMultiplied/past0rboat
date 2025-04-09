#pragma once
#include "CBaseEntity.h"

class CBaseTeamObjectiveResource : public CBaseEntity
{
public:
	NETVAR( m_iNumControlPoints, int, "CBaseTeamObjectiveResource", "m_iNumControlPoints" );
	NETVAR( m_bPlayingMiniRounds, bool, "CBaseTeamObjectiveResource", "m_bPlayingMiniRounds" );

	inline Vector& CPPositions ( int index )
	{
		static int offset = U::NetVars.GetNetVar( "CBaseTeamObjectiveResource", "m_vCPPositions[0]" );
		return ( &( *reinterpret_cast< Vector* >( reinterpret_cast< DWORD_PTR >( this ) + offset ) ) )[ index ];
	}

	inline int& BaseControlPoints( int index )
	{
		static int offset = U::NetVars.GetNetVar( "CBaseTeamObjectiveResource", "m_iBaseControlPoints" );
		return ( &( *reinterpret_cast< int* >( reinterpret_cast< DWORD_PTR >( this ) + offset ) ) )[ index ];
	}

	inline int& PreviousPoints( int index )
	{
		static int offset = U::NetVars.GetNetVar( "CBaseTeamObjectiveResource", "m_iPreviousPoints" );
		return ( &( *reinterpret_cast< int* >( reinterpret_cast< DWORD_PTR >( this ) + offset ) ) )[ index ];
	}

	inline int& OwningTeam( int index ) 
	{
		static int offset = U::NetVars.GetNetVar( "CBaseTeamObjectiveResource", "m_iOwner" );
		return ( &( *reinterpret_cast< int* >( reinterpret_cast< DWORD_PTR >( this ) + offset ) ) )[ index ];
	}

	inline bool& CPLocked( int index )
	{
		static int offset = U::NetVars.GetNetVar( "CBaseTeamObjectiveResource", "m_bCPLocked" );
		return ( &( *reinterpret_cast< bool* >( reinterpret_cast< DWORD_PTR >( this ) + offset ) ) )[ index ];
	}

	inline bool& TeamCanCap( int index )
	{
		static int offset = U::NetVars.GetNetVar( "CBaseTeamObjectiveResource", "m_bTeamCanCap" );
		return ( &( *reinterpret_cast< bool* >( reinterpret_cast< DWORD_PTR >( this ) + offset ) ) )[ index ];
	}

	inline bool& InMiniRound( int index )
	{
		static int offset = U::NetVars.GetNetVar( "CBaseTeamObjectiveResource", "m_bInMiniRound" );
		return ( &( *reinterpret_cast< bool* >( reinterpret_cast< DWORD_PTR >( this ) + offset ) ) )[ index ];
	}
};