/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   This source code contains proprietary and confidential information of
*   Valve LLC and its suppliers.  Access to this code is restricted to
*   persons who have executed a written SDK license with Valve.  Any access,
*   use or distribution of this code by or to any unlicensed person is illegal.
*
****/
//=========================================================
// headcrab.cpp - tiny, jumpy alien parasite
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"schedule.h"
#include	"game.h"
#include	"CBasePlayer.h"

//=========================================================
// Monster's Anim Events Go Here
//=========================================================
#define		SR_AE_JUMPATTACK	( 2 )

Task_t	tlSRRangeAttack1[] =
{
	{ TASK_STOP_MOVING,			(float)0		},
	{ TASK_FACE_IDEAL,			(float)0		},
	{ TASK_RANGE_ATTACK1,		(float)0		},
	{ TASK_SET_ACTIVITY,		(float)ACT_IDLE	},
	{ TASK_FACE_IDEAL,			(float)0		},
	{ TASK_WAIT_RANDOM,			(float)0.5		},
};

Schedule_t	slRCRangeAttack1[] =
{
	{ 
		tlSRRangeAttack1,
		ARRAYSIZE ( tlSRRangeAttack1 ), 
		bits_COND_ENEMY_OCCLUDED	|
		bits_COND_NO_AMMO_LOADED,
		0,
		"HCRangeAttack1"
	},
};

Task_t	tlSRRangeAttack1Fast[] =
{
	{ TASK_STOP_MOVING,			(float)0		},
	{ TASK_FACE_IDEAL,			(float)0		},
	{ TASK_RANGE_ATTACK1,		(float)0		},
	{ TASK_SET_ACTIVITY,		(float)ACT_IDLE	},
};

Schedule_t	slRCRangeAttack1Fast[] =
{
	{ 
		tlSRRangeAttack1Fast,
		ARRAYSIZE ( tlSRRangeAttack1Fast ), 
		bits_COND_ENEMY_OCCLUDED	|
		bits_COND_NO_AMMO_LOADED,
		0,
		"HCRAFast"
	},
};

class COFShockRoach : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void RunTask ( Task_t *pTask );
	void StartTask ( Task_t *pTask );
	void SetYawSpeed ( void );
	void EXPORT LeapTouch ( CBaseEntity *pOther );
	Vector Center( void );
	Vector BodyTarget( const Vector &posSrc );
	void PainSound( void );
	void DeathSound( void );
	void IdleSound( void );
	void AlertSound( void );
	void StartFollowingSound();
	void StopFollowingSound();
	void CantFollowSound();
	void PrescheduleThink( void );
	int  Classify ( void );
	const char* DisplayName();
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	BOOL CheckRangeAttack1 ( float flDot, float flDist );
	BOOL CheckRangeAttack2 ( float flDot, float flDist );
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	void RifleUse(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);

	virtual float GetDamageAmount( void ) { return gSkillData.sk_shockroach_dmg_bite; }
	virtual int GetVoicePitch( void ) { return 100; }
	virtual float GetSoundVolue( void ) { return 1.0; }
	Schedule_t* GetScheduleOfType ( int Type );
	const char* GetDeathNoticeWeapon() { return "weapon_crowbar"; }

	void MonsterThink() override;

	int Save( CSave &save ) override;
	int Restore( CRestore &restore ) override;
	static TYPEDESCRIPTION m_SaveData[];

	float m_flBirthTime;
	BOOL m_fRoachSolid;

	CUSTOM_SCHEDULES;

	static const char *pIdleSounds[];
	static const char *pAlertSounds[];
	static const char *pPainSounds[];
	static const char *pAttackSounds[];
	static const char *pDeathSounds[];
	static const char *pBiteSounds[];
};
LINK_ENTITY_TO_CLASS( monster_shockroach, COFShockRoach );

TYPEDESCRIPTION	COFShockRoach::m_SaveData[] =
{
	DEFINE_FIELD( COFShockRoach, m_flBirthTime, FIELD_TIME ),
};

IMPLEMENT_SAVERESTORE( COFShockRoach, CBaseMonster );

DEFINE_CUSTOM_SCHEDULES( COFShockRoach )
{
	slRCRangeAttack1,
	slRCRangeAttack1Fast,
};

