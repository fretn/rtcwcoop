/*
===========================================================================

Return to Castle Wolfenstein single player GPL Source Code
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company. 

This file is part of the Return to Castle Wolfenstein single player GPL Source Code (RTCW SP Source Code).  

RTCW SP Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

RTCW SP Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with RTCW SP Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the RTCW SP Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the RTCW SP Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

/*
 * name:		g_weapon.c
 *
 * desc:		perform the server side effects of a weapon firing
 *
*/


#include "g_local.h"

static float s_quadFactor;
static vec3_t forward, right, up;
static vec3_t muzzleEffect;
static vec3_t muzzleTrace;


// forward dec
void weapon_zombiespit( gentity_t *ent );

void Bullet_Fire( gentity_t *ent, float spread, int damage );
void Bullet_Fire_Extended( gentity_t *source, gentity_t *attacker, vec3_t start, vec3_t end, float spread, int damage, int recursion );

void Weapon_Medic( gentity_t *ent );
void Weapon_MagicAmmo( gentity_t *ent );

void weapon_callAirStrike( gentity_t *ent );
void G_ExplodeMissile( gentity_t *ent );

int G_GetWeaponDamage( int weapon ); // JPW

#define NUM_NAILSHOTS 10

/*
======================================================================

KNIFE/GAUNTLET (NOTE: gauntlet is now the Zombie melee)

======================================================================
*/

#define KNIFE_DIST 48

/*
==============
Weapon_Knife
==============
*/
void Weapon_Knife( gentity_t *ent ) {
	trace_t tr;
	gentity_t   *traceEnt, *tent;
	int damage, mod;
//	vec3_t		pforward, eforward;

	vec3_t end;

	mod = MOD_KNIFE;

	AngleVectors( ent->client->ps.viewangles, forward, right, up );
	CalcMuzzlePoint( ent, ent->s.weapon, forward, right, up, muzzleTrace );
	VectorMA( muzzleTrace, KNIFE_DIST, forward, end );

	// et sdk antilag
	//trap_Trace( &tr, muzzleTrace, NULL, NULL, end, ent->s.number, MASK_SHOT );
	G_HistoricalTrace( ent, &tr, muzzleTrace, NULL, NULL, end, ent->s.number, MASK_SHOT_NOCORPSE );

	if ( tr.surfaceFlags & SURF_NOIMPACT ) {
		return;
	}

	// no contact
	if ( tr.fraction == 1.0f ) {
		return;
	}

	if ( tr.entityNum >= MAX_CLIENTS ) {   // world brush or non-player entity (no blood)
		tent = G_TempEntity( tr.endpos, EV_MISSILE_MISS );
	} else {                            // other player
		tent = G_TempEntity( tr.endpos, EV_MISSILE_HIT );
	}

	tent->s.otherEntityNum = tr.entityNum;
	tent->s.eventParm = DirToByte( tr.plane.normal );
	tent->s.weapon = ent->s.weapon;

	if ( tr.entityNum == ENTITYNUM_WORLD ) { // don't worry about doing any damage
		return;
	}

	traceEnt = &g_entities[ tr.entityNum ];

	if ( !( traceEnt->takedamage ) ) {
		return;
	}

	// RF, no knife damage for big guys
	switch ( traceEnt->aiCharacter ) {
	case AICHAR_PROTOSOLDIER:
	case AICHAR_SUPERSOLDIER:
	case AICHAR_HEINRICH:
		return;
	}

	damage = G_GetWeaponDamage( ent->s.weapon ); // JPW		// default knife damage for frontal attacks

	if ( traceEnt->client ) {
		if ( ent->client->ps.serverCursorHint == HINT_KNIFE ) {
//		AngleVectors (ent->client->ps.viewangles,		pforward, NULL, NULL);
//		AngleVectors (traceEnt->client->ps.viewangles,	eforward, NULL, NULL);

			// (SA) TODO: neutralize pitch (so only yaw is considered)
//		if(DotProduct( eforward, pforward ) > 0.9f)	{	// from behind

			// if relaxed, the strike is almost assured a kill
			// if not relaxed, but still from behind, it does 10x damage (50)

// (SA) commented out right now as the ai's state always checks here as 'combat'

//			if(ent->s.aiState == AISTATE_RELAXED) {
			damage = 100;       // enough to drop a 'normal' (100 health) human with one jab
			mod = MOD_KNIFE_STEALTH;
//			} else {
//				damage *= 10;
//			}
//----(SA)	end
		}
	}

	G_Damage( traceEnt, ent, ent, vec3_origin, tr.endpos, ( damage + rand() % 5 ) * s_quadFactor, 0, mod );
}

void MagicSink( gentity_t *self ) {

        self->clipmask = 0;
        self->r.contents = 0;

        if ( self->timestamp < level.time ) {
                self->think = G_FreeEntity;
                self->nextthink = level.time + FRAMETIME;
                return;
        }

        self->s.pos.trBase[2] -= 0.5f;
        self->nextthink = level.time + 50;
}

// JPW NERVE
/*
======================
  Weapon_Class_Special
	class-specific in multiplayer
======================
*/
// JPW NERVE
#if 0
void Weapon_Medic( gentity_t *ent ) {
	//gitem_t *item;
	//gentity_t *ent2;
	//vec3_t velocity, org, offset;
	vec3_t org;
	//vec3_t angles;

	trace_t tr;
	gentity_t   *traceEnt;
	int healamt, headshot;

	vec3_t end;

	AngleVectors( ent->client->ps.viewangles, forward, right, up );
	CalcMuzzlePointForActivate( ent, forward, right, up, muzzleTrace );
	VectorMA( muzzleTrace, 30, forward, end );           // CH_ACTIVATE_DIST
	trap_Trace( &tr, muzzleTrace, NULL, NULL, end, ent->s.number, MASK_SHOT );

	if ( tr.fraction < 1.0 ) {
		traceEnt = &g_entities[ tr.entityNum ];
		if ( traceEnt->client != NULL ) {
			if ( ( traceEnt->client->ps.pm_type == PM_DEAD ) && ( traceEnt->client->sess.sessionTeam == ent->client->sess.sessionTeam ) ) {
				/*
				if ( level.time - ent->client->ps.classWeaponTime > g_medicChargeTime.integer ) {
					ent->client->ps.classWeaponTime = level.time - g_medicChargeTime.integer;
				}
				*/
				ent->client->ps.classWeaponTime += 125;
				traceEnt->client->medicHealAmt++;
				if ( ent->client->ps.classWeaponTime > level.time ) { // heal the dude
					// copy some stuff out that we'll wanna restore
					VectorCopy( traceEnt->client->ps.origin, org );
					healamt = traceEnt->client->medicHealAmt;
					headshot = traceEnt->client->ps.eFlags & EF_HEADSHOT;

					ClientSpawn( traceEnt, qtrue );
					if ( healamt > 80 ) {
						healamt = 80;
					}
					if ( healamt < 10 ) {
						healamt = 10;
					}
					if ( headshot ) {
						traceEnt->client->ps.eFlags |= EF_HEADSHOT;
					}
					traceEnt->health = healamt;
					VectorCopy( org,traceEnt->s.origin );
					VectorCopy( org,traceEnt->r.currentOrigin );
					VectorCopy( org,traceEnt->client->ps.origin );
				}
			}
		}
	} else { // throw out health pack
		/*
		if ( level.time - ent->client->ps.classWeaponTime >= g_medicChargeTime.integer * 0.25f ) {
			if ( level.time - ent->client->ps.classWeaponTime > g_medicChargeTime.integer ) {
				ent->client->ps.classWeaponTime = level.time - g_medicChargeTime.integer;
			}
			ent->client->ps.classWeaponTime += g_medicChargeTime.integer * 0.25;

			VectorCopy( ent->client->ps.viewangles, angles );
			angles[PITCH] = 0;  // always forward
			AngleVectors( angles, velocity, NULL, NULL );
			VectorScale( velocity, 75, offset );
			VectorScale( velocity, 50, velocity );
			velocity[2] += 50 + crandom() * 50;

			VectorAdd( ent->client->ps.origin,offset,org );
		}
		*/
	}
}
#endif
// jpw

// DHM - Nerve
void Weapon_Engineer( gentity_t *ent ) {
	trace_t tr;
	gentity_t   *traceEnt;
//	int			mod = MOD_KNIFE;

	vec3_t end;

	AngleVectors( ent->client->ps.viewangles, forward, right, up );
	CalcMuzzlePointForActivate( ent, forward, right, up, muzzleTrace );
	VectorMA( muzzleTrace, 96, forward, end );           // CH_ACTIVATE_DIST
	trap_Trace( &tr, muzzleTrace, NULL, NULL, end, ent->s.number, MASK_SHOT | CONTENTS_TRIGGER );

	if ( tr.surfaceFlags & SURF_NOIMPACT ) {
		return;
	}

	// no contact
	if ( tr.fraction == 1.0f ) {
		return;
	}

	if ( tr.entityNum == ENTITYNUM_NONE || tr.entityNum == ENTITYNUM_WORLD ) {
		return;
	}

	traceEnt = &g_entities[ tr.entityNum ];
	if ( traceEnt->methodOfDeath == MOD_DYNAMITE ) {

		traceEnt->health += 3;
		if ( traceEnt->health >= 248 ) {
			traceEnt->health = 255;
			// Need some kind of event/announcement here

			Add_Ammo( ent, WP_DYNAMITE, 1, qtrue );

			traceEnt->think = G_FreeEntity;
			traceEnt->nextthink = level.time + FRAMETIME;
// JPW NERVE
			if ( ent->client->sess.sessionTeam == TEAM_RED ) {
				trap_SendServerCommand( -1, "cp \"Axis engineer disarmed a det charge!\n\"" );
			} else {
				trap_SendServerCommand( -1, "cp \"Allied engineer disarmed a det charge!\n\"" );
			}
// jpw
		}
	} else if ( !traceEnt->takedamage && !Q_stricmp( traceEnt->classname, "misc_mg42" ) )       {
		// "Ammo" for this weapon is time based
		/*
		if ( ent->client->ps.classWeaponTime + g_engineerChargeTime.integer < level.time ) {
			ent->client->ps.classWeaponTime = level.time - g_engineerChargeTime.integer;
		}
		*/
		ent->client->ps.classWeaponTime += 150;

		if ( ent->client->ps.classWeaponTime > level.time ) {
			ent->client->ps.classWeaponTime = level.time;
			return;     // Out of "ammo"
		}

		if ( traceEnt->health >= 255 ) {
			traceEnt->s.frame = 0;

			if ( traceEnt->mg42BaseEnt > 0 ) {
				g_entities[ traceEnt->mg42BaseEnt ].health = 100;
				g_entities[ traceEnt->mg42BaseEnt ].takedamage = qtrue;
				traceEnt->health = 0;
			} else {
				traceEnt->health = 100;
			}

			traceEnt->takedamage = qtrue;

			trap_SendServerCommand( ent - g_entities, "cp \"You have repaired the MG42!\n\"" );
		} else {
			traceEnt->health += 3;
		}
	}
}

void Weapon_Class_Special( gentity_t *ent ) {

	switch ( ent->client->ps.stats[STAT_PLAYER_CLASS] ) {
	case PC_SOLDIER:
		G_Printf( "shooting soldier\n" );
		break;
	case PC_MEDIC:
		Weapon_Medic( ent );
		break;
	case PC_ENGINEER:
		//G_Printf("shooting engineer\n");
		//ent->client->ps.classWeaponTime = level.time;
		Weapon_Engineer( ent );
		break;
	case PC_LT:
		/*
		if ( level.time - ent->client->ps.classWeaponTime > g_LTChargeTime.integer ) {
			weapon_grenadelauncher_fire( ent,WP_GRENADE_SMOKE );
			ent->client->ps.classWeaponTime = level.time;
		}
		*/
		break;
	}
}
// jpw

