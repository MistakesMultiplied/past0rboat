#include "NavEngine.h"
#include "../../TickHandler/TickHandler.h"
#include "../../Misc/Misc.h"
#include <direct.h>

std::optional<Vector> CNavParser::GetDormantOrigin( int index )
{
	if ( !index )
		return std::nullopt;

	auto pEntity = I::ClientEntityList->GetClientEntity( index );

	if ( !pEntity || !pEntity->As<CBasePlayer>()->IsAlive( ) )
		return std::nullopt;

	if ( !pEntity->IsPlayer( ) && !pEntity->IsBuilding( ) )
		return std::nullopt;

	if ( !pEntity->IsDormant( ) || H::Entities.GetDormancy(index) )
		return pEntity->GetAbsOrigin( );

	return std::nullopt;
}



bool CNavParser::IsSetupTime( )
{
	static Timer checkTimer{};
	static bool setupTime{ false };
	if ( Vars::Misc::Movement::NavEngine::PathInSetup.Value )
		return false;

	const auto& pLocal = H::Entities.GetLocal( );
	if ( pLocal && pLocal->IsAlive( ) )
	{
		const std::string level_name = SDK::GetLevelName( );

		// No need to check the round states that quickly.
		if ( checkTimer.Run( 1.5f ) )
		{
			// Special case for Pipeline which doesn't use standard setup time
			if (level_name == "plr_pipeline")
				return false;

			if ( const auto& GameRules = I::TFGameRules( ) )
			{
				// The round just started, players cant move.
				if ( GameRules->m_iRoundState( ) == GR_STATE_PREROUND )
					return setupTime = true;

				if ( pLocal->m_iTeamNum( ) == TF_TEAM_BLUE )
				{
					if ( GameRules->m_bInSetup( ) || ( GameRules->m_bInWaitingForPlayers( ) && ( level_name.starts_with( "pl_" ) || level_name.starts_with( "cp_" ) ) ) )
						return setupTime = true;
				}
				setupTime = false;
			}
		}
	}
	return setupTime;
}

bool CNavParser::IsVectorVisibleNavigation( Vector from, Vector to, unsigned int mask )
{
	Ray_t ray;
	CGameTrace trace_visible;
	CTraceFilterNavigation Filter{};

	ray.Init( from, to );
	I::EngineTrace->TraceRay( ray, mask, &Filter, &trace_visible );
	return trace_visible.fraction == 1.0f;
}

static float slow_change_dist_y{};
void CNavParser::DoSlowAim( Vector& input_angle, float speed, Vector viewangles )
{
	// Yaw
	if ( viewangles.y != input_angle.y )
	{
		Vector slow_delta = input_angle - viewangles;

		while ( slow_delta.y > 180 )
			slow_delta.y -= 360;
		while ( slow_delta.y < -180 )
			slow_delta.y += 360;

		slow_delta /= speed;
		input_angle = viewangles + slow_delta;

		// Clamp as we changed angles
		Math::ClampAngles( input_angle );
	}
}

void CNavParser::Map::AdjacentCost( void* main, std::vector<micropather::StateCost>* adjacent )
{
	CNavArea& area = *reinterpret_cast< CNavArea* >( main );
	for ( NavConnect& connection : area.m_connections )
	{
		// An area being entered twice means it is blacklisted from entry entirely
		auto connection_key = std::pair<CNavArea*, CNavArea*>( connection.area, connection.area );

		// Entered and marked bad?
		if ( vischeck_cache.count( connection_key ) && !vischeck_cache[connection_key].vischeck_state )
			continue;

		// If the extern blacklist is running, ensure we don't try to use a bad area
		if ( !free_blacklist_blocked && std::any_of( free_blacklist.begin( ), free_blacklist.end( ), [&]( const auto& entry ) { return entry.first == connection.area; } ) )
			continue;

		auto points = F::NavParser.determinePoints( &area, connection.area );

		// Apply dropdown
		points.center = F::NavParser.handleDropdown( points.center, points.next );

		float height_diff = points.center_next.z - points.center.z;

		// Too high for us to jump!
		if ( height_diff > PLAYER_JUMP_HEIGHT )
			continue;

		points.current.z += PLAYER_JUMP_HEIGHT;
		points.center.z += PLAYER_JUMP_HEIGHT;
		points.next.z += PLAYER_JUMP_HEIGHT;

		const auto key = std::pair<CNavArea*, CNavArea*>( &area, connection.area );
		if ( vischeck_cache.count( key ) )
		{
			if ( vischeck_cache[key].vischeck_state )
			{
				const float cost = connection.area->m_center.DistToSqr( area.m_center );
				adjacent->push_back( micropather::StateCost{ reinterpret_cast< void* >( connection.area ), cost } );
			}
		}
		else
		{
			// Check if there is direct line of sight
			if ( F::NavParser.IsPlayerPassableNavigation( points.current, points.center ) &&
				F::NavParser.IsPlayerPassableNavigation( points.center, points.next ) )
			{
				vischeck_cache[ key ] = { TICKCOUNT_TIMESTAMP( 60 ), true };

				const float cost = points.next.DistToSqr( points.current );
				adjacent->push_back( micropather::StateCost{ reinterpret_cast< void* >( connection.area ), cost } );
			}
			else
				vischeck_cache[ key ] = { TICKCOUNT_TIMESTAMP( 60 ), false };
		}
	}
}