IMPLEMENT_CUSTOM_SCHEDULES( COFShockRoach, CBaseMonster );

const char *COFShockRoach::pIdleSounds[] = 
{
	"shockroach/shock_idle1.wav",
	"shockroach/shock_idle2.wav",
	"shockroach/shock_idle3.wav",
};
const char *COFShockRoach::pAlertSounds[] = 
{
	"shockroach/shock_angry.wav",
};
const char *COFShockRoach::pPainSounds[] = 
{
	"shockroach/shock_flinch.wav",
};
const char *COFShockRoach::pAttackSounds[] = 
{
	"shockroach/shock_jump1.wav",
	"shockroach/shock_jump2.wav",
};

const char *COFShockRoach::pDeathSounds[] = 
{
	"shockroach/shock_die.wav",
};

const char *COFShockRoach::pBiteSounds[] = 
{
	"shockroach/shock_bite.wav",
};

//=========================================================
// Classify - indicates this monster's place in the 
// relationship table.
//=========================================================
int	COFShockRoach :: Classify ( void )
{
	return	CBaseMonster::Classify(CLASS_ALIEN_PREY);
}

const char* COFShockRoach::DisplayName() {
	return m_displayName ? CBaseMonster::DisplayName() : "Shock Roach";
}

//=========================================================
// Center - returns the real center of the headcrab.  The 
// bounding box is much larger than the actual creature so 
// this is needed for targeting
//=========================================================
Vector COFShockRoach :: Center ( void )
{
	return Vector( pev->origin.x, pev->origin.y, pev->origin.z + 6 );
}