/*
======================
  Weapon_Syringe
        shoot the syringe, do the old lazarus bit
======================
*/
void Weapon_Syringe( gentity_t *ent ) {
        vec3_t end,org;
        trace_t tr;
        int healamt, headshot, oldweapon,oldweaponstate,oldclasstime = 0;
        qboolean usedSyringe = qfalse;          // DHM - Nerve
        int ammo[MAX_WEAPONS];              // JPW NERVE total amount of ammo
        int ammoclip[MAX_WEAPONS];          // JPW NERVE ammo in clip
        int weapons[MAX_WEAPONS / ( sizeof( int ) * 8 )];   // JPW NERVE 64 bits for weapons held
        gentity_t   *traceEnt, *te;

        AngleVectors( ent->client->ps.viewangles, forward, right, up );
        CalcMuzzlePointForActivate( ent, forward, right, up, muzzleTrace );
        VectorMA( muzzleTrace, 48, forward, end );           // CH_ACTIVATE_DIST
        //VectorMA (muzzleTrace, -16, forward, muzzleTrace);    // DHM - Back up the start point in case medic is
        // right on top of intended revivee.
        trap_Trace( &tr, muzzleTrace, NULL, NULL, end, ent->s.number, MASK_SHOT );

        if ( tr.startsolid ) {
                VectorMA( muzzleTrace, 8, forward, end );            // CH_ACTIVATE_DIST
                trap_Trace( &tr, muzzleTrace, NULL, NULL, end, ent->s.number, MASK_SHOT );
        }

        if ( tr.fraction < 1.0 ) {
                traceEnt = &g_entities[ tr.entityNum ];
                if ( traceEnt->client != NULL ) {

                        if ( ( traceEnt->client->ps.pm_type == PM_DEAD ) && ( traceEnt->client->sess.sessionTeam == ent->client->sess.sessionTeam ) ) {

                                // heal the dude
                                // copy some stuff out that we'll wanna restore
                                VectorCopy( traceEnt->client->ps.origin, org );
                                headshot = traceEnt->client->ps.eFlags & EF_HEADSHOT;
                                healamt = traceEnt->client->ps.stats[STAT_MAX_HEALTH] * 0.5;
                                oldweapon = traceEnt->client->ps.weapon;
                                oldweaponstate = traceEnt->client->ps.weaponstate;

                                // keep class special weapon time to keep them from exploiting revives
                                oldclasstime = traceEnt->client->ps.classWeaponTime;

                                memcpy( ammo,traceEnt->client->ps.ammo,sizeof( int ) * MAX_WEAPONS );
                                memcpy( ammoclip,traceEnt->client->ps.ammoclip,sizeof( int ) * MAX_WEAPONS );
                                memcpy( weapons,traceEnt->client->ps.weapons,sizeof( int ) * ( MAX_WEAPONS / ( sizeof( int ) * 8 ) ) );

                                ClientSpawn( traceEnt, qtrue );

                                memcpy( traceEnt->client->ps.ammo,ammo,sizeof( int ) * MAX_WEAPONS );
                                memcpy( traceEnt->client->ps.ammoclip,ammoclip,sizeof( int ) * MAX_WEAPONS );
                                memcpy( traceEnt->client->ps.weapons,weapons,sizeof( int ) * ( MAX_WEAPONS / ( sizeof( int ) * 8 ) ) );

                                if ( headshot ) {
                                        traceEnt->client->ps.eFlags |= EF_HEADSHOT;
                                }
                                traceEnt->client->ps.weapon = oldweapon;
                                traceEnt->client->ps.weaponstate = oldweaponstate;

                                traceEnt->client->ps.classWeaponTime = oldclasstime;

                                traceEnt->health = healamt;
                                VectorCopy( org,traceEnt->s.origin );
                                VectorCopy( org,traceEnt->r.currentOrigin );
                                VectorCopy( org,traceEnt->client->ps.origin );

                                trap_Trace( &tr, traceEnt->client->ps.origin, traceEnt->client->ps.mins, traceEnt->client->ps.maxs, traceEnt->client->ps.origin, traceEnt->s.number, MASK_PLAYERSOLID );
                                if ( tr.allsolid ) {
                                        traceEnt->client->ps.pm_flags |= PMF_DUCKED;
                                }

                                traceEnt->s.effect3Time = level.time;
                                traceEnt->r.contents = CONTENTS_CORPSE;
                                trap_LinkEntity( ent );

                                // DHM - Nerve :: Let the person being revived know about it
                                trap_SendServerCommand( traceEnt - g_entities, va( "cp \"You have been revived by [lof]%s!\n\"", ent->client->pers.netname ) );
                                traceEnt->props_frame_state = ent->s.number;

                                // DHM - Nerve :: Mark that the medicine was indeed dispensed
                                usedSyringe = qtrue;

                                // sound
                                te = G_TempEntity( traceEnt->r.currentOrigin, EV_GENERAL_SOUND );
                                te->s.eventParm = G_SoundIndex( "sound/multiplayer/vo_revive.wav" );

                                // DHM - Nerve :: Play revive animation

                                // Xian -- This was gay and I always hated it.
                                if ( g_fastres.integer > 0 ) {
                                        BG_AnimScriptEvent( &traceEnt->client->ps, ANIM_ET_JUMP, qfalse, qtrue );
                                } else {
                                        BG_AnimScriptEvent( &traceEnt->client->ps, ANIM_ET_REVIVE, qfalse, qtrue );
                                        traceEnt->client->ps.pm_flags |= PMF_TIME_LOCKPLAYER;
                                        traceEnt->client->ps.pm_time = 2100;
                                }


                                AddScore( ent, MEDIC_BONUS ); // JPW NERVE props to the medic for the swift and dexterous bit o healitude
                        }
                }
        }

        // DHM - Nerve :: If the medicine wasn't used, give back the ammo
        if ( !usedSyringe ) {
                ent->client->ps.ammoclip[BG_FindClipForWeapon( WP_MEDIC_SYRINGE )] += 1;
        }
}
// jpw

/*
==============
Weapon_Gauntlet
==============
*/
void Weapon_Gauntlet( gentity_t *ent ) {
	trace_t *tr;
	tr = CheckMeleeAttack( ent, 32, qfalse );
	if ( tr ) {
		G_Damage( &g_entities[tr->entityNum], ent, ent, vec3_origin, tr->endpos,
				  ( 10 + rand() % 5 ) * s_quadFactor, 0, MOD_GAUNTLET );
	}
}

/*
===============
CheckMeleeAttack
	using 'isTest' to return hits to world surfaces
===============
*/
trace_t *CheckMeleeAttack( gentity_t *ent, float dist, qboolean isTest ) {
	static trace_t tr;
	vec3_t end;
	gentity_t   *tent;
	gentity_t   *traceEnt;

	// set aiming directions
	AngleVectors( ent->client->ps.viewangles, forward, right, up );

	CalcMuzzlePoint( ent, WP_GAUNTLET, forward, right, up, muzzleTrace );

	VectorMA( muzzleTrace, dist, forward, end );

	trap_Trace( &tr, muzzleTrace, NULL, NULL, end, ent->s.number, MASK_SHOT );
	if ( tr.surfaceFlags & SURF_NOIMPACT ) {
		return NULL;
	}

	// no contact
	if ( tr.fraction == 1.0f ) {
		return NULL;
	}

	if ( ent->client->noclip ) {
		return NULL;
	}

	traceEnt = &g_entities[ tr.entityNum ];

	// send blood impact
	if ( traceEnt->takedamage && traceEnt->client ) {
		tent = G_TempEntity( tr.endpos, EV_MISSILE_HIT );
		tent->s.otherEntityNum = traceEnt->s.number;
		tent->s.eventParm = DirToByte( tr.plane.normal );
		tent->s.weapon = ent->s.weapon;
	}

//----(SA)	added
	if ( isTest ) {
		return &tr;
	}
//----(SA)

	if ( !traceEnt->takedamage ) {
		return NULL;
	}

	if ( ent->client->ps.powerups[PW_QUAD] ) {
		G_AddEvent( ent, EV_POWERUP_QUAD, 0 );
		s_quadFactor = g_quadfactor.value;
	} else {
		s_quadFactor = 1;
	}

	return &tr;
}


/*
======================================================================

MACHINEGUN

======================================================================
*/

/*
======================
SnapVectorTowards

Round a vector to integers for more efficient network
transmission, but make sure that it rounds towards a given point
rather than blindly truncating.  This prevents it from truncating
into a wall.
======================
*/

// (SA) modified so it doesn't have trouble with negative locations (quadrant problems)
//			(this was causing some problems with bullet marks appearing since snapping
//			too far off the target surface causes the the distance between the transmitted impact
//			point and the actual hit surface larger than the mark radius.  (so nothing shows) )

void SnapVectorTowards( vec3_t v, vec3_t to ) {
	int i;

	for ( i = 0 ; i < 3 ; i++ ) {
		if ( to[i] <= v[i] ) {
			v[i] = floor( v[i] );
		} else {
			v[i] = ceil( v[i] );
		}
	}
}

// JPW
// mechanism allows different weapon damage for single/multiplayer; we want "balanced" weapons
// in multiplayer but don't want to alter the existing single-player damage items that have already
// been changed
//
// KLUDGE/FIXME: also modded #defines below to become macros that call this fn for minimal impact elsewhere
//
int G_GetWeaponDamage( int weapon ) {
	switch ( weapon )
	{
		case WP_LUGER:
		case WP_SILENCER: return 6;
		case WP_COLT: return 8;
		case WP_AKIMBO: return 8;           //----(SA)	added
		case WP_VENOM: return 12;           // 15  ----(SA)	slight modify for DM
		case WP_MP40: return 6;
		case WP_THOMPSON: return 8;
		case WP_STEN: return 10;
		case WP_FG42SCOPE:
		case WP_FG42: return 15;
		case WP_MAUSER: return 20;
		case WP_GARAND: return 25;
		case WP_SNIPERRIFLE: return 55;
		case WP_SNOOPERSCOPE: return 25;
		case WP_NONE: return 0;
		case WP_KNIFE: return 5;
		case WP_GRENADE_LAUNCHER: return 100;
		case WP_GRENADE_PINEAPPLE: return 80;
		case WP_SMOKE_GRENADE: return 80;
		case WP_DYNAMITE: return 400;
		case WP_PANZERFAUST: return 200;            // (SA) was 100
		case WP_MORTAR: return 100;
		case WP_FLAMETHROWER:         // FIXME -- not used in single player yet
		case WP_TESLA:
		case WP_GAUNTLET:
		case WP_SNIPER:
		default:    return 1;
	}
}

// RF, wrote this so we can dynamically switch between old and new values while testing g_userAim
float G_GetWeaponSpread( int weapon ) {
	if ( g_userAim.integer ) {
		// these should be higher since they become erratic if aiming is out
		switch ( weapon )
		{
			case WP_LUGER:      return 600;
			case WP_SILENCER:   return 900;
			case WP_COLT:       return 700;
			case WP_AKIMBO:     return 700;     //----(SA)	added
			case WP_VENOM:      return 1000;
			case WP_MP40:       return 1000;
			case WP_FG42SCOPE:  return 300;
			case WP_FG42:       return 800;
			case WP_THOMPSON:   return 1200;
			case WP_STEN:       return 1200;
			case WP_MAUSER:     return 400;
			case WP_GARAND:     return 500;
			case WP_SNIPERRIFLE:    return 300;
			case WP_SNOOPERSCOPE:   return 300;
		}
	} else {        // old values
		switch ( weapon )
		{
			case WP_LUGER:      return 25;
			case WP_SILENCER:   return 150;
			case WP_COLT:       return 30;
			case WP_AKIMBO:     return 30;          //----(SA)	added
			case WP_VENOM:      return 200;
			case WP_MP40:       return 200;
			case WP_FG42SCOPE:  return 10;
			case WP_FG42:       return 150;
			case WP_THOMPSON:   return 250;
			case WP_STEN:       return 300;
			case WP_MAUSER:     return 15;
			case WP_GARAND:     return 25;
			case WP_SNIPERRIFLE:    return 10;
			case WP_SNOOPERSCOPE:   return 10;		
		}
	}
	G_Printf( "shouldn't ever get here (weapon %d)\n",weapon );

	return 0;   // shouldn't get here
}

#define LUGER_SPREAD    G_GetWeaponSpread( WP_LUGER )
#define LUGER_DAMAGE    G_GetWeaponDamage( WP_LUGER ) // JPW
#define SILENCER_SPREAD G_GetWeaponSpread( WP_SILENCER )
#define COLT_SPREAD     G_GetWeaponSpread( WP_COLT )
#define COLT_DAMAGE     G_GetWeaponDamage( WP_COLT ) // JPW