CNavArea* CNavParser::Map::findClosestNavSquare( const Vector& vec )
{
	const auto& pLocal = H::Entities.GetLocal( );
	if ( !pLocal || !pLocal->IsAlive( ) )
		return nullptr;

	auto vec_corrected = vec;
	vec_corrected.z += PLAYER_JUMP_HEIGHT;
	float overall_best_dist = FLT_MAX, best_dist = FLT_MAX;
	// If multiple candidates for LocalNav have been found, pick the closest
	CNavArea* overall_best_square = nullptr, *best_square = nullptr;

	for ( auto& i : navfile.m_areas )
	{
		// Marked bad, do not use if local origin
		if ( pLocal->GetAbsOrigin( ) == vec )
		{
			auto key = std::pair<CNavArea*, CNavArea*>( &i, &i );
			if ( vischeck_cache.count( key ) && !vischeck_cache[key].vischeck_state )
				continue;
		}

		float dist = i.m_center.DistToSqr( vec );
		if ( dist < best_dist )
		{
			best_dist = dist;
			best_square = &i;
		}

		if ( overall_best_dist < dist )
			continue;

		auto center_corrected = i.m_center;
		center_corrected.z += PLAYER_JUMP_HEIGHT;

		// Check if we are within x and y bounds of an area
		if ( !i.IsOverlapping( vec ) || !F::NavParser.IsVectorVisibleNavigation( vec_corrected, center_corrected ) )
			continue;

		overall_best_dist = dist;
		overall_best_square = &i;

		// Early return if the area is overlapping and visible
		if ( overall_best_dist == best_dist )
			return overall_best_square;
	}

	return overall_best_square ? overall_best_square : best_square;
}

