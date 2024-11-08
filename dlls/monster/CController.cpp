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
#if !defined( OEM_BUILD ) && !defined( HLDEMO_BUILD )

//=========================================================
// CONTROLLER
//=========================================================

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"effects.h"
#include	"schedule.h"
#include	"weapons.h"
#include	"CTalkSquadMonster.h"

//=========================================================
// Monster's Anim Events Go Here
//=========================================================
#define	CONTROLLER_AE_HEAD_OPEN		1
#define	CONTROLLER_AE_BALL_SHOOT	2
#define	CONTROLLER_AE_SMALL_SHOOT	3
#define CONTROLLER_AE_POWERUP_FULL	4
#define CONTROLLER_AE_POWERUP_HALF	5

#define CONTROLLER_FLINCH_DELAY			2		// at most one flinch every n secs

class CController : public CTalkSquadMonster
{
public:
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	void Spawn( void );
	void Precache( void );
	void SetYawSpeed( void );
	int  Classify ( void );
	const char* DisplayName();
	void HandleAnimEvent( MonsterEvent_t *pEvent );

	void RunAI( void );
	BOOL CheckRangeAttack1 ( float flDot, float flDist );	// balls
	BOOL CheckRangeAttack2 ( float flDot, float flDist );	// head
	BOOL CheckMeleeAttack1 ( float flDot, float flDist );	// block, throw
	Schedule_t* GetSchedule ( void );
	Schedule_t* GetScheduleOfType ( int Type );
	void StartTask ( Task_t *pTask );
	void RunTask ( Task_t *pTask );
	CUSTOM_SCHEDULES;

	void Stop( void );
	void Move ( float flInterval );
	int  CheckLocalMove ( const Vector &vecStart, const Vector &vecEnd, CBaseEntity *pTarget, float *pflDist );
	void MoveExecute( CBaseEntity *pTargetEnt, const Vector &vecDir, float flInterval );
	void SetActivity ( Activity NewActivity );
	BOOL ShouldAdvanceRoute( float flWaypointDist );
	int LookupFloat( );

	float m_flNextFlinch;

	float m_flShootTime;
	float m_flShootEnd;

	void PainSound( void );
	void AlertSound( void );
	void IdleSound( void );
	void AttackSound( void );
	void DeathSound( void );
	void StartFollowingSound();
	void StopFollowingSound();
	void CantFollowSound();

	static const char *pAttackSounds[];
	static const char *pIdleSounds[];
	static const char *pAlertSounds[];
	static const char *pPainSounds[];
	static const char *pDeathSounds[];

	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
	void Killed( entvars_t *pevAttacker, int iGib );
	void GibMonster( void );
	const char* GetDeathNoticeWeapon() { return "weapon_crowbar"; }

	EHANDLE m_hBall[2];	// hand balls
	int m_iBall[2];			// how bright it should be
	float m_iBallTime[2];	// when it should be that color
	int m_iBallCurrent[2];	// current brightness

	Vector m_vecEstVelocity;

	Vector m_velocity;
	int m_fInCombat;
};

LINK_ENTITY_TO_CLASS( monster_alien_controller, CController )
LINK_ENTITY_TO_CLASS( monster_stukabat, CController )

TYPEDESCRIPTION	CController::m_SaveData[] = 
{
	DEFINE_ARRAY( CController, m_hBall, FIELD_EHANDLE, 2 ),
	DEFINE_ARRAY( CController, m_iBall, FIELD_INTEGER, 2 ),
	DEFINE_ARRAY( CController, m_iBallTime, FIELD_TIME, 2 ),
	DEFINE_ARRAY( CController, m_iBallCurrent, FIELD_INTEGER, 2 ),
	DEFINE_FIELD( CController, m_vecEstVelocity, FIELD_VECTOR ),
};
IMPLEMENT_SAVERESTORE( CController, CTalkSquadMonster )


const char *CController::pAttackSounds[] = 
{
	"controller/con_attack1.wav",
	"controller/con_attack2.wav",
	"controller/con_attack3.wav",
};

const char *CController::pIdleSounds[] = 
{
	"controller/con_idle1.wav",
	"controller/con_idle2.wav",
	"controller/con_idle3.wav",
	"controller/con_idle4.wav",
	"controller/con_idle5.wav",
};

const char *CController::pAlertSounds[] = 
{
	"controller/con_alert1.wav",
	"controller/con_alert2.wav",
	"controller/con_alert3.wav",
};

const char *CController::pPainSounds[] = 
{
	"controller/con_pain1.wav",
	"controller/con_pain2.wav",
	"controller/con_pain3.wav",
};

const char *CController::pDeathSounds[] = 
{
	"controller/con_die1.wav",
	"controller/con_die2.wav",
};


//=========================================================
// Classify - indicates this monster's place in the 
// relationship table.
//=========================================================
int	CController :: Classify ( void )
{
	return	CBaseMonster::Classify(CLASS_ALIEN_MILITARY);
}

const char* CController::DisplayName() {
	return m_displayName ? CBaseMonster::DisplayName() : "Alien Controller";
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CController :: SetYawSpeed ( void )
{
	int ys;

	ys = 120;

#if 0
	switch ( m_Activity )
	{
	}
#endif

	pev->yaw_speed = ys * gSkillData.sk_yawspeed_mult;
}

int CController :: TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	if (IsImmune(pevAttacker))
		return 0;

	// HACK HACK -- until we fix this.
	if ( IsAlive() )
		PainSound();
	return CBaseMonster::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );
}