#define VENOM_SPREAD    G_GetWeaponSpread( WP_VENOM )
#define VENOM_DAMAGE    G_GetWeaponDamage( WP_VENOM ) // JPW

#define MP40_SPREAD     G_GetWeaponSpread( WP_MP40 )
#define MP40_DAMAGE     G_GetWeaponDamage( WP_MP40 ) // JPW
#define THOMPSON_SPREAD G_GetWeaponSpread( WP_THOMPSON )
#define THOMPSON_DAMAGE G_GetWeaponDamage( WP_THOMPSON ) // JPW
#define STEN_SPREAD     G_GetWeaponSpread( WP_STEN )
#define STEN_DAMAGE     G_GetWeaponDamage( WP_STEN ) // JPW
#define FG42_SPREAD     G_GetWeaponSpread( WP_FG42 )
#define FG42_DAMAGE     G_GetWeaponDamage( WP_FG42 ) // JPW

#define MAUSER_SPREAD   G_GetWeaponSpread( WP_MAUSER )
#define MAUSER_DAMAGE   G_GetWeaponDamage( WP_MAUSER ) // JPW
#define GARAND_SPREAD   G_GetWeaponSpread( WP_GARAND )
#define GARAND_DAMAGE   G_GetWeaponDamage( WP_GARAND ) // JPW

#define SNIPER_SPREAD   G_GetWeaponSpread( WP_SNIPERRIFLE )
#define SNIPER_DAMAGE   G_GetWeaponDamage( WP_SNIPERRIFLE ) // JPW

#define SNOOPER_SPREAD  G_GetWeaponSpread( WP_SNOOPERSCOPE )
#define SNOOPER_DAMAGE  G_GetWeaponDamage( WP_SNOOPERSCOPE ) // JPW

/*
==============
SP5_Fire

  dead code
==============
*/


/*
==============
Cross_Fire
==============
*/
void Cross_Fire( gentity_t *ent ) {
// (SA) temporarily use the zombie spit effect to check working state
	weapon_zombiespit( ent );
}



/*
==============
Tesla_Fire
==============
*/
void Tesla_Fire( gentity_t *ent ) {
	// TODO: Find all targets in the client's view frame, and lock onto them all, applying damage
	// and telling all clients to draw the appropriate effects.

	//G_Printf("TODO: Tesla damage/effects\n" );
}



void RubbleFlagCheck( gentity_t *ent, trace_t tr ) {
#if 0 // (SA) moving client-side
	qboolean is_valid = qfalse;
	int type = 0;

	if ( tr.surfaceFlags & SURF_RUBBLE || tr.surfaceFlags & SURF_GRAVEL ) {
		is_valid = qtrue;
		type = 4;
	} else if ( tr.surfaceFlags & SURF_METAL )     {
//----(SA)	removed
//		is_valid = qtrue;
//		type = 2;
	} else if ( tr.surfaceFlags & SURF_WOOD )     {
		is_valid = qtrue;
		type = 1;
	}

	if ( is_valid && ent->client && ( ent->s.weapon == WP_VENOM
									  || ent->client->ps.persistant[PERS_HWEAPON_USE] ) ) {
		if ( rand() % 100 > 75 ) {
			gentity_t   *sfx;
			vec3_t start;
			vec3_t dir;

			sfx = G_Spawn();

			sfx->s.density = type;

			VectorCopy( tr.endpos, start );

			VectorCopy( muzzleTrace, dir );
			VectorNegate( dir, dir );

			G_SetOrigin( sfx, start );
			G_SetAngle( sfx, dir );

			G_AddEvent( sfx, EV_SHARD, DirToByte( dir ) );

			sfx->think = G_FreeEntity;
			sfx->nextthink = level.time + 1000;

			sfx->s.frame = 3 + ( rand() % 3 ) ;

			trap_LinkEntity( sfx );

		}
	}
#endif
}

/*
==============
EmitterCheck
	see if a new particle emitter should be created at the bullet impact point
==============
*/
void EmitterCheck( gentity_t *ent, gentity_t *attacker, trace_t *tr ) {
	gentity_t *tent;
	vec3_t origin;

	if ( !ent->emitNum ) { // no emitters left for this entity.
		return;
	}

	VectorCopy( tr->endpos, origin );
	SnapVectorTowards( tr->endpos, attacker->s.origin ); // make sure it's out of the wall


	// why were these stricmp's?...
	if ( ent->s.eType == ET_EXPLOSIVE ) {
	} else if ( ent->s.eType == ET_LEAKY ) {

		tent = G_TempEntity( origin, EV_EMITTER );
		VectorCopy( origin, tent->s.origin );
		tent->s.time = ent->emitTime;
		tent->s.density = ent->emitPressure;    // 'pressure'
		tent->s.teamNum = ent->emitID;          // 'type'
		VectorCopy( tr->plane.normal, tent->s.origin2 );
	}

	ent->emitNum--;
}


void SniperSoundEFX( vec3_t pos ) {
	G_TempEntity( pos, EV_SNIPER_SOUND );
}


/*
==============
Bullet_Endpos
	find target end position for bullet trace based on entities weapon and accuracy
==============
*/
void Bullet_Endpos( gentity_t *ent, float spread, vec3_t *end ) {
	float r, u;
	qboolean randSpread = qtrue;
	int dist = 8192;

	r = crandom() * spread;
	u = crandom() * spread;

	// Ridah, if this is an AI shooting, apply their accuracy
	if ( ent->r.svFlags & SVF_CASTAI ) {
		float accuracy;
		accuracy = ( 1.0 - AICast_GetAccuracy( ent->s.number ) ) * AICAST_AIM_SPREAD;
		r += crandom() * accuracy;
		u += crandom() * ( accuracy * 1.25 );
	} else {
		if ( ent->s.weapon == WP_SNOOPERSCOPE || ent->s.weapon == WP_SNIPERRIFLE || ent->s.weapon == WP_FG42SCOPE ) {
//		if(ent->s.weapon == WP_SNOOPERSCOPE || ent->s.weapon == WP_SNIPERRIFLE) {
			// aim dir already accounted for sway of scoped weapons in CalcMuzzlePoints()
			dist *= 2;
			randSpread = qfalse;
		}
	}

	VectorMA( muzzleTrace, dist, forward, *end );

	if ( randSpread ) {
		VectorMA( *end, r, right, *end );
		VectorMA( *end, u, up, *end );
	}
}

/*
==============
Bullet_Fire
==============
*/
void Bullet_Fire( gentity_t *ent, float spread, int damage ) {
	vec3_t end;

	Bullet_Endpos( ent, spread, &end );

	// et sdk antilag
	if ( g_antilag.integer && g_gametype.integer != GT_SINGLE_PLAYER ) {
		G_HistoricalTraceBegin( ent );
	}

	Bullet_Fire_Extended( ent, ent, muzzleTrace, end, spread, damage, 0 );

	// et sdk antilag
	if ( g_antilag.integer && g_gametype.integer != GT_SINGLE_PLAYER ) {
		G_HistoricalTraceEnd( ent );
	}
}


/*
==============
Bullet_Fire_Extended
	A modified Bullet_Fire with more parameters.
	The original Bullet_Fire still passes through here and functions as it always has.

	uses for this include shooting through entities (windows, doors, other players, etc.) and reflecting bullets
==============
*/
void Bullet_Fire_Extended( gentity_t *source, gentity_t *attacker, vec3_t start, vec3_t end, float spread, int damage, int recursion ) {
	trace_t tr;
	gentity_t   *tent;
	gentity_t   *traceEnt;
	int dflags = 0;         // flag if source==attacker, meaning it wasn't shot directly, but was reflected went through an entity that allows bullets to pass through
	qboolean reflectBullet = qfalse;

	// RF, abort if too many recursions.. there must be a real solution for this, but for now this is the safest
	// fix I can find
	if ( recursion > 12 ) {
		return;
	}

	damage *= s_quadFactor;

	if ( source != attacker ) {
		dflags = DAMAGE_PASSTHRU;
	}

	// (SA) changed so player could shoot his own dynamite.
	// (SA) whoops, but that broke bullets going through explosives...
//	trap_Trace( &tr, start, NULL, NULL, end, source->s.number, MASK_SHOT ); // this was the original used line
//	trap_Trace (&tr, start, NULL, NULL, end, ENTITYNUM_NONE, MASK_SHOT);

	// et sdk antilag
	// Using MASK_SHOT so we can gib corpses.
	G_Trace( source, &tr, start, NULL, NULL, end, source->s.number, MASK_SHOT );

	AICast_ProcessBullet( attacker, start, tr.endpos );

	// bullet debugging using Q3A's railtrail
	if ( g_debugBullets.integer & 1 ) {
		tent = G_TempEntity( start, EV_RAILTRAIL );
		VectorCopy( tr.endpos, tent->s.origin2 );
		tent->s.otherEntityNum2 = attacker->s.number;
	}


	RubbleFlagCheck( attacker, tr );

	traceEnt = &g_entities[ tr.entityNum ];

	EmitterCheck( traceEnt, attacker, &tr );

	// snap the endpos to integers, but nudged towards the line
	SnapVectorTowards( tr.endpos, start );

	// should we reflect this bullet?
	if ( traceEnt->flags & FL_DEFENSE_GUARD ) {
		reflectBullet = qtrue;
	} else if ( traceEnt->flags & FL_DEFENSE_CROUCH ) {
		if ( rand() % 3 < 2 ) {
			reflectBullet = qtrue;
		}
	}

	// send bullet impact
	if ( traceEnt->takedamage && traceEnt->client && !reflectBullet ) {
		tent = G_TempEntity( tr.endpos, EV_BULLET_HIT_FLESH );
		tent->s.eventParm = traceEnt->s.number;
		if ( LogAccuracyHit( traceEnt, attacker ) ) {
			attacker->client->ps.persistant[PERS_ACCURACY_HITS]++;
		}

//----(SA)	added
		if ( g_debugBullets.integer >= 2 ) {   // show hit player bb
			gentity_t *bboxEnt;
			vec3_t b1, b2;
			VectorCopy( traceEnt->r.currentOrigin, b1 );
			VectorCopy( traceEnt->r.currentOrigin, b2 );
			VectorAdd( b1, traceEnt->r.mins, b1 );
			VectorAdd( b2, traceEnt->r.maxs, b2 );
			bboxEnt = G_TempEntity( b1, EV_RAILTRAIL );
			VectorCopy( b2, bboxEnt->s.origin2 );
			bboxEnt->s.dmgFlags = 1;    // ("type")
		}
//----(SA)	end

	} else if ( traceEnt->takedamage && traceEnt->s.eType == ET_BAT ) {
		tent = G_TempEntity( tr.endpos, EV_BULLET_HIT_FLESH );
		tent->s.eventParm = traceEnt->s.number;
	} else {
		// Ridah, bullet impact should reflect off surface
		vec3_t reflect;
		float dot;

		if ( g_debugBullets.integer <= -2 ) {  // show hit thing bb
			gentity_t *bboxEnt;
			vec3_t b1, b2;
			VectorCopy( traceEnt->r.currentOrigin, b1 );
			VectorCopy( traceEnt->r.currentOrigin, b2 );
			VectorAdd( b1, traceEnt->r.mins, b1 );
			VectorAdd( b2, traceEnt->r.maxs, b2 );
			bboxEnt = G_TempEntity( b1, EV_RAILTRAIL );
			VectorCopy( b2, bboxEnt->s.origin2 );
			bboxEnt->s.dmgFlags = 1;    // ("type")
		}

		if ( reflectBullet ) {
			// reflect off sheild
			VectorSubtract( tr.endpos, traceEnt->r.currentOrigin, reflect );
			VectorNormalize( reflect );
			VectorMA( traceEnt->r.currentOrigin, 15, reflect, reflect );
			tent = G_TempEntity( reflect, EV_BULLET_HIT_WALL );
		} else {
			tent = G_TempEntity( tr.endpos, EV_BULLET_HIT_WALL );
		}

		dot = DotProduct( forward, tr.plane.normal );
		VectorMA( forward, -2 * dot, tr.plane.normal, reflect );
		VectorNormalize( reflect );

		tent->s.eventParm = DirToByte( reflect );

		if ( reflectBullet ) {
			tent->s.otherEntityNum2 = traceEnt->s.number;   // force sparks
		} else {
			tent->s.otherEntityNum2 = ENTITYNUM_NONE;
		}
		// done.
	}
	tent->s.otherEntityNum = attacker->s.number;

	if ( traceEnt->takedamage ) {
		qboolean reflectBool = qfalse;
		vec3_t trDir;

		if ( reflectBullet ) {
			// if we are facing the direction the bullet came from, then reflect it
			AngleVectors( traceEnt->s.apos.trBase, trDir, NULL, NULL );
			if ( DotProduct( forward, trDir ) < 0.6 ) {
				reflectBool = qtrue;
			}
		}

		if ( reflectBool ) {
			vec3_t reflect_end;
			// reflect this bullet
			G_AddEvent( traceEnt, EV_GENERAL_SOUND, level.bulletRicochetSound );
			CalcMuzzlePoints( traceEnt, traceEnt->s.weapon );

//----(SA)	modified to use extended version so attacker would pass through
//			Bullet_Fire( traceEnt, 1000, damage );
			Bullet_Endpos( traceEnt, 2800, &reflect_end );    // make it inaccurate
			Bullet_Fire_Extended( traceEnt, attacker, muzzleTrace, reflect_end, spread, damage, recursion + 1 );
//----(SA)	end

		} else {

			// Ridah, don't hurt team-mates
			// DHM - Nerve :: Only in single player
			if ( attacker->client && traceEnt->client && ( traceEnt->r.svFlags & SVF_CASTAI ) && ( attacker->r.svFlags & SVF_CASTAI ) && AICast_SameTeam( AICast_GetCastState( attacker->s.number ), traceEnt->s.number ) ) {
				// AI's don't hurt members of their own team
				return;
			}
			// done.

			G_Damage( traceEnt, attacker, attacker, forward, tr.endpos, damage, dflags, ammoTable[attacker->s.weapon].mod );

			// allow bullets to "pass through" func_explosives if they break by taking another simultanious shot
			// start new bullet at position this hit and continue to the end position (ignoring shot-through ent in next trace)
			// spread = 0 as this is an extension of an already spread shot (so just go straight through)
			if ( Q_stricmp( traceEnt->classname, "func_explosive" ) == 0 ) {
				if ( traceEnt->health <= 0 ) {
					Bullet_Fire_Extended( traceEnt, attacker, tr.endpos, end, 0, damage, recursion + 1 );
				}
			} else if ( traceEnt->client ) {
				if ( traceEnt->health <= 0 ) {
					Bullet_Fire_Extended( traceEnt, attacker, tr.endpos, end, 0, damage / 2, recursion + 1 ); // halve the damage each player it goes through
				}
			}
		}
	}
}