void CNavParser::Map::updateIgnores( )
{
	static Timer update_time;
	if ( !update_time.Run( 1.f ) )
		return;

	const auto& pLocal = H::Entities.GetLocal( );

	if ( !pLocal || !pLocal->IsAlive( )  )
		return;

	// Clear the blacklist
	F::NavEngine.clearFreeBlacklist( BlacklistReason( BR_SENTRY ) );
	F::NavEngine.clearFreeBlacklist( BlacklistReason( BR_ENEMY_INVULN ) );
	if ( Vars::Misc::Movement::NavBot::Blacklist.Value & Vars::Misc::Movement::NavBot::BlacklistEnum::Players )
	{
		for ( const auto& pEntity : H::Entities.GetGroup( EGroupType::PLAYERS_ENEMIES ) )
		{
			if ( !pEntity || !pEntity->IsPlayer( ) )
				continue;

			const auto& pPlayer = pEntity->As<CTFPlayer>( );
			if ( !pPlayer->IsAlive( ) )
				continue;

			if ( pPlayer->IsInvulnerable( ) &&
				// Cant do the funny (We are not heavy or we do not have the holiday punch equipped)
				( pLocal->m_iClass( ) != TF_CLASS_HEAVY || G::SavedDefIndexes[ SLOT_MELEE ] != Heavy_t_TheHolidayPunch ) )
			{
				// Get origin of the player
				auto player_origin = F::NavParser.GetDormantOrigin( pPlayer->entindex( ) );
				if ( player_origin )
				{
					player_origin.value( ).z += PLAYER_JUMP_HEIGHT;

					// Actual player check
					for ( auto& i : navfile.m_areas )
					{
						Vector area = i.m_center;
						area.z += PLAYER_JUMP_HEIGHT;

						// Check if player is close to us
						if ( player_origin.value( ).DistToSqr( area ) <= pow( 1000.0f, 2 ) )
						{
							// Check if player can hurt us
							if ( !F::NavParser.IsVectorVisibleNavigation( player_origin.value( ), area, MASK_SHOT ) )
								continue;

							// Blacklist
							free_blacklist[ &i ] = BR_ENEMY_INVULN;
						}
					}
				}
			}
		}
	}

	if ( Vars::Misc::Movement::NavBot::Blacklist.Value & Vars::Misc::Movement::NavBot::BlacklistEnum::Sentries )
	{
		for ( const auto& pEntity : H::Entities.GetGroup( EGroupType::BUILDINGS_ENEMIES ) )
		{
			if ( !pEntity || !pEntity->IsBuilding( ) )
				continue;

			const auto& pBuilding = pEntity->As<CBaseObject>( );

			if ( pBuilding->GetClassID( ) == ETFClassID::CObjectSentrygun )
			{
				const auto& pSentry = pBuilding->As<CObjectSentrygun>( );
				// Should we even ignore the sentry?
				if ( pSentry->m_iState( ) != SENTRY_STATE_INACTIVE )
				{
					// Soldier/Heavy do not care about Level 1 or mini sentries
					const bool is_strong_class = pLocal->m_iClass( ) == TF_CLASS_HEAVY || pLocal->m_iClass( ) == TF_CLASS_SOLDIER;
					const int bullet = pSentry->m_iAmmoShells( );
					const int rocket = pSentry->m_iAmmoRockets( );
					if ( !is_strong_class || ( !pSentry->m_bMiniBuilding( ) && pSentry->m_iUpgradeLevel( ) != 1 ) && bullet != 0 || ( pSentry->m_iUpgradeLevel( ) == 3 && rocket != 0 ) )
					{
						// It's still building/being sapped, ignore.
						// Unless it just was deployed from a carry, then it's dangerous
						if ( pSentry->m_bCarryDeploy( ) || !pSentry->m_bBuilding( ) && !pSentry->m_bPlacing( ) && !pSentry->m_bHasSapper( ) )
						{
							// Get origin of the sentry
							auto building_origin = F::NavParser.GetDormantOrigin(pSentry->entindex());
							if ( !building_origin )
								continue;

							// For dormant sentries we need to add the jump height to the z
							// if ( pSentry->IsDormant( ) )
								building_origin->z += PLAYER_JUMP_HEIGHT;

							// Actual building check
							for ( auto& i : navfile.m_areas )
							{
								Vector area = i.m_center;
								area.z += PLAYER_JUMP_HEIGHT;
								// Out of range
								if ( building_origin->DistToSqr( area ) <= pow( 1200.0f + HALF_PLAYER_WIDTH, 2 ) )
								{
									// Check if sentry can see us
									if ( F::NavParser.IsVectorVisibleNavigation( *building_origin, area, MASK_SHOT ) )
									{
										// Blacklist because it's in view range of the sentry
										free_blacklist[ &i ] = BR_SENTRY;
									}
								}
							}
						}
					}
				}
			}
		}
	}

	const auto stickytimestamp = TICKCOUNT_TIMESTAMP( Vars::Misc::Movement::NavEngine::StickyIgnoreTime.Value );
	if (Vars::Misc::Movement::NavBot::Blacklist.Value & Vars::Misc::Movement::NavBot::BlacklistEnum::Stickies)
	{
		for ( const auto& pEntity : H::Entities.GetGroup( EGroupType::WORLD_PROJECTILES ) )
		{
			if ( !pEntity )
				continue;

			const auto& pSticky = pEntity->As<CTFGrenadePipebombProjectile>( );
			if ( pSticky->GetClassID() != ETFClassID::CTFGrenadePipebombProjectile || pSticky->m_iTeamNum( ) == pLocal->m_iTeamNum( ) || pSticky->m_iType( ) != TF_GL_MODE_REMOTE_DETONATE || pSticky->IsDormant( ) || !pSticky->m_vecVelocity( ).IsZero( 1.f ) )
				continue;

			auto sticky_origin = pSticky->GetAbsOrigin( );
			// Make sure the sticky doesn't vischeck from inside the floor
			sticky_origin.z += PLAYER_JUMP_HEIGHT / 2.0f;
			for ( auto& i : navfile.m_areas )
			{
				Vector area = i.m_center;
				area.z += PLAYER_JUMP_HEIGHT;

				// Out of range
				if ( sticky_origin.DistToSqr( area ) <= pow( 130.0f + HALF_PLAYER_WIDTH, 2 ) )
				{
					CGameTrace trace = {};
					CTraceFilterProjectile filter = {};
					SDK::Trace( sticky_origin, area, MASK_SHOT, &filter, &trace );

					// Check if Sticky can see the reason
					if ( trace.fraction == 1.0f )
						free_blacklist[ &i ] = { BR_STICKY, stickytimestamp };
					// Blacklist because it's in range of the sticky, but stickies make no noise, so blacklist it for a specific timeframe
				}
			}
		}
	}
	
	static size_t previous_blacklist_size = 0;

	const bool erased = previous_blacklist_size != free_blacklist.size( );
	previous_blacklist_size = free_blacklist.size( );

	std::erase_if( free_blacklist, []( const auto& entry ) { return entry.second.time && entry.second.time < I::GlobalVars->tickcount; } );
	std::erase_if( vischeck_cache, []( const auto& entry ) { return entry.second.expire_tick < I::GlobalVars->tickcount; } );
	std::erase_if( connection_stuck_time, []( const auto& entry ) { return entry.second.expire_tick < I::GlobalVars->tickcount; } );

	if ( erased )
		pather.Reset( );
}

static bool PointIsWithin( Vector vPoint, Vector vMin, Vector vMax )
{
	return vPoint.x <= vMax.x && vPoint.y <= vMax.y && vPoint.z <= vMax.z 
		&& vPoint.x >= vMin.x && vPoint.y >= vMin.y && vPoint.z >= vMin.z;
}

void CNavParser::Map::UpdateRespawnRooms( )
{
	std::vector<CFuncRespawnRoom*> vFoundEnts;
	CServerBaseEntity* pPrevEnt = nullptr;
	while( true )
	{
		auto pEntity = I::ServerTools->FindEntityByClassname( pPrevEnt, "func_respawnroom" );
		if ( !pEntity )
			break;

		pPrevEnt = pEntity;

		vFoundEnts.push_back( pEntity->As<CFuncRespawnRoom>() );
	}

	if ( vFoundEnts.empty( ) )
	{
		SDK::Output( "CNavParser::Map::UpdateRespawnRooms", std::format( "Couldn't find any room entities").c_str( ), { 255, 50, 50, 255 }, Vars::Debug::Logging.Value);
		return;
	}

	for ( auto pRespawnRoom : vFoundEnts )
	{	
		if ( !pRespawnRoom )
			continue;

		static Vector stepHeight( 0.0f, 0.0f, 18.0f );
		for (auto& area : navfile.m_areas)
		{
			if ( pRespawnRoom->PointIsWithin(area.m_center + stepHeight)
				|| pRespawnRoom->PointIsWithin(area.m_nwCorner + stepHeight)
				|| pRespawnRoom->PointIsWithin(area.getNeCorner( ) + stepHeight)
				|| pRespawnRoom->PointIsWithin(area.getSwCorner( ) + stepHeight)
				|| pRespawnRoom->PointIsWithin(area.m_seCorner + stepHeight))
			{
				uint32_t uFlags = pRespawnRoom->m_iTeamNum( ) == TF_TEAM_BLUE ? TF_NAV_SPAWN_ROOM_BLUE : TF_NAV_SPAWN_ROOM_RED;
				if ( !( area.m_TFattributeFlags & uFlags ) )
					area.m_TFattributeFlags |= uFlags;
			}
		}
	}
}

