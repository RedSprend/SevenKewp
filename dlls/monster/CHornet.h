#pragma once
/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
//=========================================================
// Hornets
//=========================================================

//=========================================================
// Hornet Defines
//=========================================================
#define HORNET_TYPE_RED			0
#define HORNET_TYPE_ORANGE		1
#define HORNET_RED_SPEED		(float)600
#define HORNET_ORANGE_SPEED		(float)800
#define	HORNET_BUZZ_VOLUME		(float)0.8

extern int iHornetPuff;

//=========================================================
// Hornet - this is the projectile that the Alien Grunt fires.
//=========================================================
class CHornet : public CBaseMonster
{
public:
	virtual int	ObjectCaps(void) { return CBaseMonster::ObjectCaps() & ~FCAP_IMPULSE_USE; }
	void Spawn( void );
	void Precache( void );
	int	 Classify ( void );
	const char* DisplayName();
	int  IRelationship ( CBaseEntity *pTarget );
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );
	virtual BOOL IsNormalMonster() { return FALSE; }
	static	TYPEDESCRIPTION m_SaveData[];

	void IgniteTrail( void );
	void EXPORT StartTrack ( void );
	void EXPORT StartDart ( void );
	void EXPORT TrackTarget ( void );
	void EXPORT DartThink( void );
	void EXPORT TrackTouch ( CBaseEntity *pOther );
	void EXPORT DartTouch( CBaseEntity *pOther );
	void EXPORT DieTouch ( CBaseEntity *pOther );
	
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );

	float			m_flStopAttack;
	float			m_flStartTrack;
	float			m_flNextTrack;
	int				m_iHornetType;
	float			m_flFlySpeed;
	Vector			m_lastPos;

private:
	static const char* pBuzzSounds[];
	static const char* pHitSounds[];
};