/*
======================================================================

GRENADE LAUNCHER

  700 has been the standard direction multiplier in fire_grenade()

======================================================================
*/
extern void G_ExplodeMissilePoisonGas( gentity_t *ent );

gentity_t *weapon_crowbar_throw( gentity_t *ent ) {
	gentity_t   *m;

	m = fire_crowbar( ent, muzzleEffect, forward );
	m->damage *= s_quadFactor;
	m->splashDamage *= s_quadFactor;

	return m;
}

gentity_t *weapon_grenadelauncher_fire_coop( gentity_t *ent, int grenType ) {
	gentity_t   *m, *te;     // JPW NERVE
	trace_t tr;
	vec3_t viewpos;
	float upangle = 0, pitch;                   //      start with level throwing and adjust based on angle
	vec3_t tosspos;
	qboolean underhand = qtrue;

	pitch = ent->s.apos.trBase[0];

	// JPW NERVE -- smoke grenades always overhand
	if ( pitch >= 0 ) {
		forward[2] += 0.5f;
		// Used later in underhand boost
		pitch = 1.3f;
	} else {
		pitch = -pitch;
		pitch = min( pitch, 30 );
		pitch /= 30.f;
		pitch = 1 - pitch;
		forward[2] += ( pitch * 0.5f );

		// Used later in underhand boost
		pitch *= 0.3f;
		pitch += 1.f;
	}

	VectorNormalizeFast( forward );             //      make sure forward is normalized

	upangle = -( ent->s.apos.trBase[0] );     //        this will give between  -90 / 90
	upangle = min( upangle, 50 );
	upangle = max( upangle, -50 );            //        now clamped to  -50 / 50        (don't allow firing straight up/down)
	upangle = upangle / 100.0f;               //                                   -0.5 / 0.5
	upangle += 0.5f;                        //                              0.0 / 1.0

	if ( upangle < .1 ) {
		upangle = .1;
	}

	// pineapples are not thrown as far as mashers
	if ( grenType == WP_GRENADE_LAUNCHER ) {
		upangle *= 900;
	} else if ( grenType == WP_GRENADE_PINEAPPLE ) {
		upangle *= 900;
	} else if ( grenType == WP_GRENADE_SMOKE ) {
		upangle *= 900;
	} else if ( grenType == WP_SMOKE_GRENADE ) {
		upangle *= 900;
	} else {     // WP_DYNAMITE
		upangle *= 400;
	}

	VectorCopy( muzzleEffect, tosspos );

	if ( underhand ) {
		// move a little bit more away from the player (so underhand tosses don't get caught on nearby lips)
		VectorMA( muzzleEffect, 8, forward, tosspos );
		tosspos[2] -= 8;            // lower origin for the underhand throw
		upangle *= pitch;
		SnapVector( tosspos );
	}

	VectorScale( forward, upangle, forward );
	// check for valid start spot (so you don't throw through or get stuck in a wall)
	VectorCopy( ent->s.pos.trBase, viewpos );
	viewpos[2] += ent->client->ps.viewheight;

	if ( grenType == WP_DYNAMITE ) {
		trap_Trace( &tr, viewpos, tv( -12.f, -12.f, 0.f ), tv( 12.f, 12.f, 20.f ), tosspos, ent->s.number, MASK_MISSILESHOT );
	} else {
		trap_Trace( &tr, viewpos, tv( -4.f, -4.f, 0.f ), tv( 4.f, 4.f, 6.f ), tosspos, ent->s.number, MASK_MISSILESHOT );
	}

	if ( tr.fraction < 1 ) {       // oops, bad launch spot
		VectorCopy( tr.endpos, tosspos );
		SnapVectorTowards( tosspos, viewpos );
	}

	m = fire_grenade( ent, tosspos, forward, grenType );

	m->damage = 0;      // Ridah, grenade's don't explode on contact
	m->splashDamage *= s_quadFactor;

	// JPW NERVE
	if ( grenType == WP_GRENADE_SMOKE || grenType == WP_SMOKE_GRENADE) {

		if ( ent->client->sess.sessionTeam == TEAM_RED ) {         // store team so we can generate red or blue smoke
			m->s.otherEntityNum2 = 1;
		} else {
			m->s.otherEntityNum2 = 0;
		}
		m->nextthink = level.time + 4000;
		m->think = weapon_callAirStrike;

		te = G_TempEntity( m->s.pos.trBase, EV_GLOBAL_SOUND );
		te->s.eventParm = G_SoundIndex( "sound/multiplayer/airstrike_01.wav" );
		te->r.svFlags |= SVF_BROADCAST | SVF_USE_CURRENT_ORIGIN;
	}
	// jpw

	//----(SA)      adjust for movement of character.  TODO: Probably comment in later, but only for forward/back not strafing
	//VectorAdd( m->s.pos.trDelta, ent->client->ps.velocity, m->s.pos.trDelta );    // "real" physics

	// let the AI know which grenade it has fired
	ent->grenadeFired = m->s.number;

	// Ridah, return the grenade so we can do some prediction before deciding if we really want to throw it or not
	return m;
}

gentity_t *weapon_grenadelauncher_fire( gentity_t *ent, int grenType ) {
	gentity_t   *m, *te; // JPW NERVE
	float upangle = 0;                  //	start with level throwing and adjust based on angle
	vec3_t tosspos;
	qboolean underhand = 0;

	if ( ( ent->s.apos.trBase[0] > 0 ) && ( grenType != WP_GRENADE_SMOKE && grenType != WP_SMOKE_GRENADE) ) { // JPW NERVE -- smoke grenades always overhand
		underhand = qtrue;
	}

	if ( underhand ) {
		forward[2] = 0;                 //	start the toss level for underhand
	} else {
		forward[2] += 0.2;              //	extra vertical velocity for overhand

	}
	VectorNormalize( forward );         //	make sure forward is normalized

	upangle = -( ent->s.apos.trBase[0] ); //	this will give between	-90 / 90
	upangle = min( upangle, 50 );
	upangle = max( upangle, -50 );        //	now clamped to			-50 / 50	(don't allow firing straight up/down)
	upangle = upangle / 100.0f;           //						   -0.5 / 0.5
	upangle += 0.5f;                    //						    0.0 / 1.0

	if ( upangle < .1 ) {
		upangle = .1;
	}

	// pineapples are not thrown as far as mashers
	if ( grenType == WP_GRENADE_LAUNCHER ) {
		upangle *= 800;     //									    0.0 / 800.0
	} else if ( grenType == WP_GRENADE_PINEAPPLE )                                {
		if ( g_gametype.integer != GT_SINGLE_PLAYER ) {
			upangle *= 800;
		} else {
			upangle *= 600;     //									    0.0 / 600.0
		}
	} else if ( grenType == WP_GRENADE_SMOKE )   { // smoke grenades *really* get chucked
		upangle *= 800;
	} else if ( grenType == WP_SMOKE_GRENADE )   { // smoke grenades *really* get chucked
		upangle *= 900;
	} else {      // WP_DYNAMITE
		upangle *= 400;     //										0.0 / 100.0

	}
	/*
	if(ent->aiCharacter)
	{
		VectorScale(forward, 700, forward);				//----(SA)	700 is the default grenade throw they are already used to
		m = fire_grenade (ent, muzzleTrace, forward);	//----(SA)	temp to make AI's throw grenades at their actual target
	}
	else
	*/



	{
		VectorCopy( muzzleEffect, tosspos );
		if ( underhand ) {
			VectorMA( muzzleEffect, 15, forward, tosspos );   // move a little bit more away from the player (so underhand tosses don't get caught on nearby lips)
			tosspos[2] -= 24;   // lower origin for the underhand throw
			upangle *= 1.3;     // a little more force to counter the lower position / lack of additional lift
		}

		VectorScale( forward, upangle, forward );


		{
			// check for valid start spot (so you don't throw through or get stuck in a wall)
			trace_t tr;
			vec3_t viewpos;

			VectorCopy( ent->s.pos.trBase, viewpos );
			viewpos[2] += ent->client->ps.viewheight;

			trap_Trace( &tr, viewpos, NULL, NULL, tosspos, ent->s.number, MASK_SHOT );
			if ( tr.fraction < 1 ) {   // oops, bad launch spot
				VectorCopy( tr.endpos, tosspos );
			}
		}


		m = fire_grenade( ent, tosspos, forward, grenType );
	}


	//m->damage *= s_quadFactor;
	m->damage = 0;  // Ridah, grenade's don't explode on contact
	m->splashDamage *= s_quadFactor;

	if ( ent->aiCharacter == AICHAR_VENOM ) { // poison gas grenade
		m->think = G_ExplodeMissilePoisonGas;
		m->s.density = 1;
	}

// JPW NERVE
	if ( grenType == WP_GRENADE_SMOKE || grenType == WP_SMOKE_GRENADE) {
		if ( ent->client->sess.sessionTeam == TEAM_RED ) { // store team so we can generate red or blue smoke
			m->s.otherEntityNum2 = 1;
		} else {
			m->s.otherEntityNum2 = 0;
		}
		m->nextthink = level.time + 4000;
		m->think = weapon_callAirStrike;

		te = G_TempEntity( m->s.pos.trBase, EV_GLOBAL_SOUND );
		te->s.eventParm = G_SoundIndex( "sound/scenaric/forest/me109_flight.wav" );
		te->r.svFlags |= SVF_BROADCAST | SVF_USE_CURRENT_ORIGIN;
	}
// jpw

	//----(SA)	adjust for movement of character.  TODO: Probably comment in later, but only for forward/back not strafing
//	VectorAdd( m->s.pos.trDelta, ent->client->ps.velocity, m->s.pos.trDelta );	// "real" physics

	// let the AI know which grenade it has fired
	ent->grenadeFired = m->s.number;

	// Ridah, return the grenade so we can do some prediction before deciding if we really want to throw it or not
	return m;
}