bool CNavParser::CastRay( Vector origin, Vector endpos, unsigned mask, ITraceFilter* filter )
{
	CGameTrace trace;
	Ray_t ray;

	ray.Init( origin, endpos );

	// This was found to be So inefficient that it is literally unusable for our purposes. it is almost 1000x slower than the above.
	// ray.Init(origin, target, -right * HALF_PLAYER_WIDTH, right * HALF_PLAYER_WIDTH);

	I::EngineTrace->TraceRay( ray, mask, filter, &trace );

	return trace.DidHit( );
}

bool CNavParser::IsPlayerPassableNavigation( Vector origin, Vector target, unsigned int mask )
{
	const Vector tr = target - origin;
	Vector angles;
	Math::VectorAngles( tr, angles );

	Vector forward, right;
	Math::AngleVectors( angles, &forward, &right, nullptr );
	right.z = 0;

	const float tr_length = tr.Length( );
	const Vector relative_endpos = forward * tr_length;

	// We want to keep the same angle for these two bounding box traces
	const Vector left_ray_origin = origin - right * HALF_PLAYER_WIDTH;
	const Vector left_ray_endpos = left_ray_origin + relative_endpos;

	CTraceFilterNavigation Filter{};

	// Left ray hit something
	if ( CastRay( left_ray_origin, left_ray_endpos, mask, &Filter ) )
		return false;

	const Vector right_ray_origin = origin + right * HALF_PLAYER_WIDTH;
	const Vector right_ray_endpos = right_ray_origin + relative_endpos;

	// Return if the right ray hit something
	return !CastRay( right_ray_origin, right_ray_endpos, mask, &Filter );
}

Vector CNavParser::GetForwardVector(Vector origin, Vector viewangles, float distance)
{
    Vector forward;
    float sp, sy, cp, cy;
	const QAngle angle = viewangles;

    Math::SinCos(DEG2RAD(angle[1]), &sy, &cy);
    Math::SinCos(DEG2RAD(angle[0]), &sp, &cp);
    forward.x = cp * cy;
    forward.y = cp * sy;
    forward.z = -sp;
    forward   = forward * distance + origin;
    return forward;
}

Vector CNavParser::handleDropdown( Vector current_pos, Vector next_pos )
{
	Vector to_target = (next_pos - current_pos);
    float height_diff = to_target.z;
    
	// Handle drops more carefully
	if (height_diff < 0)
	{
		float drop_distance = -height_diff;
        
		// Small drops (less than jump height) - no special handling needed
		if (drop_distance <= PLAYER_JUMP_HEIGHT)
			return current_pos;
            
		// Medium drops - move out a bit to prevent getting stuck
		if (drop_distance <= PLAYER_HEIGHT)
		{
			to_target.z = 0;
			to_target.Normalize();
			QAngle angles;
			Math::VectorAngles(to_target, angles);
			Vector vec_angles(angles.x, angles.y, angles.z);
			return GetForwardVector(current_pos, vec_angles, PLAYER_WIDTH * 1.5f);
		}
		// Large drops - move out significantly to prevent fall damage
		to_target.z = 0;
		to_target.Normalize();
		QAngle angles;
		Math::VectorAngles(to_target, angles);
		Vector vec_angles(angles.x, angles.y, angles.z);
		return GetForwardVector(current_pos, vec_angles, PLAYER_WIDTH * 2.5f);
	}
	// Handle upward movement
	if (height_diff > 0)
	{
		// If it's within jump height, move closer to help with the jump
		if (height_diff <= PLAYER_JUMP_HEIGHT)
		{
			to_target.z = 0;
			to_target.Normalize();
			QAngle angles;
			Math::VectorAngles(-to_target, angles);
			Vector vec_angles(angles.x, angles.y, angles.z);
			return GetForwardVector(current_pos, vec_angles, PLAYER_WIDTH * 0.5f);
		}
	}
    
    return current_pos;
}