void CController::Killed( entvars_t *pevAttacker, int iGib )
{
	// shut off balls
	/*
	m_iBall[0] = 0;
	m_iBallTime[0] = gpGlobals->time + 4.0;
	m_iBall[1] = 0;
	m_iBallTime[1] = gpGlobals->time + 4.0;
	*/

	// fade balls
	if (m_hBall[0])
	{
		m_hBall[0]->SUB_StartFadeOut();
		m_hBall[0] = NULL;
	}
	if (m_hBall[1])
	{
		m_hBall[1]->SUB_StartFadeOut();
		m_hBall[1] = NULL;
	}

	CTalkSquadMonster::Killed( pevAttacker, iGib );
}


void CController::GibMonster( void )
{
	// delete balls
	if (m_hBall[0])
	{
		UTIL_Remove(m_hBall[0] );
		m_hBall[0] = NULL;
	}
	if (m_hBall[1])
	{
		UTIL_Remove(m_hBall[1] );
		m_hBall[1] = NULL;
	}
	CTalkSquadMonster::GibMonster( );
}




void CController :: PainSound( void )
{
	if (RANDOM_LONG(0,5) < 2)
		EMIT_SOUND_ARRAY_DYN( CHAN_VOICE, pPainSounds ); 
}	

void CController :: AlertSound( void )
{
	EMIT_SOUND_ARRAY_DYN( CHAN_VOICE, pAlertSounds ); 
}

void CController :: IdleSound( void )
{
	EMIT_SOUND_ARRAY_DYN( CHAN_VOICE, pIdleSounds ); 
}

void CController :: AttackSound( void )
{
	EMIT_SOUND_ARRAY_DYN( CHAN_VOICE, pAttackSounds ); 
	CSoundEnt::InsertSound(bits_SOUND_COMBAT, pev->origin, NORMAL_GUN_VOLUME, 0.3);
}

void CController :: DeathSound( void )
{
	EMIT_SOUND_ARRAY_DYN( CHAN_VOICE, pDeathSounds ); 
}

void CController::StartFollowingSound() {
	EMIT_SOUND_ARRAY_DYN(CHAN_VOICE, pAttackSounds);
}

void CController::StopFollowingSound() {
	EMIT_SOUND_ARRAY_DYN(CHAN_VOICE, pPainSounds);
}