/*
=====================
Zombie spit
=====================
*/
void weapon_zombiespit( gentity_t *ent ) {
	return;
#if 0 //RF, HARD disable
	gentity_t *m;

	m = fire_zombiespit( ent, muzzleTrace, forward );
	m->damage *= s_quadFactor;
	m->splashDamage *= s_quadFactor;

	if ( m ) {
		G_AddEvent( ent, EV_GENERAL_SOUND, G_SoundIndex( "sound/Loogie/spit.wav" ) );
	}
#endif
}

/*
=====================
Zombie spirit
=====================
*/
void weapon_zombiespirit( gentity_t *ent, gentity_t *missile ) {
	gentity_t *m;

	m = fire_zombiespirit( ent, missile, muzzleTrace, forward );
	m->damage *= s_quadFactor;
	m->splashDamage *= s_quadFactor;

	if ( m ) {
		G_AddEvent( ent, EV_GENERAL_SOUND, G_SoundIndex( "zombieAttackPlayer" ) );
	}

}

//----(SA)	modified this entire "venom" section
/*
============================================================================

VENOM GUN TRACING

============================================================================
*/
#define DEFAULT_VENOM_COUNT 10
#define DEFAULT_VENOM_SPREAD 20
#define DEFAULT_VENOM_DAMAGE 15

qboolean VenomPellet( vec3_t start, vec3_t end, gentity_t *ent ) {
	trace_t tr;
	int damage;
	gentity_t       *traceEnt;

	trap_Trace( &tr, start, NULL, NULL, end, ent->s.number, MASK_SHOT );
	traceEnt = &g_entities[ tr.entityNum ];

	// send bullet impact
	if (  tr.surfaceFlags & SURF_NOIMPACT ) {
		return qfalse;
	}

	if ( traceEnt->takedamage ) {
		damage = DEFAULT_VENOM_DAMAGE * s_quadFactor;

		G_Damage( traceEnt, ent, ent, forward, tr.endpos, damage, 0, MOD_VENOM );
		if ( LogAccuracyHit( traceEnt, ent ) ) {
			return qtrue;
		}
	}
	return qfalse;
}

// this should match CG_VenomPattern
void VenomPattern( vec3_t origin, vec3_t origin2, int seed, gentity_t *ent ) {
	int i;
	float r, u;
	vec3_t end;
	vec3_t forward, right, up;
	qboolean hitClient = qfalse;

	// derive the right and up vectors from the forward vector, because
	// the client won't have any other information
	VectorNormalize2( origin2, forward );
	PerpendicularVector( right, forward );
	CrossProduct( forward, right, up );

	// generate the "random" spread pattern
	for ( i = 0 ; i < DEFAULT_VENOM_COUNT ; i++ ) {
		r = Q_crandom( &seed ) * DEFAULT_VENOM_SPREAD;
		u = Q_crandom( &seed ) * DEFAULT_VENOM_SPREAD;
		VectorMA( origin, 8192, forward, end );
		VectorMA( end, r, right, end );
		VectorMA( end, u, up, end );
		if ( VenomPellet( origin, end, ent ) && !hitClient ) {
			hitClient = qtrue;
			ent->client->ps.persistant[PERS_ACCURACY_HITS]++;
		}
	}
}



/*
==============
weapon_venom_fire
==============
*/
void weapon_venom_fire( gentity_t *ent, qboolean fullmode, float aimSpreadScale ) {
	gentity_t       *tent;

	if ( fullmode ) {
		tent = G_TempEntity( muzzleTrace, EV_VENOMFULL );
	} else {
		tent = G_TempEntity( muzzleTrace, EV_VENOM );
	}

	VectorScale( forward, 4096, tent->s.origin2 );
	SnapVector( tent->s.origin2 );
	tent->s.eventParm = rand() & 255;       // seed for spread pattern
	tent->s.otherEntityNum = ent->s.number;

	if ( fullmode ) {
		VenomPattern( tent->s.pos.trBase, tent->s.origin2, tent->s.eventParm, ent );
	} else
	{
		int dam;
		dam = VENOM_DAMAGE;
		if ( ent->aiCharacter ) {  // venom guys are /vicious/
			dam *= 0.5f;
		}
		Bullet_Fire( ent, VENOM_SPREAD * aimSpreadScale, dam );
	}
}





/*
======================================================================

ROCKET

======================================================================
*/

void Weapon_RocketLauncher_Fire( gentity_t *ent, float aimSpreadScale ) {
//	trace_t		tr;
	float r, u;
	vec3_t dir, launchpos;     //, viewpos, wallDir;
	gentity_t   *m;

	// get a little bit of randomness and apply it back to the direction
	if ( !ent->aiCharacter ) {
		r = crandom() * aimSpreadScale;
		u = crandom() * aimSpreadScale;

		VectorScale( forward, 16, dir );
		VectorMA( dir, r, right, dir );
		VectorMA( dir, u, up, dir );
		VectorNormalize( dir );

		VectorCopy( muzzleEffect, launchpos );

		// check for valid start spot (so you don't lose it in a wall)
		// (doesn't ever happen)
//		VectorCopy( ent->s.pos.trBase, viewpos );
//		viewpos[2] += ent->client->ps.viewheight;
//		trap_Trace (&tr, viewpos, NULL, NULL, muzzleEffect, ent->s.number, MASK_SHOT);
//		if(tr.fraction < 1) {	// oops, bad launch spot
///			VectorCopy(tr.endpos, launchpos);
//			VectorSubtract(tr.endpos, viewpos, wallDir);
//			VectorNormalize(wallDir);
//			VectorMA(tr.endpos, -5, wallDir, launchpos);
//		}

		m = fire_rocket( ent, launchpos, dir );

		// add kick-back
		VectorMA( ent->client->ps.velocity, -64, forward, ent->client->ps.velocity );

	} else {
		m = fire_rocket( ent, muzzleEffect, forward );
	}

	m->damage *= s_quadFactor;
	m->splashDamage *= s_quadFactor;

//	VectorAdd( m->s.pos.trDelta, ent->client->ps.velocity, m->s.pos.trDelta );	// "real" physics
}




/*
======================================================================

LIGHTNING GUN

======================================================================
*/

// RF, not used anymore for Flamethrower (still need it for tesla?)
void Weapon_LightningFire( gentity_t *ent ) {
	trace_t tr;
	vec3_t end;
	gentity_t   *traceEnt;
	int damage;

	damage = 5 * s_quadFactor;

	VectorMA( muzzleTrace, LIGHTNING_RANGE, forward, end );

	trap_Trace( &tr, muzzleTrace, NULL, NULL, end, ent->s.number, MASK_SHOT );

	if ( tr.entityNum == ENTITYNUM_NONE ) {
		return;
	}

	traceEnt = &g_entities[ tr.entityNum ];
/*
	if ( traceEnt->takedamage && traceEnt->client ) {
		tent = G_TempEntity( tr.endpos, EV_MISSILE_HIT );
		tent->s.otherEntityNum = traceEnt->s.number;
		tent->s.eventParm = DirToByte( tr.plane.normal );
		tent->s.weapon = ent->s.weapon;
		if( LogAccuracyHit( traceEnt, ent ) ) {
			ent->client->ps.persistant[PERS_ACCURACY_HITS]++;
		}
	} else if ( !( tr.surfaceFlags & SURF_NOIMPACT ) ) {
		tent = G_TempEntity( tr.endpos, EV_MISSILE_MISS );
		tent->s.eventParm = DirToByte( tr.plane.normal );
	}
*/
	if ( traceEnt->takedamage && !AICast_NoFlameDamage( traceEnt->s.number ) ) {
		#define FLAME_THRESHOLD 50

		// RF, only do damage once they start burning
		//if (traceEnt->health > 0)	// don't explode from flamethrower
		//	G_Damage( traceEnt, ent, ent, forward, tr.endpos, 1, 0, MOD_LIGHTNING);

		// now check the damageQuota to see if we should play a pain animation
		// first reduce the current damageQuota with time
		if ( traceEnt->flameQuotaTime && traceEnt->flameQuota > 0 ) {
			traceEnt->flameQuota -= (int)( ( (float)( level.time - traceEnt->flameQuotaTime ) / 1000 ) * (float)damage / 2.0 );
			if ( traceEnt->flameQuota < 0 ) {
				traceEnt->flameQuota = 0;
			}
		}

		// add the new damage
		traceEnt->flameQuota += damage;
		traceEnt->flameQuotaTime = level.time;

		// Ridah, make em burn
		if ( traceEnt->client && ( traceEnt->health <= 0 || traceEnt->flameQuota > FLAME_THRESHOLD ) ) {
			if ( traceEnt->s.onFireEnd < level.time ) {
				traceEnt->s.onFireStart = level.time;
			}
			if ( traceEnt->health <= 0 || !( traceEnt->r.svFlags & SVF_CASTAI ) ) {
				if ( traceEnt->r.svFlags & SVF_CASTAI ) {
					traceEnt->s.onFireEnd = level.time + 6000;
				} else {
					traceEnt->s.onFireEnd = level.time + FIRE_FLASH_TIME;
				}
			} else {
				traceEnt->s.onFireEnd = level.time + 99999; // make sure it goes for longer than they need to die
			}
			traceEnt->flameBurnEnt = ent->s.number;
			// add to playerState for client-side effect
			traceEnt->client->ps.onFireStart = level.time;
		}
	}
}


/*
======================================================================

FLAMETHROWER

======================================================================
*/

void G_BurnMeGood( gentity_t *self, gentity_t *body ) {
	// add the new damage
	body->flameQuota += 5;
	body->flameQuotaTime = level.time;

	// JPW NERVE -- yet another flamethrower damage model, trying to find a feels-good damage combo that isn't overpowered
	if ( body->lastBurnedFrameNumber != level.framenum ) {
		G_Damage( body,self->parent,self->parent,vec3_origin,self->r.currentOrigin,5,0,MOD_FLAMETHROWER );   // was 2 dmg in release ver, hit avg. 2.5 times per frame
		body->lastBurnedFrameNumber = level.framenum;
	}
	// jpw

	// make em burn
	if ( body->client && ( body->health <= 0 || body->flameQuota > 0 ) ) { // JPW NERVE was > FLAME_THRESHOLD
		if ( body->s.onFireEnd < level.time ) {
			body->s.onFireStart = level.time;
		}

		body->s.onFireEnd = level.time + FIRE_FLASH_TIME;
		body->flameBurnEnt = self->s.number;
		// add to playerState for client-side effect
		body->client->ps.onFireStart = level.time;
	}
}

// those are used in the cg_ traces calls
static vec3_t flameChunkMins = {-4, -4, -4};
static vec3_t flameChunkMaxs = { 4,  4,  4};

void Weapon_FlamethrowerFire( gentity_t *ent ) {
	vec3_t start;
	vec3_t trace_start;
	vec3_t trace_end;
	trace_t trace;

	VectorCopy( ent->r.currentOrigin, start );
	start[2] += ent->client->ps.viewheight;
	VectorCopy( start, trace_start );

	VectorMA( start, -8, forward, start );
	VectorMA( start, 10, right, start );
	VectorMA( start, -6, up, start );

	// prevent flame thrower cheat, run & fire while aiming at the ground, don't get hurt
	// 72 total box height, 18 xy -> 77 trace radius (from view point towards the ground) is enough to cover the area around the feet
	VectorMA( trace_start, 77.0, forward, trace_end );
	trap_Trace( &trace, trace_start, flameChunkMins, flameChunkMaxs, trace_end, ent->s.number, MASK_SHOT | MASK_WATER );
	if ( trace.fraction != 1.0 ) {
		// additional checks to filter out false positives
		if ( trace.endpos[2] > ( ent->r.currentOrigin[2] + ent->r.mins[2] - 8 ) && trace.endpos[2] < ent->r.currentOrigin[2] ) {
			// trigger in a 21 radius around origin
			trace_start[0] -= trace.endpos[0];
			trace_start[1] -= trace.endpos[1];
			if ( trace_start[0] * trace_start[0] + trace_start[1] * trace_start[1] < 441 ) {
				// set self in flames
				G_BurnMeGood( ent, ent );
			}
		}
	}

	fire_flamechunk( ent, start, forward );
}

void G_AirStrikeExplode( gentity_t *self ) {

        self->r.svFlags &= ~SVF_NOCLIENT;
        self->r.svFlags |= SVF_BROADCAST;

        self->think = G_ExplodeMissile;
        self->nextthink = level.time + 50;
}