NavPoints CNavParser::determinePoints( CNavArea* current, CNavArea* next )
{
    auto area_center = current->m_center;
    auto next_center = next->m_center;
    // Gets a vector on the edge of the current area that is as close as possible to the center of the next area
    auto area_closest = current->getNearestPoint(Vector2D(next_center.x, next_center.y));
    // Do the same for the other area
    auto next_closest = next->getNearestPoint(Vector2D(area_center.x, area_center.y));

    // Use one of them as a center point, the one that is either x or y alligned with a center
    // Of the areas. This will avoid walking into walls.
    auto center_point = area_closest;

    // Determine if alligned, if not, use the other one as the center point
    if (center_point.x != area_center.x && center_point.y != area_center.y && center_point.x != next_center.x && center_point.y != next_center.y)
    {
        center_point = next_closest;
        // Use the point closest to next_closest on the "original" mesh for z
        center_point.z = current->getNearestPoint(Vector2D(next_closest.x, next_closest.y)).z;
    }

    // If safepathing is enabled, adjust points to stay more centered and avoid corners
    if (Vars::Misc::Movement::NavEngine::SafePathing.Value)
    {
        // Move points more towards the center of the areas
        Vector to_next = (next_center - area_center);
        to_next.z = 0.0f;
        to_next.Normalize();

        // Calculate center point as a weighted average between area centers
        // Use a 60/40 split to favor the current area more
        center_point = area_center + (next_center - area_center) * 0.4f;
        
        // Add extra safety margin near corners
        float corner_margin = PLAYER_WIDTH * 0.75f;
        
        // Check if we're near a corner by comparing distances to area edges
        bool near_corner = false;
        Vector area_mins = current->m_nwCorner; // Northwest corner
        Vector area_maxs = current->m_seCorner; // Southeast corner
        
        if (center_point.x - area_mins.x < corner_margin || 
            area_maxs.x - center_point.x < corner_margin ||
            center_point.y - area_mins.y < corner_margin || 
            area_maxs.y - center_point.y < corner_margin)
        {
            near_corner = true;
        }
        
        // If near corner, move point more towards center
        if (near_corner)
        {
            center_point = center_point + (area_center - center_point).Normalized() * corner_margin;
        }

        // Ensure the point is within the current area
        center_point = current->getNearestPoint(Vector2D(center_point.x, center_point.y));
    }

    // Nearest point to center on "next", used for height checks
    auto center_next = next->getNearestPoint(Vector2D(center_point.x, center_point.y));

    return NavPoints(area_center, center_point, center_next, next_center);
}

static Timer inactivity;
static Timer time_spent_on_crumb;
bool CNavEngine::navTo( const Vector& destination, int priority, bool should_repath, bool nav_to_local, bool is_repath )
{
	const auto& pLocal = H::Entities.GetLocal( );

	if ( !pLocal || !pLocal->IsAlive( ) || F::Ticks.m_bWarp || F::Ticks.m_bDoubletap )
		return false;

	if ( !isReady( ) )
		return false;

	// Don't path, priority is too low
	if ( priority < current_priority )
		return false;

	CNavArea* start_area = map->findClosestNavSquare( pLocal->GetAbsOrigin( ) );
	CNavArea* dest_area = map->findClosestNavSquare( destination );

	if ( !start_area || !dest_area )
		return false;

	auto path = map->findPath( start_area, dest_area );
	if ( path.empty( ) )
		return false;

	if ( !nav_to_local )
		path.erase( path.begin( ) );

	crumbs.clear( );

	for ( size_t i = 0; i < path.size( ); i++ )
	{
		auto area = reinterpret_cast< CNavArea* >( path.at( i ) );
		if ( !area )
			continue;

		// All entries besides the last need an extra crumb
		if ( i != path.size( ) - 1 )
		{
			auto next_area = reinterpret_cast< CNavArea* >( path.at( i + 1 ) );

			auto points = F::NavParser.determinePoints( area, next_area );

			points.center = F::NavParser.handleDropdown( points.center, points.next );

			crumbs.push_back( { area, points.current } );
			crumbs.push_back( { area, points.center } );
		}
		else
			crumbs.push_back( { area, area->m_center } );
	}

	crumbs.push_back( { nullptr, destination } );
	inactivity.Update( );

	current_priority = priority;
	current_navtolocal = nav_to_local;
	repath_on_fail = should_repath;
	// Ensure we know where to go
	if ( repath_on_fail )
		last_destination = destination;

	return true;
}

void CNavEngine::vischeckPath( )
{
	static Timer vischeck_timer{};
	// No crumbs to check, or vischeck timer should not run yet, bail.
	if ( crumbs.size( ) < 2 || !vischeck_timer.Run( Vars::Misc::Movement::NavEngine::VischeckTime.Value ) )
		return;

	const auto timestamp = TICKCOUNT_TIMESTAMP( Vars::Misc::Movement::NavEngine::VischeckCacheTime.Value );

	// Iterate all the crumbs
	for ( unsigned int i = 0; i < crumbs.size( ) - 1; i++ )
	{
		const auto current_crumb = crumbs[ i ];
		const auto next_crumb = crumbs[ i + 1 ];
		auto current_center = current_crumb.vec;
		auto next_center = next_crumb.vec;

		current_center.z += PLAYER_JUMP_HEIGHT;
		next_center.z += PLAYER_JUMP_HEIGHT;
		const auto key = std::pair<CNavArea*, CNavArea*>( current_crumb.navarea, next_crumb.navarea );

		// Check if we can pass, if not, abort pathing and mark as bad
		if ( !F::NavParser.IsPlayerPassableNavigation( current_center, next_center ) )
		{
			// Mark as invalid for a while
			map->vischeck_cache[ key ] = { timestamp, false };
			abandonPath( );
		}
		// Else we can update the cache (if not marked bad before this)
		else if ( !map->vischeck_cache.count( key ) || map->vischeck_cache[key].vischeck_state )
			map->vischeck_cache[ key ] = { timestamp, true };
	}
}