Vector COFShockRoach :: BodyTarget( const Vector &posSrc ) 
{ 
	return Center( );
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void COFShockRoach :: SetYawSpeed ( void )
{
	int ys;

	switch ( m_Activity )
	{
	case ACT_IDLE:
	case ACT_LEAP:
	case ACT_FALL:
	case ACT_TURN_LEFT:
	case ACT_TURN_RIGHT:
		ys = 90;
		break;
	default:
		ys = 30;
		break;
	}

	pev->yaw_speed = ys * gSkillData.sk_yawspeed_mult;
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void COFShockRoach :: HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch( pEvent->event )
	{
		case SR_AE_JUMPATTACK:
		{
			ClearBits( pev->flags, FL_ONGROUND );

			UTIL_SetOrigin (pev, pev->origin + Vector ( 0 , 0 , 1) );// take him off ground so engine doesn't instantly reset onground 
			UTIL_MakeVectors ( pev->angles );

			Vector vecJumpDir;
			if (m_hEnemy != NULL)
			{
				float gravity = g_psv_gravity->value;
				if (gravity <= 1)
					gravity = 1;

				Vector targetOri = m_hEnemy->GetTargetOrigin();

				// How fast does the headcrab need to travel to reach that height given gravity?
				float height = (targetOri.z + m_hEnemy->pev->view_ofs.z - pev->origin.z);
				if (height < 16)
					height = 16;
				float speed = sqrt(2 * gravity * height);
				float time = speed / gravity;

				// Scale the sideways velocity to get there at the right time
				vecJumpDir = (targetOri + m_hEnemy->pev->view_ofs - pev->origin);
				vecJumpDir = vecJumpDir * (1.0 / time);

				// Speed to offset gravity at the desired height
				vecJumpDir.z = speed;

				// Don't jump too far/fast
				float distance = vecJumpDir.Length();
				
				if (distance > 650)
				{
					vecJumpDir = vecJumpDir * ( 650.0 / distance );
				}
			}
			else
			{
				// jump hop, don't care where
				vecJumpDir = Vector( gpGlobals->v_forward.x, gpGlobals->v_forward.y, gpGlobals->v_up.z ) * 350;
			}

			//Not used for Shock Roach
			//int iSound = RANDOM_LONG(0,2);
			//if ( iSound != 0 )
			//	EMIT_SOUND_DYN( edict(), CHAN_VOICE, pAttackSounds[iSound], GetSoundVolue(), ATTN_IDLE, 0, GetVoicePitch() );

			pev->velocity = vecJumpDir;
			m_flNextAttack = gpGlobals->time + 2;
		}
		break;

		default:
			CBaseMonster::HandleAnimEvent( pEvent );
			break;
	}
}

//=========================================================
// Spawn
//=========================================================
void COFShockRoach :: Spawn()
{
	Precache( );

	InitModel();
	SetSize(Vector(-12, -12, 0), Vector(12, 12, 4));

	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_FLY;
	m_bloodColor		= BLOOD_COLOR_GREEN;
	pev->effects		= 0;
	pev->view_ofs		= Vector ( 0, 0, 20 );// position of the eyes relative to monster's origin.
	pev->yaw_speed		= 5;//!!! should we put this in the monster's changeanim function since turn rates may vary with state/anim?
	m_flFieldOfView		= 0.5;// indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_MonsterState		= MONSTERSTATE_NONE;

	m_fRoachSolid = false;
	m_flBirthTime = gpGlobals->time;

	MonsterInit();

	SetUse(&COFShockRoach::RifleUse);
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void COFShockRoach :: Precache()
{
	CBaseMonster::Precache();

	PRECACHE_SOUND_ARRAY(pIdleSounds);
	PRECACHE_SOUND_ARRAY(pAlertSounds);
	PRECACHE_SOUND_ARRAY(pPainSounds);
	PRECACHE_SOUND_ARRAY(pAttackSounds);
	PRECACHE_SOUND_ARRAY(pDeathSounds);
	PRECACHE_SOUND_ARRAY(pBiteSounds);

	PRECACHE_SOUND("shockroach/shock_walk.wav" );

	m_defaultModel = "models/w_shock_rifle.mdl";
	UTIL_PrecacheOther("weapon_shockrifle");
	PRECACHE_MODEL(GetModel());
}	


//=========================================================
// RunTask 
//=========================================================
void COFShockRoach :: RunTask ( Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_RANGE_ATTACK1:
	case TASK_RANGE_ATTACK2:
		{
			if ( m_fSequenceFinished )
			{
				TaskComplete();
				SetTouch( NULL );
				m_IdealActivity = ACT_IDLE;
			}
			break;
		}
	case TASK_MOVE_TO_TARGET_RANGE:
	{
		CBaseMonster::RunTask(pTask);

		// always run when following someone because the walk speed is painfully slow
		m_movementActivity = ACT_RUN;
		break;
	}
	default:
		{
			CBaseMonster :: RunTask(pTask);
		}
	}
}

//=========================================================
// LeapTouch - this is the headcrab's touch function when it
// is in the air
//=========================================================
void COFShockRoach :: LeapTouch ( CBaseEntity *pOther )
{
	if ( !pOther->pev->takedamage )
	{
		return;
	}

	if ( pOther->Classify() == Classify() )
	{
		return;
	}

	//Give the player a shock rifle if they don't have one
	if( pOther->IsPlayer() )
	{
		auto pPlayer = static_cast<CBasePlayer*>( pOther );

		if( !pPlayer->HasNamedPlayerItem( "weapon_shockrifle" ) )
		{
			pPlayer->GiveNamedItem( "weapon_shockrifle" );
			SetTouch( NULL );
			UTIL_Remove( this );
			return;
		}
	}

	// Don't hit if back on ground
	if ( !FBitSet( pev->flags, FL_ONGROUND ) )
	{
		EMIT_SOUND_DYN( edict(), CHAN_WEAPON, RANDOM_SOUND_ARRAY(pBiteSounds), GetSoundVolue(), ATTN_IDLE, 0, GetVoicePitch() );
		
		pOther->TakeDamage( pev, pev, GetDamageAmount(), DMG_SLASH );
	}

	SetTouch( NULL );
}

//=========================================================
// PrescheduleThink
//=========================================================
void COFShockRoach :: PrescheduleThink ( void )
{
	// make the crab coo a little bit in combat state
	if ( m_MonsterState == MONSTERSTATE_COMBAT && RANDOM_FLOAT( 0, 5 ) < 0.1 )
	{
		IdleSound();
	}
}

void COFShockRoach :: StartTask ( Task_t *pTask )
{
	m_iTaskStatus = TASKSTATUS_RUNNING;

	switch ( pTask->iTask )
	{
	case TASK_RANGE_ATTACK1:
		{
			EMIT_SOUND_DYN( edict(), CHAN_WEAPON, pAttackSounds[0], GetSoundVolue(), ATTN_IDLE, 0, GetVoicePitch() );
			m_IdealActivity = ACT_RANGE_ATTACK1;
			SetTouch ( &COFShockRoach::LeapTouch );
			break;
		}
	default:
		{
			CBaseMonster :: StartTask( pTask );
		}
	}
}


//=========================================================
// CheckRangeAttack1
//=========================================================
BOOL COFShockRoach :: CheckRangeAttack1 ( float flDot, float flDist )
{
	if ( FBitSet( pev->flags, FL_ONGROUND ) && flDist <= 256 && flDot >= 0.65 )
	{
		return TRUE;
	}
	return FALSE;
}

//=========================================================
// CheckRangeAttack2
//=========================================================
BOOL COFShockRoach :: CheckRangeAttack2 ( float flDot, float flDist )
{
	return FALSE;
	// BUGBUG: Why is this code here?  There is no ACT_RANGE_ATTACK2 animation.  I've disabled it for now.
#if 0
	if ( FBitSet( pev->flags, FL_ONGROUND ) && flDist > 64 && flDist <= 256 && flDot >= 0.5 )
	{
		return TRUE;
	}
	return FALSE;
#endif
}

int COFShockRoach :: TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	// Don't take any acid damage -- BigMomma's mortar is acid
	if ( bitsDamageType & DMG_ACID )
		flDamage = 0;

	//Don't take damage while spawning
	if( gpGlobals->time - m_flBirthTime < 2 )
		flDamage = 0;

	//Never gib the roach
	return CBaseMonster::TakeDamage( pevInflictor, pevAttacker, flDamage, ( bitsDamageType & ~DMG_ALWAYSGIB ) | DMG_NEVERGIB );
}

