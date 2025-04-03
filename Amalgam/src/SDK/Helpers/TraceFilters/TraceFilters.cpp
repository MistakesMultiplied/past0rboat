#include "TraceFilters.h"

#include "../../SDK.h"

bool CTraceFilterHitscan::ShouldHitEntity(IHandleEntity* pServerEntity, int nContentsMask)
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
	{
		auto pLocal = H::Entities.GetLocal();
		const int iTargetTeam = pEntity->m_iTeamNum(), iLocalTeam = pLocal ? pLocal->m_iTeamNum() : iTargetTeam;
		return iTargetTeam != iLocalTeam;
	}
	case ETFClassID::CTFPlayer:
	case ETFClassID::CBaseObject:
	case ETFClassID::CObjectSentrygun:
	case ETFClassID::CObjectDispenser:
	case ETFClassID::CObjectTeleporter: 
	{
		auto pLocal = H::Entities.GetLocal();
		auto pWeapon = H::Entities.GetWeapon();
		bool bSniperRifle = false;
		if (pLocal && pLocal == pSkip && pWeapon)
		{
			switch (pWeapon->GetWeaponID())
			{
			case TF_WEAPON_SNIPERRIFLE:
			case TF_WEAPON_SNIPERRIFLE_CLASSIC:
			case TF_WEAPON_SNIPERRIFLE_DECAP:
				bSniperRifle = true;
			}
		}
		if (!bSniperRifle)
			return true;
		const int iTargetTeam = pEntity->m_iTeamNum(), iLocalTeam = pLocal ? pLocal->m_iTeamNum() : iTargetTeam;
		return iTargetTeam != iLocalTeam;
	}
	}

	return true;
}
TraceType_t CTraceFilterHitscan::GetTraceType() const
{
	return TRACE_EVERYTHING;
}

bool CTraceFilterProjectile::ShouldHitEntity(IHandleEntity* pServerEntity, int nContentsMask)
{
	if (!pServerEntity || pServerEntity == pSkip)
		return false;

	auto pEntity = reinterpret_cast<CBaseEntity*>(pServerEntity);

	switch (pEntity->GetClassID())
	{
	case ETFClassID::CBaseEntity:
	case ETFClassID::CBaseDoor:
	case ETFClassID::CDynamicProp:
	case ETFClassID::CPhysicsProp:
	case ETFClassID::CPhysicsPropMultiplayer:
	case ETFClassID::CObjectCartDispenser:
	case ETFClassID::CFuncTrackTrain:
	case ETFClassID::CFuncConveyor:
	case ETFClassID::CBaseObject:
	case ETFClassID::CObjectSentrygun:
	case ETFClassID::CObjectDispenser:
	case ETFClassID::CObjectTeleporter: return true;
	case ETFClassID::CTFMedigunShield:
	{
		auto pLocal = H::Entities.GetLocal();
		auto pWeapon = H::Entities.GetWeapon();

		const int iTargetTeam = pEntity->m_iTeamNum(), iLocalTeam = pLocal ? pLocal->m_iTeamNum() : iTargetTeam;
		return iTargetTeam != iLocalTeam;
	}
	case ETFClassID::CTFPlayer:
	{
		auto pLocal = H::Entities.GetLocal();
		auto pWeapon = H::Entities.GetWeapon();
		const bool bHeal = (pLocal && pLocal == pSkip && pWeapon) ? pWeapon->GetWeaponID() == TF_WEAPON_CROSSBOW || pWeapon->GetWeaponID() == TF_WEAPON_LUNCHBOX : false;
		if (bHeal)
			return true;
		const int iTargetTeam = pEntity->m_iTeamNum(), iLocalTeam = pLocal ? pLocal->m_iTeamNum() : iTargetTeam;
		return iTargetTeam != iLocalTeam;
	}
	}

	return false;
}
TraceType_t CTraceFilterProjectile::GetTraceType() const
{
	return TRACE_EVERYTHING;
}

bool CTraceFilterWorldAndPropsOnly::ShouldHitEntity(IHandleEntity* pServerEntity, int nContentsMask)
{
	if (!pServerEntity || pServerEntity == pSkip)
		return false;

	auto pEntity = reinterpret_cast<CBaseEntity*>(pServerEntity);

	switch (pEntity->GetClassID())
	{
	case ETFClassID::CBaseEntity:
	case ETFClassID::CBaseDoor:
	case ETFClassID::CDynamicProp:
	case ETFClassID::CPhysicsProp:
	case ETFClassID::CPhysicsPropMultiplayer:
	case ETFClassID::CObjectCartDispenser:
	case ETFClassID::CFuncTrackTrain:
	case ETFClassID::CFuncConveyor: return true;
	case ETFClassID::CFuncRespawnRoomVisualizer:
		if (nContentsMask & MASK_PLAYERSOLID)
		{
			switch (pEntity->m_iTeamNum())
			{
			case TF_TEAM_RED: return nContentsMask & CONTENTS_REDTEAM;
			case TF_TEAM_BLUE: return nContentsMask & CONTENTS_BLUETEAM;
			}
		}
	}

	if (pServerEntity->GetRefEHandle().GetSerialNumber() == (1 << 15))
		return I::ClientEntityList->GetClientEntity(0) != pSkip;

	return false;
}
TraceType_t CTraceFilterWorldAndPropsOnly::GetTraceType() const
{
	return TRACE_EVERYTHING_FILTER_PROPS;
}

#define MOVEMENT_COLLISION_GROUP 8
#define RED_CONTENTS_MASK 0x800
#define BLU_CONTENTS_MASK 0x1000

bool CTraceFilterNavigation::ShouldHitEntity( IHandleEntity* pServerEntity, int nContentsMask )
{
	if (!pServerEntity)
		return false;

	auto pEntity = reinterpret_cast<CBaseEntity*>(pServerEntity);
	if( pEntity->entindex() != 0 && pEntity->GetClassID( ) != ETFClassID::CBaseEntity )
	{
		if ( pEntity->GetClassID( ) == ETFClassID::CFuncRespawnRoomVisualizer )
		{
			auto pLocal = H::Entities.GetLocal( );
			const int iTargetTeam = pEntity->m_iTeamNum( ), iLocalTeam = pLocal ? pLocal->m_iTeamNum( ) : iTargetTeam;

			// Cant we just check for the teamnum here???
		
			// If we can't collide, hit it
			if ( !pEntity->ShouldCollide( MOVEMENT_COLLISION_GROUP, iLocalTeam == TF_TEAM_RED ? RED_CONTENTS_MASK : BLU_CONTENTS_MASK ) )
				return true;
		}
		return false;
	}
	return true;
}

TraceType_t CTraceFilterNavigation::GetTraceType( ) const
{
	return TRACE_EVERYTHING;
}