static Timer blacklist_check_timer{};
// Check if one of the crumbs is suddenly blacklisted
void CNavEngine::checkBlacklist( )
{
	// Only check every 500ms
	if ( !blacklist_check_timer.Run( 0.5f ) )
		return;

	const auto& pLocal = H::Entities.GetLocal( );

	if ( !pLocal || !pLocal->IsAlive( ) )
		return;

	// Local player is ubered and does not care about the blacklist
	// TODO: Only for damage type things
	if ( pLocal->IsInvulnerable( ) )
	{
		map->free_blacklist_blocked = true;
		map->pather.Reset( );
		return;
	}
	const auto origin = pLocal->GetAbsOrigin( );

	const auto local_area = map->findClosestNavSquare( origin );
	for ( const auto& entry : map->free_blacklist )
	{
		// Local player is in a blocked area, so temporarily remove the blacklist as else we would be stuck
		if ( entry.first == local_area )
		{
			map->free_blacklist_blocked = true;
			map->pather.Reset( );
			return;
		}
	}

	// Local player is not blocking the nav area, so blacklist should not be marked as blocked
	map->free_blacklist_blocked = false;

	bool should_abandon = false;
	for ( auto& crumb : crumbs )
	{
		if ( should_abandon )
			break;
		// A path Node is blacklisted, abandon pathing
		for ( const auto& entry : map->free_blacklist )
			if ( entry.first == crumb.navarea )
				should_abandon = true;
	}
	if ( should_abandon )
		abandonPath( );
}

void CNavEngine::updateStuckTime( )
{
	// No crumbs
	if ( crumbs.empty( ) )
		return;

	// We're stuck, add time to connection
	if ( inactivity.Check( Vars::Misc::Movement::NavEngine::StuckTime.Value / 2 ) )
	{
		std::pair<CNavArea*, CNavArea*> key = { last_crumb.navarea ? last_crumb.navarea : crumbs[ 0 ].navarea, crumbs[ 0 ].navarea };

		// Expires in 10 seconds
		map->connection_stuck_time[ key ].expire_tick = TICKCOUNT_TIMESTAMP( Vars::Misc::Movement::NavEngine::StuckExpireTime.Value );
		// Stuck for one tick
		map->connection_stuck_time[ key ].time_stuck += 1;

		// We are stuck for too long, blastlist node for a while and repath
		if ( map->connection_stuck_time[ key ].time_stuck > TIME_TO_TICKS( Vars::Misc::Movement::NavEngine::StuckDetectTime.Value ) )
		{
			const auto expire_tick = TICKCOUNT_TIMESTAMP( Vars::Misc::Movement::NavEngine::StuckBlacklistTime.Value );
			SDK::Output( "CNavEngine", std::format( "Stuck for too long, blacklisting the node (expires on tick: {})", expire_tick ).c_str( ), { 255, 131, 131, 255 }, Vars::Debug::Logging.Value );
			map->vischeck_cache[ key ].expire_tick = expire_tick;
			map->vischeck_cache[ key ].vischeck_state = false;
			abandonPath( );
		}
	}
}

void CNavEngine::OnLevelInit( )
{
	const auto level_name = I::EngineClient->GetLevelName( );
	if ( level_name )
	{
		if ( !map || map->mapname != level_name )
		{
			char* p, cwd[ MAX_PATH + 1 ], lvl_name[ 256 ];
			std::string nav_path;
			strncpy_s( lvl_name, level_name, 255 );
			lvl_name[ 255 ] = 0;
			p = std::strrchr( lvl_name, '.' );
			if ( !p )
				return;

			*p = 0;

			p = _getcwd( cwd, sizeof( cwd ) );
			if ( !p )
				return;

			nav_path = std::format( "{}/tf/{}.nav", cwd, lvl_name );
			SDK::Output( "NavEngine", std::format( "Nav File location: {}", nav_path ).c_str( ), Color_t( 50, 255, 50, 255 ), Vars::Debug::Logging.Value );
			map = std::make_unique<CNavParser::Map>( nav_path.c_str( ) );
		}
		else
			map->Reset( );
	}
}

static Timer restartTimer{};
bool CNavEngine::isReady( bool roundCheck )
{
	const auto& pLocal = H::Entities.GetLocal( );
	if ( !Vars::Misc::Movement::NavEngine::Enabled.Value || !pLocal || !pLocal->IsAlive( ) )
	{
		restartTimer.Update( );
		return false;
	}

	// Too early, the engine might not fully restart yet.
	if ( !restartTimer.Check( 0.5f ) )
		return false;

	if ( !I::EngineClient->IsInGame( ) )
		return false;

	static Timer error_timer{};

	if ( !map || map->state != CNavParser::NavState::Active )
		return false;

	if ( !roundCheck && F::NavParser.IsSetupTime( ) )
		return false;

	return true;
}