void COFShockRoach::RifleUse(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) {
	if (pActivator->IsPlayer())
	{
		auto pPlayer = static_cast<CBasePlayer*>(pActivator);

		if (!pPlayer->HasNamedPlayerItem("weapon_shockrifle"))
		{
			pPlayer->GiveNamedItem("weapon_shockrifle");
			SetTouch(NULL);
			UTIL_Remove(this);
			return;
		}
	}

	FollowerUse(pActivator, pCaller, useType, value);
}

//=========================================================
// IdleSound
//=========================================================
void COFShockRoach :: IdleSound ( void )
{
}

//=========================================================
// AlertSound 
//=========================================================
void COFShockRoach :: AlertSound ( void )
{
}

//=========================================================
// AlertSound 
//=========================================================
void COFShockRoach :: PainSound ( void )
{
}

//=========================================================
// DeathSound 
//=========================================================
void COFShockRoach :: DeathSound ( void )
{
}

void COFShockRoach::StartFollowingSound() {
	EMIT_SOUND(ENT(pev), CHAN_VOICE, RANDOM_SOUND_ARRAY(pIdleSounds), 1, ATTN_NORM);
}

void COFShockRoach::StopFollowingSound() {
	EMIT_SOUND(ENT(pev), CHAN_VOICE, RANDOM_SOUND_ARRAY(pPainSounds), 1, ATTN_NORM);
}

void COFShockRoach::CantFollowSound() {
	EMIT_SOUND(ENT(pev), CHAN_VOICE, RANDOM_SOUND_ARRAY(pPainSounds), 1, ATTN_NORM);
}

Schedule_t* COFShockRoach :: GetScheduleOfType ( int Type )
{
	switch	( Type )
	{
		case SCHED_RANGE_ATTACK1:
		{
			return &slRCRangeAttack1[ 0 ];
		}
		break;
	}

	return CBaseMonster::GetScheduleOfType( Type );
}

void COFShockRoach::MonsterThink()
{
	const auto lifeTime = gpGlobals->time - m_flBirthTime;

	if( lifeTime >= 0.2 )
	{
		pev->movetype = MOVETYPE_STEP;
	}

	if( !m_fRoachSolid && lifeTime >= 2.0 )
	{
		SetSize(Vector( -12, -12, 0 ), Vector( 12, 12, 4 ) );
		m_fRoachSolid = true;
	}

	if( lifeTime >= gSkillData.sk_shockroach_lifespan )
		TakeDamage( pev, pev, pev->health, DMG_NEVERGIB );

	CBaseMonster::MonsterThink();
}