#define NUMBOMBS 10
#define BOMBSPREAD 150
void weapon_callAirStrike( gentity_t *ent ) {
	int i;
	vec3_t bombaxis, lookaxis, pos, bomboffset, fallaxis, temp;
	gentity_t *bomb,*te;
	trace_t tr;
	float traceheight, bottomtraceheight;

	VectorCopy( ent->s.pos.trBase,bomboffset );
	bomboffset[2] += 4096;

	// cancel the airstrike if FF off and player joined spec
	if ( !g_friendlyFire.integer && ent->parent->client && ent->parent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
		ent->splashDamage = 0; // no damage
		ent->think = G_ExplodeMissile;
		ent->nextthink = level.time + crandom() * 50;
		return; // do nothing, don't hurt anyone
	}

	// turn off smoke grenade
	ent->think = G_ExplodeMissile;
	ent->nextthink = level.time + 950 + NUMBOMBS * 100 + crandom() * 50; // 3000 offset is for aircraft flyby

	trap_Trace( &tr, ent->s.pos.trBase, NULL, NULL, bomboffset, ent->s.number, MASK_SHOT );
	if ( ( tr.fraction < 1.0 ) && ( !( tr.surfaceFlags & SURF_NOIMPACT ) ) ) { //SURF_SKY)) ) { // JPW NERVE changed for trenchtoast foggie prollem
		G_SayTo( ent->parent, ent->parent, 2, COLOR_YELLOW, "Pilot: ", "Aborting, can't see target.");

		if ( ent->parent->client && ent->parent->client->sess.sessionTeam == TEAM_BLUE ) {
			te = G_TempEntity( ent->parent->s.pos.trBase, EV_CLIENT_SOUND );
			te->s.eventParm = G_SoundIndex( "sound/multiplayer/allies/a-aborting.wav" );
			te->s.teamNum = ent->parent->s.clientNum;
		} else {
			te = G_TempEntity( ent->parent->s.pos.trBase, EV_CLIENT_SOUND );
			te->s.eventParm = G_SoundIndex( "sound/multiplayer/axis/g-aborting.wav" );
			te->s.teamNum = ent->parent->s.clientNum;
		}

		return;
	}

	if ( ent->parent->client && ent->parent->client->sess.sessionTeam == TEAM_BLUE ) {
		te = G_TempEntity( ent->parent->s.pos.trBase, EV_CLIENT_SOUND );
		te->s.eventParm = G_SoundIndex( "sound/multiplayer/allies/a-affirmative_omw.wav" );
		te->s.teamNum = ent->parent->s.clientNum;
	} else {
		te = G_TempEntity( ent->parent->s.pos.trBase, EV_CLIENT_SOUND );
		te->s.eventParm = G_SoundIndex( "sound/multiplayer/axis/g-affirmative_omw.wav" );
		te->s.teamNum = ent->parent->s.clientNum;
	}

	VectorCopy( tr.endpos, bomboffset );
	traceheight = bomboffset[2];
	bottomtraceheight = traceheight - 8192;

	VectorSubtract( ent->s.pos.trBase,ent->parent->client->ps.origin,lookaxis );
	lookaxis[2] = 0;
	VectorNormalize( lookaxis );
	pos[0] = 0;
	pos[1] = 0;
	pos[2] = crandom(); // generate either up or down vector,
	VectorNormalize( pos ); // which adds randomness to pass direction below
	RotatePointAroundVector( bombaxis,pos,lookaxis,90 + crandom() * 30 ); // munge the axis line a bit so it's not totally perpendicular
	VectorNormalize( bombaxis );

	VectorCopy( bombaxis,pos );
	VectorScale( pos,(float)( -0.5f * BOMBSPREAD * NUMBOMBS ),pos );
	VectorAdd( ent->s.pos.trBase, pos, pos ); // first bomb position
	VectorScale( bombaxis,BOMBSPREAD,bombaxis ); // bomb drop direction offset

	for ( i = 0; i < NUMBOMBS; i++ ) {
		bomb = G_Spawn();
		bomb->nextthink = level.time + i * 100 + crandom() * 50 + 1000; // 1000 for aircraft flyby, other term for tumble stagger
		bomb->think = G_AirStrikeExplode;
		bomb->s.eType       = ET_MISSILE;
		bomb->r.svFlags     = SVF_USE_CURRENT_ORIGIN | SVF_NOCLIENT;
		bomb->s.weapon      = WP_ARTY; // might wanna change this
		bomb->r.ownerNum    = ent->s.number;
		bomb->parent        = ent->parent;
		bomb->damage        = 400; // maybe should un-hard-code these?
		bomb->splashDamage  = 400;
		bomb->classname             = "air strike";
		bomb->splashRadius          = 400;
		bomb->methodOfDeath         = MOD_AIRSTRIKE;
		bomb->splashMethodOfDeath   = MOD_AIRSTRIKE;
		bomb->clipmask = MASK_MISSILESHOT;
		bomb->s.pos.trType = TR_STATIONARY; // was TR_GRAVITY,  might wanna go back to this and drop from height
		//bomb->s.pos.trTime = level.time;		// move a bit on the very first frame
		bomboffset[0] = crandom() * 0.5 * BOMBSPREAD;
		bomboffset[1] = crandom() * 0.5 * BOMBSPREAD;
		bomboffset[2] = 0;
		VectorAdd( pos,bomboffset,bomb->s.pos.trBase );

		VectorCopy( bomb->s.pos.trBase,bomboffset ); // make sure bombs fall "on top of" nonuniform scenery
		bomboffset[2] = traceheight;

		VectorCopy( bomboffset, fallaxis );
		fallaxis[2] = bottomtraceheight;

		trap_Trace( &tr, bomboffset, NULL, NULL, fallaxis, ent->s.number, MASK_SHOT );
		if ( tr.fraction != 1.0 ) {
			VectorCopy( tr.endpos,bomb->s.pos.trBase );
		}

		VectorClear( bomb->s.pos.trDelta );

		// Snap origin!
		VectorCopy( bomb->s.pos.trBase, temp );
		temp[2] += 2.f;
		SnapVectorTowards( bomb->s.pos.trBase, temp );          // save net bandwidth

		VectorCopy( bomb->s.pos.trBase, bomb->r.currentOrigin );

		// move pos for next bomb
		VectorAdd( pos,bombaxis,pos );
	}
}

// JPW NERVE -- sound effect for spotter round, had to do this as half-second bomb warning

void artilleryThink_real( gentity_t *ent ) {
	ent->freeAfterEvent = qtrue;
	trap_LinkEntity( ent );
	G_AddEvent( ent, EV_GENERAL_SOUND, G_SoundIndex( "sound/multiplayer/artillery_01.wav" ) );
}
void artilleryThink( gentity_t *ent ) {
	ent->think = artilleryThink_real;
	ent->nextthink = level.time + 100;

	ent->r.svFlags = SVF_USE_CURRENT_ORIGIN | SVF_BROADCAST;
}

// JPW NERVE -- makes smoke disappear after a bit (just unregisters stuff)
void artilleryGoAway( gentity_t *ent ) {
	ent->freeAfterEvent = qtrue;
	trap_LinkEntity( ent );
}

// JPW NERVE -- generates some smoke debris
void artillerySpotterThink( gentity_t *ent ) {
	gentity_t *bomb;
	vec3_t tmpdir;
	int i;
	ent->think = G_ExplodeMissile;
	ent->nextthink = level.time + 1;
	SnapVector( ent->s.pos.trBase );

	for ( i = 0; i < 7; i++ ) {
		bomb = G_Spawn();
		bomb->s.eType       = ET_MISSILE;
		bomb->r.svFlags     = SVF_USE_CURRENT_ORIGIN;
		bomb->r.ownerNum    = ent->s.number;
		bomb->parent        = ent;
		bomb->nextthink = level.time + 1000 + random() * 300;
		bomb->classname = "WP"; // WP == White Phosphorous, so we can check for bounce noise in grenade bounce routine
		bomb->damage        = 000; // maybe should un-hard-code these?
		bomb->splashDamage  = 000;
		bomb->splashRadius  = 000;
		bomb->s.weapon  = WP_SMOKETRAIL;
		bomb->think = artilleryGoAway;
		bomb->s.eFlags |= EF_BOUNCE;
		bomb->clipmask = MASK_MISSILESHOT;
		bomb->s.pos.trType = TR_GRAVITY; // was TR_GRAVITY,  might wanna go back to this and drop from height
		bomb->s.pos.trTime = level.time;        // move a bit on the very first frame
		bomb->s.otherEntityNum2 = ent->s.otherEntityNum2;
		VectorCopy( ent->s.pos.trBase,bomb->s.pos.trBase );
		tmpdir[0] = crandom();
		tmpdir[1] = crandom();
		tmpdir[2] = 1;
		VectorNormalize( tmpdir );
		tmpdir[2] = 1; // extra up
		VectorScale( tmpdir,500 + random() * 500,tmpdir );
		VectorCopy( tmpdir,bomb->s.pos.trDelta );
		SnapVector( bomb->s.pos.trDelta );          // save net bandwidth
		VectorCopy( ent->s.pos.trBase,bomb->s.pos.trBase );
		VectorCopy( ent->s.pos.trBase,bomb->r.currentOrigin );
	}
}