static bool WasOn{ false };
void CNavEngine::CreateMove( CUserCmd* pCmd )
{
	//if ( !Config->Misc->NavBot->Enabled )
	//{
	//	WasOn = false;
	//}
	//else if ( !WasOn && gEngineClient->IsInGame( ) )
	//{
	//	WasOn = true;
	//	OnLevelInit( );
	//}

	const auto& pLocal = H::Entities.GetLocal( );

	if ( !pLocal /*|| F::Ticks.m_bRecharge*/ )
		return;

	if ( !pLocal->IsAlive( ) )
	{
		cancelPath( );
		return;
	}

	if ( ( current_priority == engineer && ( ( !Vars::Aimbot::Melee::AutoEngie::AutoRepair.Value && !Vars::Aimbot::Melee::AutoEngie::AutoUpgrade.Value ) || pLocal->m_iClass( ) != TF_CLASS_ENGINEER ) ) ||
		( current_priority == capture && !(Vars::Misc::Movement::NavBot::Preferences.Value & Vars::Misc::Movement::NavBot::PreferencesEnum::CaptureObjectives) ) )
	{
		cancelPath( );
		return;
	}

	if ( !pCmd || ( pCmd->buttons & ( IN_FORWARD | IN_BACK | IN_MOVERIGHT | IN_MOVELEFT ) && !F::Misc.m_bAntiAFK )
		|| !isReady( true ) )
		return;

	//map->UpdateRespawnRooms( );

	// Still in setup. If on fitting team and map, do not path yet.
	if ( F::NavParser.IsSetupTime( ) )
	{
		cancelPath( );
		return;
	}

	if ( Vars::Misc::Movement::NavEngine::VischeckEnabled.Value && !F::Ticks.m_bWarp && !F::Ticks.m_bDoubletap )
		vischeckPath( );

	checkBlacklist( );

	followCrumbs( pLocal, pCmd );

	updateStuckTime( );
	map->updateIgnores( );
}

void CNavEngine::abandonPath( )
{
	if ( !map )
		return;

	map->pather.Reset( );
	crumbs.clear( );
	last_crumb.navarea = nullptr;
	// We want to repath on failure
	if ( repath_on_fail )
		navTo( last_destination, current_priority, true, current_navtolocal, false );
	else
		current_priority = 0;
}

void CNavEngine::cancelPath( )
{
	crumbs.clear( );
	last_crumb.navarea = nullptr;
	current_priority = 0;
}

void CNavEngine::clearFreeBlacklist( ) const
{
	map->free_blacklist.clear( );
}

bool CanJumpIfScoped( CTFPlayer* pLocal, CTFWeaponBase* pWeapon )
{
	// You can still jump if youre scoped in water
	if ( pLocal->m_fFlags( ) & FL_INWATER )
		return true;

	const auto WeaponID = pWeapon->GetWeaponID( );
	return WeaponID == TF_WEAPON_SNIPERRIFLE_CLASSIC ? !pWeapon->As<CTFSniperRifleClassic>( )->m_bCharging( ) : !pLocal->InCond( TF_COND_ZOOMED );
}

void LookAtPath( CUserCmd* pCmd, const Vec2 vDest, const Vec3 vLocalEyePos, bool bSilent )
{
	static Vec3 LastAngles{};
	Vec3 next{ vDest.x, vDest.y, vLocalEyePos.z };
	next = Math::CalcAngle( vLocalEyePos, next );

	const int aim_speed = 25; // how smooth nav is/ im cringing at this damn speed.
	// activate nav spin and smoothen
	F::NavParser.DoSlowAim( next, aim_speed, LastAngles );
	if ( bSilent )
		pCmd->viewangles = next;
	else
		I::EngineClient->SetViewAngles( next );
	LastAngles = next;
}