void CController::CantFollowSound() {
	EMIT_SOUND_ARRAY_DYN(CHAN_VOICE, pPainSounds);
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CController :: HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch( pEvent->event )
	{
		case CONTROLLER_AE_HEAD_OPEN:
		{
			Vector vecStart, angleGun;
			
			GetAttachment( 0, vecStart, angleGun );
			
			UTIL_ELight(entindex(), 1, vecStart, 1, RGBA(255, 192, 64), 20, -32);

			m_iBall[0] = 192;
			m_iBallTime[0] = gpGlobals->time + atoi( pEvent->options ) / 15.0;
			m_iBall[1] = 255;
			m_iBallTime[1] = gpGlobals->time + atoi( pEvent->options ) / 15.0;

		}
		break;

		case CONTROLLER_AE_BALL_SHOOT:
		{
			Vector vecStart, angleGun;
			
			GetAttachment( 0, vecStart, angleGun );

			UTIL_ELight(entindex(), 1, g_vecZero, 32, RGBA(255, 192, 64), 10, 32);

			const char* soundlist = m_soundReplacementPath ? STRING(m_soundReplacementPath) : "";
			std::unordered_map<std::string, std::string> keys = { {"soundlist", soundlist} };
			CBaseMonster *pBall = (CBaseMonster*)Create( "controller_head_ball", vecStart, pev->angles, edict(), keys);

			pBall->pev->velocity = Vector( 0, 0, 32 );
			pBall->m_hEnemy = m_hEnemy;

			if (CBaseMonster::IRelationship(Classify(), CLASS_PLAYER) == R_AL) {
				pBall->pev->rendercolor = Vector(0, 255, 255);
			}

			m_iBall[0] = 0;
			m_iBall[1] = 0;
		}
		break;

		case CONTROLLER_AE_SMALL_SHOOT:
		{
			AttackSound( );
			m_flShootTime = gpGlobals->time;
			m_flShootEnd = m_flShootTime + atoi( pEvent->options ) / 15.0;
		}
		break;
		case CONTROLLER_AE_POWERUP_FULL:
		{
			m_iBall[0] = 255;
			m_iBallTime[0] = gpGlobals->time + atoi( pEvent->options ) / 15.0;
			m_iBall[1] = 255;
			m_iBallTime[1] = gpGlobals->time + atoi( pEvent->options ) / 15.0;
		}
		break;
		case CONTROLLER_AE_POWERUP_HALF:
		{
			m_iBall[0] = 192;
			m_iBallTime[0] = gpGlobals->time + atoi( pEvent->options ) / 15.0;
			m_iBall[1] = 192;
			m_iBallTime[1] = gpGlobals->time + atoi( pEvent->options ) / 15.0;
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
void CController :: Spawn()
{
	Precache( );

	InitModel();
	SetSize(Vector( -32, -32, 0 ), Vector( 32, 32, 64 ));

	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_FLY;
	pev->flags			|= FL_FLY;
	m_bloodColor		= BLOOD_COLOR_GREEN;
	pev->view_ofs		= Vector( 0, 0, -2 );// position of the eyes relative to monster's origin.
	m_flFieldOfView		= VIEW_FIELD_FULL;// indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_MonsterState		= MONSTERSTATE_NONE;

	MonsterInit();
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CController::Precache()
{
	CBaseMonster::Precache();

	m_defaultModel = "models/controller.mdl";
	PRECACHE_MODEL(GetModel());

	PRECACHE_SOUND_ARRAY(pAttackSounds);
	PRECACHE_SOUND_ARRAY(pIdleSounds);
	PRECACHE_SOUND_ARRAY(pAlertSounds);
	PRECACHE_SOUND_ARRAY(pPainSounds);
	PRECACHE_SOUND_ARRAY(pDeathSounds);

	PRECACHE_MODEL("sprites/xspark4.spr");

	UTIL_PrecacheOther("controller_energy_ball");

	// precached here instead of in head_ball so that per-monster sound replacement works
	PRECACHE_SOUND("debris/zap4.wav");
	PRECACHE_SOUND("weapons/electro4.wav");

	const char* soundlist = m_soundReplacementPath ? STRING(m_soundReplacementPath) : "";
	std::unordered_map<std::string, std::string> keys = { {"soundlist", soundlist} };
	UTIL_PrecacheOther( "controller_head_ball", keys);
}	

//=========================================================
// AI Schedules Specific to this monster
//=========================================================


// Chase enemy schedule
Task_t tlControllerChaseEnemy[] = 
{
	{ TASK_GET_PATH_TO_ENEMY,	(float)128		},
	{ TASK_WAIT_FOR_MOVEMENT,	(float)0		},

};

Schedule_t slControllerChaseEnemy[] =
{
	{ 
		tlControllerChaseEnemy,
		ARRAYSIZE ( tlControllerChaseEnemy ),
		bits_COND_NEW_ENEMY			|
		bits_COND_TASK_FAILED,
		0,
		"CONTROLLER_CHASE_ENEMY"
	},
};



Task_t	tlControllerStrafe[] =
{
	{ TASK_WAIT,					(float)0.2					},
	{ TASK_GET_PATH_TO_ENEMY,		(float)128					},
	{ TASK_WAIT_FOR_MOVEMENT,		(float)0					},
	{ TASK_WAIT,					(float)1					},
};

Schedule_t	slControllerStrafe[] =
{
	{ 
		tlControllerStrafe,
		ARRAYSIZE ( tlControllerStrafe ), 
		bits_COND_NEW_ENEMY,
		0,
		"CONTROLLER_STRAFE"
	},
};


Task_t	tlControllerTakeCover[] =
{
	{ TASK_WAIT,					(float)0.2					},
	{ TASK_FIND_COVER_FROM_ENEMY,	(float)0					},
	{ TASK_WAIT_FOR_MOVEMENT,		(float)0					},
	{ TASK_WAIT,					(float)1					},
};

Schedule_t	slControllerTakeCover[] =
{
	{ 
		tlControllerTakeCover,
		ARRAYSIZE ( tlControllerTakeCover ), 
		bits_COND_NEW_ENEMY,
		0,
		"CONTROLLER_TAKE_COVER"
	},
};


Task_t	tlControllerFail[] =
{
	{ TASK_STOP_MOVING,			0				},
	{ TASK_SET_ACTIVITY,		(float)ACT_IDLE },
	{ TASK_WAIT,				(float)2		},
	{ TASK_WAIT_PVS,			(float)0		},
};

Schedule_t	slControllerFail[] =
{
	{
		tlControllerFail,
		ARRAYSIZE ( tlControllerFail ),
		0,
		0,
		"CONTROLLER_FAIL"
	},
};



DEFINE_CUSTOM_SCHEDULES( CController )
{
	slControllerChaseEnemy,
	slControllerStrafe,
	slControllerTakeCover,
	slControllerFail,
};

IMPLEMENT_CUSTOM_SCHEDULES( CController, CTalkSquadMonster )



//=========================================================
// StartTask
//=========================================================
void CController :: StartTask ( Task_t *pTask )
{
	switch ( pTask->iTask )
	{
	case TASK_RANGE_ATTACK1:
		CTalkSquadMonster :: StartTask ( pTask );
		break;
	case TASK_GET_PATH_TO_ENEMY_LKP:
		{
			if (BuildNearestRoute( m_vecEnemyLKP, pev->view_ofs, pTask->flData, (m_vecEnemyLKP - pev->origin).Length() + 1024 ))
			{
				TaskComplete();
			}
			else
			{
				// no way to get there =(
				ALERT ( at_aiconsole, "GetPathToEnemyLKP failed!!\n" );
				TaskFail();
			}
			break;
		}
	case TASK_GET_PATH_TO_ENEMY:
		{
			CBaseEntity *pEnemy = m_hEnemy;

			if ( pEnemy == NULL )
			{
				TaskFail();
				return;
			}

			if (BuildNearestRoute( pEnemy->pev->origin, pEnemy->pev->view_ofs, pTask->flData, (pEnemy->pev->origin - pev->origin).Length() + 1024 ))
			{
				TaskComplete();
			}
			else
			{
				// no way to get there =(
				ALERT ( at_aiconsole, "GetPathToEnemy failed!!\n" );
				TaskFail();
			}
			break;
		}
	default:
		CTalkSquadMonster :: StartTask ( pTask );
		break;
	}
}


Vector Intersect( Vector vecSrc, Vector vecDst, Vector vecMove, float flSpeed )
{
	Vector vecTo = vecDst - vecSrc;

	float a = DotProduct( vecMove, vecMove ) - flSpeed * flSpeed;
	float b = 0 * DotProduct(vecTo, vecMove); // why does this work?
	float c = DotProduct( vecTo, vecTo );

	float t;
	if (a == 0)
	{
		t = c / (flSpeed * flSpeed);
	}
	else
	{
		t = b * b - 4 * a * c;
		t = sqrt( t ) / (2.0 * a);
		float t1 = -b +t;
		float t2 = -b -t;

		if (t1 < 0 || t2 < t1)
			t = t2;
		else
			t = t1;
	}

	// ALERT( at_console, "Intersect %f\n", t );

	if (t < 0.1)
		t = 0.1;
	if (t > 10.0)
		t = 10.0;

	Vector vecHit = vecTo + vecMove * t;
	return vecHit.Normalize( ) * flSpeed;
}


int CController::LookupFloat( )
{
	if (m_velocity.Length( ) < 32.0)
	{
		return LookupSequence( "up" );
	}

	UTIL_MakeAimVectors( pev->angles );
	float x = DotProduct( gpGlobals->v_forward, m_velocity );
	float y = DotProduct( gpGlobals->v_right, m_velocity );
	float z = DotProduct( gpGlobals->v_up, m_velocity );

	if (fabs(x) > fabs(y) && fabs(x) > fabs(z))
	{
		if (x > 0)
			return LookupSequence( "forward");
		else
			return LookupSequence( "backward");
	}
	else if (fabs(y) > fabs(z))
	{
		if (y > 0)
			return LookupSequence( "right");
		else
			return LookupSequence( "left");
	}
	else
	{
		if (z > 0)
			return LookupSequence( "up");
		else
			return LookupSequence( "down");
	}
}


//=========================================================
// RunTask 
//=========================================================
void CController :: RunTask ( Task_t *pTask )
{

	if (m_flShootEnd > gpGlobals->time)
	{
		Vector vecHand, vecAngle;
		
		GetAttachment( 2, vecHand, vecAngle );
	
		while (m_flShootTime < m_flShootEnd && m_flShootTime < gpGlobals->time)
		{
			Vector vecSrc = vecHand + pev->velocity * (m_flShootTime - gpGlobals->time);
			Vector vecDir;
			
			if (m_hEnemy != NULL)
			{
				if (HasConditions( bits_COND_SEE_ENEMY ))
				{
					m_vecEstVelocity = m_vecEstVelocity * 0.5 + m_hEnemy->pev->velocity * 0.5;
				}
				else
				{
					m_vecEstVelocity = m_vecEstVelocity * 0.8;
				}
				vecDir = Intersect( vecSrc, m_hEnemy->BodyTarget( pev->origin ), m_vecEstVelocity, gSkillData.sk_controller_speedball );
				float delta = 0.03490; // +-2 degree
				vecDir = vecDir + Vector( RANDOM_FLOAT( -delta, delta ), RANDOM_FLOAT( -delta, delta ), RANDOM_FLOAT( -delta, delta ) ) * gSkillData.sk_controller_speedball;

				vecSrc = vecSrc + vecDir * (gpGlobals->time - m_flShootTime);
				CBaseMonster *pBall = (CBaseMonster*)Create( "controller_energy_ball", vecSrc, pev->angles, edict() );
				pBall->pev->velocity = vecDir;

				if (CBaseMonster::IRelationship(Classify(), CLASS_PLAYER) == R_AL) {
					pBall->pev->rendercolor = Vector(0, 255, 255);
				}
			}
			m_flShootTime += 0.2;
		}

		if (m_flShootTime > m_flShootEnd)
		{
			m_iBall[0] = 64;
			m_iBallTime[0] = m_flShootEnd;
			m_iBall[1] = 64;
			m_iBallTime[1] = m_flShootEnd;
			m_fInCombat = FALSE;
		}
	}

	switch ( pTask->iTask )
	{
	case TASK_WAIT_FOR_MOVEMENT:
	case TASK_WAIT:
	case TASK_WAIT_FACE_ENEMY:
	case TASK_MOVE_TO_TARGET_RANGE:
	case TASK_WAIT_PVS:
		if (m_hTargetEnt && IsFollowing() && (m_MonsterState == MONSTERSTATE_IDLE || m_MonsterState == MONSTERSTATE_ALERT)) {
			MakeIdealYaw(m_hTargetEnt->pev->origin);
		}
		else if (m_MonsterState != MONSTERSTATE_IDLE) {
			MakeIdealYaw(m_vecEnemyLKP);
		}
		
		ChangeYaw( pev->yaw_speed );

		if (m_fSequenceFinished)
		{
			m_fInCombat = FALSE;
		}

		CTalkSquadMonster :: RunTask ( pTask );

		if (!m_fInCombat)
		{
			if (HasConditions ( bits_COND_CAN_RANGE_ATTACK1 ))
			{
				pev->sequence = LookupActivity( ACT_RANGE_ATTACK1 );
				pev->frame = 0;
				ResetSequenceInfo( );
				m_fInCombat = TRUE;
			}
			else if (HasConditions ( bits_COND_CAN_RANGE_ATTACK2 ))
			{
				pev->sequence = LookupActivity( ACT_RANGE_ATTACK2 );
				pev->frame = 0;
				ResetSequenceInfo( );
				m_fInCombat = TRUE;
			}
			else
			{
				int iFloat = LookupFloat( );
				if (m_fSequenceFinished || iFloat != pev->sequence)
				{
					pev->sequence = iFloat;
					pev->frame = 0;
					ResetSequenceInfo( );
				}
			}
		}
		break;
	default: 
		CTalkSquadMonster :: RunTask ( pTask );
		break;
	}
}


//=========================================================
// GetSchedule - Decides which type of schedule best suits
// the monster's current state and conditions. Then calls
// monster's member function to get a pointer to a schedule
// of the proper type.
//=========================================================
Schedule_t *CController :: GetSchedule ( void )
{
	switch	( m_MonsterState )
	{
	case MONSTERSTATE_IDLE:
		break;

	case MONSTERSTATE_ALERT:
		break;

	case MONSTERSTATE_COMBAT:
		{
			// dead enemy
			if ( HasConditions ( bits_COND_LIGHT_DAMAGE ) )
			{
				// m_iFrustration++;
			}
			if ( HasConditions ( bits_COND_HEAVY_DAMAGE ) )
			{
				// m_iFrustration++;
			}
		}
		break;
	default:
		break;
	}

	return CTalkSquadMonster :: GetSchedule();
}



//=========================================================
//=========================================================
Schedule_t* CController :: GetScheduleOfType ( int Type ) 
{
	// ALERT( at_console, "%d\n", m_iFrustration );
	switch	( Type )
	{
	case SCHED_CHASE_ENEMY:
		return slControllerChaseEnemy;
	case SCHED_RANGE_ATTACK1:
		return slControllerStrafe;
	case SCHED_RANGE_ATTACK2:
	case SCHED_MELEE_ATTACK1:
	case SCHED_MELEE_ATTACK2:
	case SCHED_TAKE_COVER_FROM_ENEMY:
		return slControllerTakeCover;
	case SCHED_FAIL:
		return slControllerFail;
	}

	return CBaseMonster :: GetScheduleOfType( Type );
}





//=========================================================
// CheckRangeAttack1  - shoot a bigass energy ball out of their head
//
//=========================================================
BOOL CController :: CheckRangeAttack1 ( float flDot, float flDist )
{
	if ( flDot > 0.5 && flDist > 256 && flDist <= 2048 )
	{
		return TRUE;
	}
	return FALSE;
}


BOOL CController :: CheckRangeAttack2 ( float flDot, float flDist )
{
	if ( flDot > 0.5 && flDist > 64 && flDist <= 2048 )
	{
		return TRUE;
	}
	return FALSE;
}


BOOL CController :: CheckMeleeAttack1 ( float flDot, float flDist )
{
	return FALSE;
}


void CController :: SetActivity ( Activity NewActivity )
{
	CBaseMonster::SetActivity( NewActivity );

	switch ( m_Activity)
	{
	case ACT_WALK:
		m_flGroundSpeed = 100;
		break;
	default:
		m_flGroundSpeed = 100;
		break;
	}
}



//=========================================================
// RunAI
//=========================================================
void CController :: RunAI( void )
{
	CBaseMonster :: RunAI();
	Vector vecStart, angleGun;

	if ( HasMemory( bits_MEMORY_KILLED ) )
		return;

	for (int i = 0; i < 2; i++)
	{
		CSprite* ball = (CSprite*)m_hBall[i].GetEntity();
		if (!ball)
		{
			ball = CSprite::SpriteCreate( "sprites/xspark4.spr", pev->origin, TRUE );
			ball->SetTransparency( kRenderGlow, 255, 255, 255, 255, kRenderFxNoDissipation );
			ball->SetAttachment( edict(), (i + 3) );
			ball->SetScale( 1.0 );

			if (CBaseMonster::IRelationship(Classify(), CLASS_PLAYER) == R_AL) {
				ball->SetColor(0, 255, 255);
			}

			m_hBall[i] = ball;
		}

		float t = m_iBallTime[i] - gpGlobals->time;
		if (t > 0.1)
			t = 0.1 / t;
		else
			t = 1.0;

		m_iBallCurrent[i] += (m_iBall[i] - m_iBallCurrent[i]) * t;

		ball->SetBrightness( m_iBallCurrent[i] );

		GetAttachment( i + 2, vecStart, angleGun );
		UTIL_SetOrigin(ball->pev, vecStart );
		
		UTIL_ELight(entindex(), i+3, vecStart, m_iBallCurrent[i] / 8, RGBA(255, 192, 64), 5, 0);
	}
}


extern void DrawRoute( entvars_t *pev, WayPoint_t *m_Route, int m_iRouteIndex, int r, int g, int b );

void CController::Stop( void ) 
{ 
	m_IdealActivity = GetStoppedActivity(); 
}


#define DIST_TO_CHECK	200
void CController :: Move ( float flInterval ) 
{
	float		flWaypointDist;
	float		flCheckDist;
	float		flDist;// how far the lookahead check got before hitting an object.
	float		flMoveDist;
	Vector		vecDir;
	Vector		vecApex;
	CBaseEntity	*pTargetEnt;

	// Don't move if no valid route
	if ( FRouteClear() )
	{
		ALERT( at_aiconsole, "Tried to move with no route!\n" );
		TaskFail();
		return;
	}
	
	if ( m_flMoveWaitFinished > gpGlobals->time )
		return;

// Debug, test movement code
#if 0
//	if ( CVAR_GET_FLOAT("stopmove" ) != 0 )
	{
		if ( m_movementGoal == MOVEGOAL_ENEMY )
			RouteSimplify( m_hEnemy );
		else
			RouteSimplify( m_hTargetEnt );
		FRefreshRoute();
		return;
	}
#else
// Debug, draw the route
//	DrawRoute( pev, m_Route, m_iRouteIndex, 0, 0, 255 );
#endif

	// if the monster is moving directly towards an entity (enemy for instance), we'll set this pointer
	// to that entity for the CheckLocalMove and Triangulate functions.
	pTargetEnt = NULL;

	if (m_flGroundSpeed == 0)
	{
		m_flGroundSpeed = 100;
		// TaskFail( );
		// return;
	}

	flMoveDist = m_flGroundSpeed * flInterval;

	do 
	{
		// local move to waypoint.
		vecDir = ( m_Route[ m_iRouteIndex ].vecLocation - pev->origin ).Normalize();
		flWaypointDist = ( m_Route[ m_iRouteIndex ].vecLocation - pev->origin ).Length();
		
		// MakeIdealYaw ( m_Route[ m_iRouteIndex ].vecLocation );
		// ChangeYaw ( pev->yaw_speed );

		// if the waypoint is closer than CheckDist, CheckDist is the dist to waypoint
		if ( flWaypointDist < DIST_TO_CHECK )
		{
			flCheckDist = flWaypointDist;
		}
		else
		{
			flCheckDist = DIST_TO_CHECK;
		}
		
		if ( (m_Route[ m_iRouteIndex ].iType & (~bits_MF_NOT_TO_MASK)) == bits_MF_TO_ENEMY )
		{
			// only on a PURE move to enemy ( i.e., ONLY MF_TO_ENEMY set, not MF_TO_ENEMY and DETOUR )
			pTargetEnt = m_hEnemy;
		}
		else if ( (m_Route[ m_iRouteIndex ].iType & ~bits_MF_NOT_TO_MASK) == bits_MF_TO_TARGETENT )
		{
			pTargetEnt = m_hTargetEnt;
		}

		// !!!BUGBUG - CheckDist should be derived from ground speed.
		// If this fails, it should be because of some dynamic entity blocking this guy.
		// We've already checked this path, so we should wait and time out if the entity doesn't move
		flDist = 0;
		if ( CheckLocalMove ( pev->origin, pev->origin + vecDir * flCheckDist, pTargetEnt, &flDist ) != LOCALMOVE_VALID )
		{
			CBaseEntity *pBlocker;

			// Can't move, stop
			Stop();
			// Blocking entity is in global trace_ent
			pBlocker = CBaseEntity::Instance( gpGlobals->trace_ent );
			if (pBlocker)
			{
				DispatchBlocked( edict(), pBlocker->edict() );
			}
			if ( pBlocker && m_moveWaitTime > 0 && pBlocker->IsMoving() && !pBlocker->IsPlayer() && (gpGlobals->time-m_flMoveWaitFinished) > 3.0 )
			{
				// Can we still move toward our target?
				if ( flDist < m_flGroundSpeed )
				{
					// Wait for a second
					m_flMoveWaitFinished = gpGlobals->time + m_moveWaitTime;
	//				ALERT( at_aiconsole, "Move %s!!!\n", STRING( pBlocker->pev->classname ) );
					return;
				}
			}
			else 
			{
				// try to triangulate around whatever is in the way.
				if ( FTriangulate( pev->origin, m_Route[ m_iRouteIndex ].vecLocation, flDist, pTargetEnt, &vecApex ) )
				{
					InsertWaypoint( vecApex, bits_MF_TO_DETOUR );
					RouteSimplify( pTargetEnt );
				}
				else
				{
	 			    ALERT ( at_aiconsole, "Couldn't Triangulate\n" );
					Stop();
					if ( m_moveWaitTime > 0 )
					{
						FRefreshRoute();
						m_flMoveWaitFinished = gpGlobals->time + m_moveWaitTime * 0.5;
					}
					else
					{
						TaskFail();
						ALERT( at_aiconsole, "Failed to move!\n" );
						//ALERT( at_aiconsole, "%f, %f, %f\n", pev->origin.z, (pev->origin + (vecDir * flCheckDist)).z, m_Route[m_iRouteIndex].vecLocation.z );
					}
					return;
				}
			}
		}

		// UNDONE: this is a hack to quit moving farther than it has looked ahead.
		if (flCheckDist < flMoveDist)
		{
			MoveExecute( pTargetEnt, vecDir, flCheckDist / m_flGroundSpeed );

			// ALERT( at_console, "%.02f\n", flInterval );
			AdvanceRoute( flWaypointDist );
			flMoveDist -= flCheckDist;
		}
		else
		{
			MoveExecute( pTargetEnt, vecDir, flMoveDist / m_flGroundSpeed );

			if ( ShouldAdvanceRoute( flWaypointDist - flMoveDist ) )
			{
				AdvanceRoute( flWaypointDist );
			}
			flMoveDist = 0;
		}

		if ( MovementIsComplete() )
		{
			Stop();
			RouteClear();
		}
	} while (flMoveDist > 0 && flCheckDist > 0);

	// cut corner?
	if (flWaypointDist < 128)
	{
		if ( m_movementGoal == MOVEGOAL_ENEMY )
			RouteSimplify( m_hEnemy );
		else
			RouteSimplify( m_hTargetEnt );
		FRefreshRoute();

		if (m_flGroundSpeed > 100)
			m_flGroundSpeed -= 40;
	}
	else
	{
		if (m_flGroundSpeed < 400)
			m_flGroundSpeed += 10;
	}
}



BOOL CController:: ShouldAdvanceRoute( float flWaypointDist )
{
	if ( flWaypointDist <= 32  )
	{
		return TRUE;
	}

	return FALSE;
}


int CController :: CheckLocalMove ( const Vector &vecStart, const Vector &vecEnd, CBaseEntity *pTarget, float *pflDist )
{
	TraceResult tr;

	UTIL_TraceHull( vecStart + Vector( 0, 0, 32), vecEnd + Vector( 0, 0, 32), dont_ignore_monsters, large_hull, edict(), &tr );

	// ALERT( at_console, "%.0f %.0f %.0f : ", vecStart.x, vecStart.y, vecStart.z );
	// ALERT( at_console, "%.0f %.0f %.0f\n", vecEnd.x, vecEnd.y, vecEnd.z );

	if (pflDist)
	{
		*pflDist = ( (tr.vecEndPos - Vector( 0, 0, 32 )) - vecStart ).Length();// get the distance.
	}

	// ALERT( at_console, "check %d %d %f\n", tr.fStartSolid, tr.fAllSolid, tr.flFraction );
	if (tr.fStartSolid || tr.flFraction < 1.0)
	{
		if ( pTarget && pTarget->edict() == gpGlobals->trace_ent )
			return LOCALMOVE_VALID;
		return LOCALMOVE_INVALID;
	}

	return LOCALMOVE_VALID;
}


void CController::MoveExecute( CBaseEntity *pTargetEnt, const Vector &vecDir, float flInterval )
{
	if ( m_IdealActivity != m_movementActivity )
		m_IdealActivity = m_movementActivity;

	// ALERT( at_console, "move %.4f %.4f %.4f : %f\n", vecDir.x, vecDir.y, vecDir.z, flInterval );

	// float flTotal = m_flGroundSpeed * pev->framerate * flInterval;
	// UTIL_MoveToOrigin ( ENT(pev), m_Route[ m_iRouteIndex ].vecLocation, flTotal, MOVE_STRAFE );

	m_velocity = m_velocity * 0.8 + m_flGroundSpeed * vecDir * 0.2;

	UTIL_MoveToOrigin ( ENT(pev), pev->origin + m_velocity, m_velocity.Length() * flInterval, MOVE_STRAFE );
	
}




//=========================================================
// Controller bouncy ball attack
//=========================================================
class CControllerHeadBall : public CBaseMonster
{
	void Spawn( void );
	void Precache( void );
	void EXPORT HuntThink( void );
	void EXPORT DieThink( void );
	void EXPORT BounceTouch( CBaseEntity *pOther );
	void MovetoTarget( Vector vecTarget );
	void Crawl( void );
	const char* GetDeathNoticeWeapon() { return "weapon_crowbar"; }
	virtual BOOL IsNormalMonster() { return FALSE; }
	int m_iTrail;
	int m_flNextAttack;
	Vector m_vecIdeal;
	EHANDLE m_hOwner;
};
LINK_ENTITY_TO_CLASS( controller_head_ball, CControllerHeadBall )



void CControllerHeadBall :: Spawn( void )
{
	m_hOwner = Instance(pev->owner);

	Precache( );
	// motor
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;

	SET_MODEL(ENT(pev), "sprites/xspark4.spr");
	pev->rendermode = kRenderTransAdd;
	pev->rendercolor.x = 255;
	pev->rendercolor.y = 255;
	pev->rendercolor.z = 255;
	pev->renderamt = 255;
	pev->scale = 2.0;

	UTIL_SetSize(pev, Vector( 0, 0, 0), Vector(0, 0, 0));
	UTIL_SetOrigin( pev, pev->origin );

	SetThink( &CControllerHeadBall::HuntThink );
	SetTouch( &CControllerHeadBall::BounceTouch );

	m_vecIdeal = Vector( 0, 0, 0 );

	pev->nextthink = gpGlobals->time + 0.1;

	pev->dmgtime = gpGlobals->time;
}


void CControllerHeadBall :: Precache( void )
{
	CBaseMonster::Precache();
	PRECACHE_MODEL("sprites/xspark1.spr");
	PRECACHE_SOUND("debris/zap4.wav");
	PRECACHE_SOUND("weapons/electro4.wav");
}


void CControllerHeadBall :: HuntThink( void  )
{
	pev->nextthink = gpGlobals->time + 0.1;

	pev->renderamt -= 5;

	UTIL_ELight(entindex(), 0, pev->origin, pev->renderamt / 16, RGBA(255, 255, 255), 2, 0);

	// check world boundaries
	if (gpGlobals->time - pev->dmgtime > 5 || pev->renderamt < 64 || m_hEnemy == NULL || m_hOwner == NULL || pev->origin.x < -4096 || pev->origin.x > 4096 || pev->origin.y < -4096 || pev->origin.y > 4096 || pev->origin.z < -4096 || pev->origin.z > 4096)
	{
		SetTouch( NULL );
		UTIL_Remove( this );
		return;
	}

	MovetoTarget( m_hEnemy->Center( ) );

	if ((m_hEnemy->Center() - pev->origin).Length() < 64)
	{
		TraceResult tr;

		UTIL_TraceLine( pev->origin, m_hEnemy->Center(), dont_ignore_monsters, ENT(pev), &tr );

		CBaseEntity *pEntity = CBaseEntity::Instance(tr.pHit);
		if (pEntity != NULL && pEntity->pev->takedamage)
		{
			ClearMultiDamage( );
			pEntity->TraceAttack( m_hOwner->pev, gSkillData.sk_controller_dmgzap, pev->velocity, &tr, DMG_SHOCK );
			ApplyMultiDamage( pev, m_hOwner->pev );
		}

		UTIL_BeamEntPoint(entindex(), 0, tr.vecEndPos, g_sModelIndexLaser, 0, 10, 3, 20, 0, RGBA(255, 255, 255, 255), 10);

		int oldFlags = pev->flags;
		pev->flags |= FL_MONSTER; // HACK: consider this entity for sound replacement
		UTIL_EmitAmbientSound( ENT(pev), tr.vecEndPos, "weapons/electro4.wav", 0.5, ATTN_NORM, 0, RANDOM_LONG( 140, 160 ) );
		pev->flags = oldFlags;

		m_flNextAttack = gpGlobals->time + 3.0;

		SetThink( &CControllerHeadBall::DieThink );
		pev->nextthink = gpGlobals->time + 0.3;
	}

	// Crawl( );
}


void CControllerHeadBall :: DieThink( void  )
{
	UTIL_Remove( this );
}


void CControllerHeadBall :: MovetoTarget( Vector vecTarget )
{
	// accelerate
	float flSpeed = m_vecIdeal.Length();
	if (flSpeed == 0)
	{
		m_vecIdeal = pev->velocity;
		flSpeed = m_vecIdeal.Length();
	}

	if (flSpeed > 400)
	{
		m_vecIdeal = m_vecIdeal.Normalize( ) * 400;
	}
	m_vecIdeal = m_vecIdeal + (vecTarget - pev->origin).Normalize() * 100;
	pev->velocity = m_vecIdeal;
}



void CControllerHeadBall :: Crawl( void  )
{

	Vector vecAim = Vector( RANDOM_FLOAT( -1, 1 ), RANDOM_FLOAT( -1, 1 ), RANDOM_FLOAT( -1, 1 ) ).Normalize( );
	Vector vecPnt = pev->origin + pev->velocity * 0.3 + vecAim * 64;

	UTIL_BeamEntPoint(entindex(), 0, vecPnt, g_sModelIndexLaser, 0, 10, 3, 20, 0, RGBA(255, 255, 255, 255), 10);
}


void CControllerHeadBall::BounceTouch( CBaseEntity *pOther )
{
	Vector vecDir = m_vecIdeal.Normalize( );

	TraceResult tr = UTIL_GetGlobalTrace( );

	float n = -DotProduct(tr.vecPlaneNormal, vecDir);

	vecDir = 2.0 * tr.vecPlaneNormal * n + vecDir;

	m_vecIdeal = vecDir * m_vecIdeal.Length();
}




class CControllerZapBall : public CBaseMonster
{
	void Spawn( void );
	void Precache( void );
	void EXPORT AnimateThink( void );
	void EXPORT ExplodeTouch( CBaseEntity *pOther );
	virtual BOOL IsNormalMonster() { return FALSE; }

	EHANDLE m_hOwner;
};
LINK_ENTITY_TO_CLASS( controller_energy_ball, CControllerZapBall )


void CControllerZapBall :: Spawn( void )
{
	Precache( );
	// motor
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;

	SET_MODEL(ENT(pev), "sprites/xspark4.spr");
	pev->rendermode = kRenderTransAdd;
	pev->rendercolor.x = 255;
	pev->rendercolor.y = 255;
	pev->rendercolor.z = 255;
	pev->renderamt = 255;
	pev->scale = 0.5;

	UTIL_SetSize(pev, Vector( 0, 0, 0), Vector(0, 0, 0));
	UTIL_SetOrigin( pev, pev->origin );

	SetThink( &CControllerZapBall::AnimateThink );
	SetTouch( &CControllerZapBall::ExplodeTouch );

	m_hOwner = Instance( pev->owner );
	pev->dmgtime = gpGlobals->time; // keep track of when ball spawned
	pev->nextthink = gpGlobals->time + 0.1;
}


void CControllerZapBall :: Precache( void )
{
	PRECACHE_MODEL("sprites/xspark4.spr");
	// PRECACHE_SOUND("debris/zap4.wav");
	// PRECACHE_SOUND("weapons/electro4.wav");
}


void CControllerZapBall :: AnimateThink( void  )
{
	pev->nextthink = gpGlobals->time + 0.1;
	
	pev->frame = ((int)pev->frame + 1) % 11;

	if (gpGlobals->time - pev->dmgtime > 5 || pev->velocity.Length() < 10)
	{
		SetTouch( NULL );
		UTIL_Remove( this );
	}
}


void CControllerZapBall::ExplodeTouch( CBaseEntity *pOther )
{
	if (pOther->pev->takedamage)
	{
		TraceResult tr = UTIL_GetGlobalTrace( );

		entvars_t	*pevOwner;
		if (m_hOwner != NULL)
		{
			pevOwner = m_hOwner->pev;
		}
		else
		{
			pevOwner = pev;
		}

		ClearMultiDamage( );
		pOther->TraceAttack(pevOwner, gSkillData.sk_controller_dmgball, pev->velocity.Normalize(), &tr, DMG_ENERGYBEAM ); 
		ApplyMultiDamage( pevOwner, pevOwner );

		UTIL_EmitAmbientSound( ENT(pev), tr.vecEndPos, "weapons/electro4.wav", 0.3, ATTN_NORM, 0, RANDOM_LONG( 90, 99 ) );

	}

	UTIL_Remove( this );
}



#endif		// !OEM && !HLDEMO