/*
==================
Weapon_Artillery
==================
*/
void Weapon_Artillery( gentity_t *ent ) {
        trace_t trace;
        int i = 0;
        vec3_t muzzlePoint,end,bomboffset,pos,fallaxis;
        float traceheight, bottomtraceheight;
        gentity_t *bomb,*bomb2,*te;

        if ( ent->client->ps.stats[STAT_PLAYER_CLASS] != PC_LT ) {
                G_Printf( "not a lieutenant, you can't shoot this!\n" );
                return;
        }
        if ( level.time - ent->client->ps.classWeaponTime > g_LTChargeTime.integer ) {

                AngleVectors( ent->client->ps.viewangles, forward, right, up );

                VectorCopy( ent->r.currentOrigin, muzzlePoint );
                muzzlePoint[2] += ent->client->ps.viewheight;

                VectorMA( muzzlePoint, 8192, forward, end );
                trap_Trace( &trace, muzzlePoint, NULL, NULL, end, ent->s.number, MASK_SHOT );

                if ( trace.surfaceFlags & SURF_NOIMPACT ) {
                        return;
                }

                VectorCopy( trace.endpos,pos );
                VectorCopy( pos,bomboffset );
                bomboffset[2] += 4096;

                trap_Trace( &trace, pos, NULL, NULL, bomboffset, ent->s.number, MASK_SHOT );
                if ( ( trace.fraction < 1.0 ) && ( !( trace.surfaceFlags & SURF_NOIMPACT ) ) ) { // JPW NERVE was SURF_SKY)) ) {
                        G_SayTo( ent, ent, 2, COLOR_YELLOW, "Fire Mission: ", "Aborting, can't see target.");

                        if ( ent->client->sess.sessionTeam == TEAM_BLUE ) {
                                te = G_TempEntity( ent->s.pos.trBase, EV_CLIENT_SOUND );
                                te->s.eventParm = G_SoundIndex( "sound/multiplayer/allies/a-art_abort.wav" );
                                te->s.teamNum = ent->s.clientNum;
                        } else {
                                te = G_TempEntity( ent->s.pos.trBase, EV_CLIENT_SOUND );
                                te->s.eventParm = G_SoundIndex( "sound/multiplayer/axis/g-art_abort.wav" );
                                te->s.teamNum = ent->s.clientNum;
                        }
                        return;
                }
                G_SayTo( ent, ent, 2, COLOR_YELLOW, "Fire Mission: ", "Firing for effect!");

                if ( ent->client->sess.sessionTeam == TEAM_BLUE ) {
                        te = G_TempEntity( ent->s.pos.trBase, EV_CLIENT_SOUND );
                        te->s.eventParm = G_SoundIndex( "sound/multiplayer/allies/a-firing.wav" );
                        te->s.teamNum = ent->s.clientNum;
                } else {
                        te = G_TempEntity( ent->s.pos.trBase, EV_CLIENT_SOUND );
                        te->s.eventParm = G_SoundIndex( "sound/multiplayer/axis/g-firing.wav" );
                        te->s.teamNum = ent->s.clientNum;
                }

                VectorCopy( trace.endpos, bomboffset );
                traceheight = bomboffset[2];
                bottomtraceheight = traceheight - 8192;


// "spotter" round (i == 0)
// i == 1->4 is regular explosives
                for ( i = 0; i < 5; i++ ) {
                        bomb = G_Spawn();
                        bomb->think = G_AirStrikeExplode;
                        bomb->s.eType       = ET_MISSILE;
                        bomb->r.svFlags     = SVF_USE_CURRENT_ORIGIN | SVF_NOCLIENT;
                        bomb->s.weapon      = WP_ARTY; // might wanna change this
                        bomb->r.ownerNum    = ent->s.number;
                        bomb->parent        = ent;
/*
                        if (i == 0) {
                                bomb->nextthink = level.time + 4500;
                                bomb->think = artilleryThink;
                        }
*/
                        if ( i == 0 ) {
                                bomb->nextthink = level.time + 5000;
                                bomb->r.svFlags     = SVF_USE_CURRENT_ORIGIN | SVF_BROADCAST;
                                bomb->classname = "props_explosion"; // was "air strike"
                                bomb->damage        = 0; // maybe should un-hard-code these?
                                bomb->splashDamage  = 90;
                                bomb->splashRadius  = 50;
//              bomb->s.weapon  = WP_SMOKE_GRENADE;
                                // TTimo ambiguous else
                                if ( ent->client != NULL ) { // set team color on smoke
                                        if ( ent->client->sess.sessionTeam == TEAM_RED ) { // store team so we can generate red or blue smoke
                                                bomb->s.otherEntityNum2 = 1;
                                        } else {
                                                bomb->s.otherEntityNum2 = 0;
                                        }
                                }
                                bomb->think = artillerySpotterThink;
                        } else {
                                bomb->nextthink = level.time + 8950 + 2000 * i + crandom() * 800;
                                bomb->classname = "air strike";
                                bomb->damage        = 0;
                                bomb->splashDamage  = 400;
                                bomb->splashRadius  = 400;
                        }
                        bomb->methodOfDeath         = MOD_AIRSTRIKE;
                        bomb->splashMethodOfDeath   = MOD_AIRSTRIKE;
                        bomb->clipmask = MASK_MISSILESHOT;
                        bomb->s.pos.trType = TR_STATIONARY; // was TR_GRAVITY,  might wanna go back to this and drop from height
                        bomb->s.pos.trTime = level.time;        // move a bit on the very first frame
                        if ( i ) { // spotter round is always dead on (OK, unrealistic but more fun)
                                bomboffset[0] = crandom() * 250;
                                bomboffset[1] = crandom() * 250;
                        } else {
                                bomboffset[0] = crandom() * 50; // was 0; changed per id request to prevent spotter round assassinations
                                bomboffset[1] = crandom() * 50; // was 0;
                        }
                        bomboffset[2] = 0;
                        VectorAdd( pos,bomboffset,bomb->s.pos.trBase );

                        VectorCopy( bomb->s.pos.trBase,bomboffset ); // make sure bombs fall "on top of" nonuniform scenery
                        bomboffset[2] = traceheight;

                        VectorCopy( bomboffset, fallaxis );
                        fallaxis[2] = bottomtraceheight;

                        trap_Trace( &trace, bomboffset, NULL, NULL, fallaxis, ent->s.number, MASK_SHOT );
                        if ( trace.fraction != 1.0 ) {
                                VectorCopy( trace.endpos,bomb->s.pos.trBase );
                        }

                        bomb->s.pos.trDelta[0] = 0; // might need to change this
                        bomb->s.pos.trDelta[1] = 0;
                        bomb->s.pos.trDelta[2] = 0;
                        SnapVector( bomb->s.pos.trDelta );          // save net bandwidth
                        VectorCopy( bomb->s.pos.trBase, bomb->r.currentOrigin );

// build arty falling sound effect in front of bomb drop
                        bomb2 = G_Spawn();
                        bomb2->think = artilleryThink;
                        bomb2->s.eType  = ET_MISSILE;
                        bomb2->r.svFlags    = SVF_USE_CURRENT_ORIGIN | SVF_NOCLIENT;
                        bomb2->r.ownerNum   = ent->s.number;
                        bomb2->parent       = ent;
                        bomb2->damage       = 0;
                        bomb2->nextthink = bomb->nextthink - 600;
                        bomb2->classname = "air strike";
                        bomb2->clipmask = MASK_MISSILESHOT;
                        bomb2->s.pos.trType = TR_STATIONARY; // was TR_GRAVITY,  might wanna go back to this and drop from height
                        bomb2->s.pos.trTime = level.time;       // move a bit on the very first frame
                        VectorCopy( bomb->s.pos.trBase,bomb2->s.pos.trBase );
                        VectorCopy( bomb->s.pos.trDelta,bomb2->s.pos.trDelta );
                        VectorCopy( bomb->s.pos.trBase,bomb2->r.currentOrigin );
                }
                ent->client->ps.classWeaponTime = level.time;
        }

}
//======================================================================


/*
==============
AddLean
	add leaning offset
==============
*/
void AddLean( gentity_t *ent, vec3_t point ) {
	if ( ent->client ) {
		if ( ent->client->ps.leanf ) {
			vec3_t right;
			AngleVectors( ent->client->ps.viewangles, NULL, right, NULL );
			VectorMA( point, ent->client->ps.leanf, right, point );
		}
	}
}

/*
===============
LogAccuracyHit
===============
*/
qboolean LogAccuracyHit( gentity_t *target, gentity_t *attacker ) {
	if ( !target->takedamage ) {
		return qfalse;
	}

	if ( target == attacker ) {
		return qfalse;
	}

	if ( !target->client ) {
		return qfalse;
	}

	if ( !attacker->client ) {
		return qfalse;
	}

	if ( target->client->ps.stats[STAT_HEALTH] <= 0 ) {
		return qfalse;
	}

	if ( OnSameTeam( target, attacker ) ) {
		return qfalse;
	}

	return qtrue;
}


/*
===============
CalcMuzzlePoint

set muzzle location relative to pivoting eye
===============
*/
void CalcMuzzlePoint( gentity_t *ent, int weapon, vec3_t forward, vec3_t right, vec3_t up, vec3_t muzzlePoint ) {
	VectorCopy( ent->r.currentOrigin, muzzlePoint );
	muzzlePoint[2] += ent->client->ps.viewheight;
	// Ridah, this puts the start point outside the bounding box, isn't necessary
//	VectorMA( muzzlePoint, 14, forward, muzzlePoint );
	// done.

	// Ridah, offset for more realistic firing from actual gun position
	//----(SA) modified
	switch ( weapon )  // Ridah, changed this so I can predict weapons
	{
	case WP_PANZERFAUST:
//			VectorMA( muzzlePoint, 14, right, muzzlePoint );	//----(SA)	new first person rl position
		VectorMA( muzzlePoint, 10, right, muzzlePoint );        //----(SA)	new first person rl position
		VectorMA( muzzlePoint, -10, up, muzzlePoint );
		break;
//		case WP_ROCKET_LAUNCHER:
//			VectorMA( muzzlePoint, 14, right, muzzlePoint );	//----(SA)	new first person rl position
//			break;
	case WP_DYNAMITE:
	case WP_GRENADE_PINEAPPLE:
	case WP_GRENADE_LAUNCHER:
		VectorMA( muzzlePoint, 20, right, muzzlePoint );
		break;
	case WP_AKIMBO:     // left side rather than right
		VectorMA( muzzlePoint, -6, right, muzzlePoint );
		VectorMA( muzzlePoint, -4, up, muzzlePoint );
	default:
		VectorMA( muzzlePoint, 6, right, muzzlePoint );
		VectorMA( muzzlePoint, -4, up, muzzlePoint );
		break;
	}

	// done.

	// (SA) actually, this is sort of moot right now since
	// you're not allowed to fire when leaning.  Leave in
	// in case we decide to enable some lean-firing.
	AddLean( ent, muzzlePoint );

	// snap to integer coordinates for more efficient network bandwidth usage
	SnapVector( muzzlePoint );
}

// Rafael - for activate
void CalcMuzzlePointForActivate( gentity_t *ent, vec3_t forward, vec3_t right, vec3_t up, vec3_t muzzlePoint ) {

	VectorCopy( ent->s.pos.trBase, muzzlePoint );
	muzzlePoint[2] += ent->client->ps.viewheight;

	AddLean( ent, muzzlePoint );

	// snap to integer coordinates for more efficient network bandwidth usage
//	SnapVector( muzzlePoint );
	// (SA) /\ only used server-side, so leaving the accuracy in is fine (and means things that show a cursorhint will be hit when activated)
	//			(there were differing views of activatable stuff between cursorhint and activatable)
}
// done.

// Ridah
void CalcMuzzlePoints( gentity_t *ent, int weapon ) {
	vec3_t viewang;

	VectorCopy( ent->client->ps.viewangles, viewang );

	if ( !( ent->r.svFlags & SVF_CASTAI ) ) {   // non ai's take into account scoped weapon 'sway' (just another way aimspread is visualized/utilized)
		float spreadfrac, phase;

		if ( weapon == WP_SNIPERRIFLE || weapon == WP_SNOOPERSCOPE || weapon == WP_FG42SCOPE ) {
			spreadfrac = ent->client->currentAimSpreadScale;

			// rotate 'forward' vector by the sway
			phase = level.time / 1000.0 * ZOOM_PITCH_FREQUENCY * M_PI * 2;
			viewang[PITCH] += ZOOM_PITCH_AMPLITUDE * sin( phase ) * ( spreadfrac + ZOOM_PITCH_MIN_AMPLITUDE );

			phase = level.time / 1000.0 * ZOOM_YAW_FREQUENCY * M_PI * 2;
			viewang[YAW] += ZOOM_YAW_AMPLITUDE * sin( phase ) * ( spreadfrac + ZOOM_YAW_MIN_AMPLITUDE );
		}
	}


	// set aiming directions
	AngleVectors( viewang, forward, right, up );

//----(SA)	modified the muzzle stuff so that weapons that need to fire down a perfect trace
//			straight out of the camera (SP5, Mauser right now) can have that accuracy, but
//			weapons that need an offset effect (bazooka/grenade/etc.) can still look like
//			they came out of the weap.
	CalcMuzzlePointForActivate( ent, forward, right, up, muzzleTrace );
	CalcMuzzlePoint( ent, weapon, forward, right, up, muzzleEffect );
}

/*
==================
Weapon_MagicAmmo
==================
*/
void Weapon_MagicAmmo( gentity_t *ent ) {
        gitem_t *item;
        gentity_t *ent2;
        vec3_t velocity, org, offset;
        vec3_t angles,mins,maxs;
        trace_t tr;

        // TTimo unused
//      int                     mod = MOD_KNIFE;


        if ( level.time - ent->client->ps.classWeaponTime >= g_LTChargeTime.integer * 0.25f ) {
                if ( level.time - ent->client->ps.classWeaponTime > g_LTChargeTime.integer ) {
                        ent->client->ps.classWeaponTime = level.time - g_LTChargeTime.integer;
                }
                ent->client->ps.classWeaponTime += g_LTChargeTime.integer * 0.25;
//                      ent->client->ps.classWeaponTime = level.time;
//                      if (ent->client->ps.classWeaponTime > level.time)
//                              ent->client->ps.classWeaponTime = level.time;
                item = BG_FindItem( "Ammo Pack" );
                VectorCopy( ent->client->ps.viewangles, angles );

                // clamp pitch
                if ( angles[PITCH] < -30 ) {
                        angles[PITCH] = -30;
                } else if ( angles[PITCH] > 30 ) {
                        angles[PITCH] = 30;
                }

                AngleVectors( angles, velocity, NULL, NULL );
                VectorScale( velocity, 64, offset );
                offset[2] += ent->client->ps.viewheight / 2;
                VectorScale( velocity, 75, velocity );
                velocity[2] += 50 + crandom() * 25;

                VectorAdd( ent->client->ps.origin,offset,org );

                VectorSet( mins, -ITEM_RADIUS, -ITEM_RADIUS, 0 );
                VectorSet( maxs, ITEM_RADIUS, ITEM_RADIUS, 2 * ITEM_RADIUS );

                trap_Trace( &tr, ent->client->ps.origin, mins, maxs, org, ent->s.number, MASK_SOLID );
                VectorCopy( tr.endpos, org );

                ent2 = LaunchItem( item, org, velocity, ent->s.number );
                ent2->think = MagicSink;
                ent2->timestamp = level.time + 31200;
                ent2->parent = ent;
        }
}