static Timer last_jump;
// Used to determine if we want to jump or if we want to crouch
static bool crouch{ false };
static int ticks_since_jump{ 0 };
void CNavEngine::followCrumbs( CTFPlayer* pLocal, CUserCmd* pCmd )
{
	size_t crumbs_amount = crumbs.size( );

	// No more crumbs, reset status
	if ( !crumbs_amount )
	{
		// Invalidate last crumb
		last_crumb.navarea = nullptr;

		repath_on_fail = false;
		current_priority = 0;
		return;
	}

	if ( current_crumb.navarea != crumbs[ 0 ].navarea )
			time_spent_on_crumb.Update( );
	current_crumb = crumbs[ 0 ];

	// Ensure we do not try to walk downwards unless we are falling
	static std::vector<float> fall_vec{};
	Vector vel;
	pLocal->EstimateAbsVelocity( vel );

	fall_vec.push_back( vel.z );
	if ( fall_vec.size( ) > 10 )
		fall_vec.erase( fall_vec.begin( ) );

	bool reset_z = true;
	for ( const auto& entry : fall_vec )
	{
		if ( !( entry <= 0.01f && entry >= -0.01f ) )
		{
			reset_z = false;
		}
	}

	const auto vLocalOrigin = pLocal->GetAbsOrigin( );

	if ( reset_z && !F::Ticks.m_bWarp && !F::Ticks.m_bDoubletap )
	{
		reset_z = false;

		Vector end = vLocalOrigin;
		end.z -= 100.0f;
		
		CGameTrace trace;
		CTraceFilterHitscan Filter{};
		Filter.pSkip = pLocal;
		SDK::TraceHull( vLocalOrigin, end, pLocal->m_vecMins( ), pLocal->m_vecMaxs( ), MASK_PLAYERSOLID, &Filter, &trace );

		// Only reset if we are standing on a building
		if ( trace.DidHit( ) && trace.m_pEnt && trace.m_pEnt->IsBuilding( ) )
			reset_z = true;
	}

	Vector current_vec = crumbs[ 0 ].vec;
	if ( reset_z )
		current_vec.z = vLocalOrigin.z;

	// We are close enough to the crumb to have reached it
	if ( current_vec.DistToSqr( vLocalOrigin ) < pow( 50.0f, 2 ) )
	{
		last_crumb = crumbs[ 0 ];
		crumbs.erase( crumbs.begin( ) );
		time_spent_on_crumb.Update( );
		if ( !--crumbs_amount )
			return;
		inactivity.Update( );
	}

	current_vec = crumbs[ 0 ].vec;
	if ( reset_z )
		current_vec.z = vLocalOrigin.z;

	// We are close enough to the second crumb, Skip both (This is especially helpful with drop-downs)
	if ( crumbs.size( ) > 1 && crumbs[ 1 ].vec.DistToSqr( vLocalOrigin ) < pow( 50.0f, 2 ) )
	{
		last_crumb = crumbs[ 1 ];
		crumbs.erase( crumbs.begin( ), std::next( crumbs.begin( ) ) );
		--crumbs_amount;
		if ( !--crumbs_amount )
			return;
		inactivity.Update( );
	}
	// If we make any progress at all, reset this
	else
	{
		// If we spend way too long on this crumb, ignore the logic below
		if ( !time_spent_on_crumb.Check( Vars::Misc::Movement::NavEngine::StuckDetectTime.Value ) )
		{
			
			Vec3 vel3d;
			pLocal->EstimateAbsVelocity( vel3d );
			Vector2D vel = Vector2D( vel3d.x, vel3d.y );
			// 44.0f -> Revved brass beast, do not use z axis as jumping counts towards that. Yes this will mean long falls will trigger it, but that is not really bad.
			if ( !vel.IsZero( 40.0f ) )
			{
				inactivity.Update( );
			}
			else
			{
				SDK::Output( "CNavEngine", std::format( "Spent too much time on the crumb, assuming were stuck" ).c_str( ), { 255, 131, 131, 255 }, Vars::Debug::Logging.Value );
			}
		}
	}

	const auto& pWeapon = pLocal->m_hActiveWeapon( ).Get()->As<CTFWeaponBase>();
	//if ( !G::DoubleTap && !G::Warp )
	{
		// Detect when jumping is necessary.
		// 1. No jumping if zoomed (or revved)
		// 2. Jump if it's necessary to do so based on z values
		// 3. Jump if stuck (not getting closer) for more than stuck_time/2
		if ( pWeapon && !pWeapon->IsDormant( ) )
		{
			const auto WepID = pWeapon->GetWeaponID( );
			if ( ( WepID != TF_WEAPON_SNIPERRIFLE &&
				WepID != TF_WEAPON_SNIPERRIFLE_CLASSIC &&
				WepID != TF_WEAPON_SNIPERRIFLE_DECAP ) || 
				CanJumpIfScoped(pLocal, pWeapon) )
			{
				if ( WepID != TF_WEAPON_MINIGUN || !( pCmd->buttons & IN_ATTACK2 ) )
				{
					bool should_jump = false;
					float height_diff = crumbs[0].vec.z - pLocal->GetAbsOrigin().z;
        
					// Check if we need to jump
					if (height_diff > 18.0f && height_diff <= PLAYER_JUMP_HEIGHT)
					{
						should_jump = true;
					}
					// Also jump if we're stuck and it might help
					else if (inactivity.Check(Vars::Misc::Movement::NavEngine::StuckTime.Value / 2))
					{
						auto local = map->findClosestNavSquare(pLocal->GetAbsOrigin());
						if (!local || !(local->m_attributeFlags & (NAV_MESH_NO_JUMP | NAV_MESH_STAIRS)))
							should_jump = true;
					}
					if (should_jump && last_jump.Check(0.2f))
					{
						// Make it crouch until we land, but jump the first tick
						pCmd->buttons |= crouch ? IN_DUCK : IN_JUMP;

						// Only flip to crouch state, not to jump state
						if (!crouch)
						{
							crouch = true;
							ticks_since_jump = 0;
						}
						ticks_since_jump++;

						// Update jump timer now since we are back on ground
						if (crouch && pLocal->OnSolid() && ticks_since_jump > 3 )
						{
							// Reset
							crouch = false;
							last_jump.Update();
						}
					}
				}
			}
		}
	}

	const auto vLocalEyePos = pLocal->GetEyePosition( );

	if ( G::Attacking != 1 )
	{
		// Look at path (nav spin) (smooth nav)
		if ( Vars::Misc::Movement::NavEngine::LookAtPath.Value )
		{
			LookAtPath( pCmd, { crumbs[ 0 ].vec.x, crumbs[ 0 ].vec.y }, vLocalEyePos, Vars::Misc::Movement::NavEngine::LookAtPath.Value == Vars::Misc::Movement::NavEngine::LookAtPathEnum::Silent && ( !Vars::AntiHack::AntiAim::Enabled.Value || !G::AntiAim ) );
		}
	}
	
	SDK::WalkTo( pCmd, pLocal, current_vec );
}