void Weapon_Medic( gentity_t *ent ) {
        gitem_t *item;
        gentity_t *ent2;
        vec3_t velocity, org, offset;
        vec3_t angles,mins,maxs;
        trace_t tr;

        // TTimo unused
//      int                     mod = MOD_KNIFE;


        if ( level.time - ent->client->ps.classWeaponTime >= g_medicChargeTime.integer * 0.25f ) {
                if ( level.time - ent->client->ps.classWeaponTime > g_medicChargeTime.integer ) {
                        ent->client->ps.classWeaponTime = level.time - g_medicChargeTime.integer;
                }
                ent->client->ps.classWeaponTime += g_medicChargeTime.integer * 0.25;
//                      ent->client->ps.classWeaponTime = level.time;
//                      if (ent->client->ps.classWeaponTime > level.time)
//                              ent->client->ps.classWeaponTime = level.time;
                item = BG_FindItem( "Med Health Classes" );
                VectorCopy( ent->client->ps.viewangles, angles );

                // clamp pitch
                if ( angles[PITCH] < -30 ) {
                        angles[PITCH] = -30;
                } else if ( angles[PITCH] > 30 ) {
                        angles[PITCH] = 30;
                }

                AngleVectors( angles, velocity, NULL, NULL );
                VectorScale( velocity, 64, offset );
                offset[2] += ent->client->ps.viewheight / 2;
                VectorScale( velocity, 75, velocity );
                velocity[2] += 50 + crandom() * 25;

                VectorAdd( ent->client->ps.origin,offset,org );

                VectorSet( mins, -ITEM_RADIUS, -ITEM_RADIUS, 0 );
                VectorSet( maxs, ITEM_RADIUS, ITEM_RADIUS, 2 * ITEM_RADIUS );

                trap_Trace( &tr, ent->client->ps.origin, mins, maxs, org, ent->s.number, MASK_SOLID );
                VectorCopy( tr.endpos, org );

                ent2 = LaunchItem( item, org, velocity, ent->s.number );
                ent2->think = MagicSink;
                ent2->timestamp = level.time + 31200;
                ent2->parent = ent; // JPW NERVE so we can score properly later
        }
}

/*
===============
FireWeapon
===============
*/
void FireWeapon( gentity_t *ent ) {
	float aimSpreadScale;
	vec3_t viewang;  // JPW NERVE

	// Rafael mg42
	//if (ent->active)
	//	return;
	if ( ent->client->ps.persistant[PERS_HWEAPON_USE] && ent->active ) {
		return;
	}

	if ( ent->client->ps.powerups[PW_QUAD] ) {
		s_quadFactor = g_quadfactor.value;
	} else {
		s_quadFactor = 1;
	}

	// track shots taken for accuracy tracking.  Grapple is not a weapon and gauntet is just not tracked
//----(SA)	removing old weapon references
//	if( ent->s.weapon != WP_GRAPPLING_HOOK && ent->s.weapon != WP_GAUNTLET ) {
//		ent->client->ps.persistant[PERS_ACCURACY_SHOTS]++;
//	}

	// Ridah, need to call this for AI prediction also
	CalcMuzzlePoints( ent, ent->s.weapon );

	if ( g_userAim.integer ) {
		aimSpreadScale = ent->client->currentAimSpreadScale;
		// Ridah, add accuracy factor for AI
		if ( ent->aiCharacter ) {
			float aim_accuracy;
			aim_accuracy = AICast_GetAccuracy( ent->s.number );
			if ( aim_accuracy <= 0 ) {
				aim_accuracy = 0.0001;
			}
			aimSpreadScale = ( 1.0 - aim_accuracy ) * 2.0;
		} else {
			//	/maximum/ accuracy for player for a given weapon
			switch ( ent->s.weapon ) {
			case WP_LUGER:
			case WP_SILENCER:
			case WP_COLT:
			case WP_AKIMBO:
				aimSpreadScale += 0.4f;
				break;

			case WP_PANZERFAUST:
				aimSpreadScale += 0.3f;     // it's calculated a different way, so this keeps the accuracy never perfect, but never rediculously wild either
				break;

			default:
				aimSpreadScale += 0.15f;
				break;
			}

			if ( aimSpreadScale > 1 ) {
				aimSpreadScale = 1.0f;  // still cap at 1.0
			}
		}
	} else {
		aimSpreadScale = 1.0;
	}

        if ( g_gametype.integer == GT_COOP_CLASSES ) {
                if ( ( ent->client->ps.eFlags & EF_ZOOMING ) && ( ent->client->ps.stats[STAT_KEYS] & ( 1 << INV_BINOCS ) ) &&
                         ( ent->s.weapon != WP_SNIPERRIFLE ) ) {

                        if ( !( ent->client->ps.leanf ) ) {
                                Weapon_Artillery( ent );
                        }

                        return;
                }
        }

	// fire the specific weapon
	switch ( ent->s.weapon ) {
	case WP_KNIFE:
		Weapon_Knife( ent );
		break;
// JPW NERVE
	case WP_CLASS_SPECIAL:
		Weapon_Class_Special( ent );
		break;
// jpw
		break;
	case WP_LUGER:
		Bullet_Fire( ent, LUGER_SPREAD * aimSpreadScale, LUGER_DAMAGE );
		break;
	case WP_SILENCER:
		Bullet_Fire( ent, SILENCER_SPREAD * aimSpreadScale, LUGER_DAMAGE );
		break;
	case WP_AKIMBO: //----(SA)	added
	case WP_COLT:
		Bullet_Fire( ent, COLT_SPREAD * aimSpreadScale, COLT_DAMAGE );
		break;
	case WP_VENOM:
		weapon_venom_fire( ent, qfalse, aimSpreadScale );
		break;
	case WP_SNIPERRIFLE:
		Bullet_Fire( ent, SNIPER_SPREAD * aimSpreadScale, SNIPER_DAMAGE );
// JPW NERVE -- added muzzle flip in multiplayer
		if ( !ent->aiCharacter ) {
//		if (g_gametype.integer != GT_SINGLE_PLAYER) {
			VectorCopy( ent->client->ps.viewangles,viewang );
//			viewang[PITCH] -= 6; // handled in clientthink instead
			ent->client->sniperRifleMuzzleYaw = crandom() * 0.5; // used in clientthink
			ent->client->sniperRifleMuzzlePitch = 0.8f;
			ent->client->sniperRifleFiredTime = level.time;
			SetClientViewAngle( ent,viewang );
		}
// jpw
		break;
	case WP_SNOOPERSCOPE:
		Bullet_Fire( ent, SNOOPER_SPREAD * aimSpreadScale, SNOOPER_DAMAGE );
// JPW NERVE -- added muzzle flip in multiplayer
		if ( !ent->aiCharacter ) {
//		if (g_gametype.integer != GT_SINGLE_PLAYER) {
			VectorCopy( ent->client->ps.viewangles,viewang );
			ent->client->sniperRifleMuzzleYaw = crandom() * 0.5; // used in clientthink
			ent->client->sniperRifleMuzzlePitch = 0.9f;
			ent->client->sniperRifleFiredTime = level.time;
			SetClientViewAngle( ent,viewang );
		}
// jpw
		break;
	case WP_MAUSER:
		Bullet_Fire( ent, MAUSER_SPREAD * aimSpreadScale, MAUSER_DAMAGE );
		break;
	case WP_GARAND:
		Bullet_Fire( ent, GARAND_SPREAD * aimSpreadScale, GARAND_DAMAGE );
		break;
//----(SA)	added
	case WP_FG42SCOPE:
		if ( !ent->aiCharacter ) {
//		if (g_gametype.integer != GT_SINGLE_PLAYER) {
			VectorCopy( ent->client->ps.viewangles,viewang );
//			ent->client->sniperRifleMuzzleYaw = crandom()*0.04; // used in clientthink
			ent->client->sniperRifleMuzzleYaw = 0;
			ent->client->sniperRifleMuzzlePitch = 0.07f;
			ent->client->sniperRifleFiredTime = level.time;
			SetClientViewAngle( ent,viewang );
		}
	case WP_FG42:
		Bullet_Fire( ent, FG42_SPREAD * aimSpreadScale, FG42_DAMAGE );
		break;
//----(SA)	end
	case WP_STEN:
		Bullet_Fire( ent, STEN_SPREAD * aimSpreadScale, STEN_DAMAGE );
		break;
	case WP_MP40:
		Bullet_Fire( ent, MP40_SPREAD * aimSpreadScale, MP40_DAMAGE );
		break;
	case WP_THOMPSON:
		Bullet_Fire( ent, THOMPSON_SPREAD * aimSpreadScale, THOMPSON_DAMAGE );
		break;
	case WP_PANZERFAUST:
		ent->client->ps.classWeaponTime = level.time; // JPW NERVE
		Weapon_RocketLauncher_Fire( ent, aimSpreadScale );
		break;
	case WP_GRENADE_LAUNCHER:
	case WP_GRENADE_PINEAPPLE:
	case WP_DYNAMITE:
		// weapon_grenadelauncher_fire( ent, ent->s.weapon );
		//RF- disabled this since it's broken (do we still want it?)
		//if (ent->aiName && !Q_strncmp(ent->aiName, "mechanic", 8) && !AICast_HasFiredWeapon(ent->s.number, ent->s.weapon))
		//	weapon_crowbar_throw (ent);
		//else
		if ( ent->s.weapon == WP_DYNAMITE ) {
			ent->client->ps.classWeaponTime = level.time; // JPW NERVE
		}
		if ( g_gametype.integer <= GT_COOP ) {
			weapon_grenadelauncher_fire_coop( ent, ent->s.weapon );
		} else {
			weapon_grenadelauncher_fire( ent, ent->s.weapon );
		}
		break;
	case WP_FLAMETHROWER:
		Weapon_FlamethrowerFire( ent );
		break;
	case WP_TESLA:
		Tesla_Fire( ent );

		// push the player back a bit
		if ( !ent->aiCharacter ) {
			vec3_t forward, vangle;
			VectorCopy( ent->client->ps.viewangles, vangle );
			vangle[PITCH] = 0;  // nullify pitch so you can't lightning jump
			AngleVectors( vangle, forward, NULL, NULL );
			// make it less if in the air
			if ( ent->s.groundEntityNum == ENTITYNUM_NONE ) {
				VectorMA( ent->client->ps.velocity, -32, forward, ent->client->ps.velocity );
			} else {
				VectorMA( ent->client->ps.velocity, -100, forward, ent->client->ps.velocity );
			}
		}
		break;
	case WP_GAUNTLET:
		Weapon_Gauntlet( ent );
		break;

	case WP_MONSTER_ATTACK1:
		switch ( ent->aiCharacter ) {
		case AICHAR_WARZOMBIE:
			break;
		case AICHAR_ZOMBIE:
			// temp just to show it works
			// G_Printf("ptoo\n");
			weapon_zombiespit( ent );
			break;
		default:
			//G_Printf( "FireWeapon: unknown ai weapon: %s attack1\n", ent->classname );
			// ??? bug ???
			break;
		}

	case WP_MORTAR:
		break;
        case WP_MEDKIT:
                Weapon_Medic( ent );
                break;
        case WP_AMMO:
                Weapon_MagicAmmo( ent );
                break;
	case WP_MEDIC_SYRINGE:
                Weapon_Syringe( ent );
		break;
        case WP_SMOKE_GRENADE:
                if ( level.time - ent->client->ps.classWeaponTime >= g_LTChargeTime.integer * 0.5f ) {
                        if ( level.time - ent->client->ps.classWeaponTime > g_LTChargeTime.integer ) {
                                ent->client->ps.classWeaponTime = level.time - g_LTChargeTime.integer;
                        }
                        ent->client->ps.classWeaponTime = level.time; //+= g_LTChargeTime.integer*0.5f; FIXME later
                        weapon_grenadelauncher_fire( ent,WP_SMOKE_GRENADE );
                }
                break;
        case WP_ARTY:
                G_Printf( "calling artilery\n" );
                break;

	default:
		break;
	}

	AICast_RecordWeaponFire( ent );
}
