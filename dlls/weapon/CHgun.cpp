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
#if !defined( OEM_BUILD ) && !defined( HLDEMO_BUILD )

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "skill.h"
#include "weapons.h"
#include "nodes.h"
#include "CBasePlayer.h"
#include "monster/CHornet.h"
#include "gamerules.h"
#include "weapon/CHgun.h"

static float GetRechargeTime()
{
	if (gpGlobals->maxClients > 1)
	{
		return 0.3f;
	}
	return 0.5f;
}

enum hgun_e {
	HGUN_IDLE1 = 0,
	HGUN_FIDGETSWAY,
	HGUN_FIDGETSHAKE,
	HGUN_DOWN,
	HGUN_UP,
	HGUN_SHOOT
};

enum firemode_e
{
	FIREMODE_TRACK = 0,
	FIREMODE_FAST
};


LINK_ENTITY_TO_CLASS( weapon_hornetgun, CHgun )

BOOL CHgun::IsUseable( void )
{
	return TRUE;
}

void CHgun::Spawn( )
{
	pev->classname = MAKE_STRING("weapon_hornetgun"); // hack to allow for alternate names
	
	Precache( );
	m_iId = WEAPON_HORNETGUN;
	SetWeaponModelW();

	m_iDefaultAmmo = HIVEHAND_DEFAULT_GIVE;
	m_iFirePhase = 0;

	FallInit();// get ready to fall down.
}


void CHgun::Precache( void )
{
	m_defaultModelV = "models/v_hgun.mdl";
	m_defaultModelP = "models/p_hgun.mdl";
	m_defaultModelW = "models/w_hgun.mdl";
	CBasePlayerWeapon::Precache();

	PrecacheEvents();

	UTIL_PrecacheOther("hornet");
}

void CHgun::PrecacheEvents() {
	m_usHornetFire = PRECACHE_EVENT(1, "events/firehornet.sc");
}

int CHgun::AddToPlayer( CBasePlayer *pPlayer )
{
	if ( CBasePlayerWeapon::AddToPlayer( pPlayer ) )
	{

#ifndef CLIENT_DLL
		if ( g_pGameRules->IsMultiplayer() )
		{
			// in multiplayer, all hivehands come full. 
			pPlayer->m_rgAmmo[ PrimaryAmmoIndex() ] = gSkillData.sk_ammo_max_hornets;
		}
#endif

		return TRUE;
	}
	return FALSE;
}

int CHgun::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "Hornets";
	p->iMaxAmmo1 = gSkillData.sk_ammo_max_hornets;
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 3;
	p->iPosition = 3;
	p->iId = m_iId = WEAPON_HORNETGUN;
	p->iFlags = ITEM_FLAG_NOAUTOSWITCHEMPTY | ITEM_FLAG_NOAUTORELOAD;
	p->iWeight = HORNETGUN_WEIGHT;

	return 1;
}


BOOL CHgun::Deploy( )
{
	Reload();

	return DefaultDeploy(GetModelV(), GetModelP(), HGUN_UP, "hive" );
}

void CHgun::Holster( int skiplocal /* = 0 */ )
{
	CBasePlayer* m_pPlayer = GetPlayer();
	if (!m_pPlayer)
		return;

	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;
	SendWeaponAnim( HGUN_DOWN );

	//!!!HACKHACK - can't select hornetgun if it's empty! no way to get ammo for it, either.
	if ( !m_pPlayer->m_rgAmmo[ PrimaryAmmoIndex() ] )
	{
		m_pPlayer->m_rgAmmo[ PrimaryAmmoIndex() ] = 1;
	}
}


void CHgun::PrimaryAttack()
{
	CBasePlayer* m_pPlayer = GetPlayer();
	if (!m_pPlayer)
		return;

	Reload( );

	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
	{
		return;
	}

#ifndef CLIENT_DLL
	UTIL_MakeVectors( m_pPlayer->pev->v_angle );

	CBaseEntity *pHornet = CBaseEntity::Create( "hornet", m_pPlayer->GetGunPosition( ) + gpGlobals->v_forward * 16 + gpGlobals->v_right * 8 + gpGlobals->v_up * -12, m_pPlayer->pev->v_angle, m_pPlayer->edict() );
	pHornet->pev->velocity = gpGlobals->v_forward * 300;

	m_flRechargeTime = gpGlobals->time + GetRechargeTime();
#endif
	
	m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;
	

	m_pPlayer->m_iWeaponVolume = QUIET_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = DIM_GUN_FLASH;

	int flags;
#if defined( CLIENT_WEAPONS )
	flags = FEV_NOTHOST;
#else
	flags = 0;
#endif

	PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_usHornetFire, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0.0, 0.0, FIREMODE_TRACK, 0, 0, 0 );

	

	// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	m_flNextPrimaryAttack = GetNextAttackDelay(0.25);

	if (m_flNextPrimaryAttack < UTIL_WeaponTimeBase() )
	{
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.25;
	}
	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] == 0)
	{
		m_flNextPrimaryAttack += GetRechargeTime();
	}

	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
}



void CHgun::SecondaryAttack( void )
{
	CBasePlayer* m_pPlayer = GetPlayer();
	if (!m_pPlayer)
		return;

	Reload();

	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
	{
		return;
	}

	//Wouldn't be a bad idea to completely predict these, since they fly so fast...
#ifndef CLIENT_DLL
	CBaseEntity *pHornet;
	Vector vecSrc;

	UTIL_MakeVectors( m_pPlayer->pev->v_angle );

	vecSrc = m_pPlayer->GetGunPosition( ) + gpGlobals->v_forward * 16 + gpGlobals->v_right * 8 + gpGlobals->v_up * -12;

	m_iFirePhase++;
	switch ( m_iFirePhase )
	{
	case 1:
		vecSrc = vecSrc + gpGlobals->v_up * 8;
		break;
	case 2:
		vecSrc = vecSrc + gpGlobals->v_up * 8;
		vecSrc = vecSrc + gpGlobals->v_right * 8;
		break;
	case 3:
		vecSrc = vecSrc + gpGlobals->v_right * 8;
		break;
	case 4:
		vecSrc = vecSrc + gpGlobals->v_up * -8;
		vecSrc = vecSrc + gpGlobals->v_right * 8;
		break;
	case 5:
		vecSrc = vecSrc + gpGlobals->v_up * -8;
		break;
	case 6:
		vecSrc = vecSrc + gpGlobals->v_up * -8;
		vecSrc = vecSrc + gpGlobals->v_right * -8;
		break;
	case 7:
		vecSrc = vecSrc + gpGlobals->v_right * -8;
		break;
	case 8:
		vecSrc = vecSrc + gpGlobals->v_up * 8;
		vecSrc = vecSrc + gpGlobals->v_right * -8;
		m_iFirePhase = 0;
		break;
	}

	pHornet = CBaseEntity::Create( "hornet", vecSrc, m_pPlayer->pev->v_angle, m_pPlayer->edict() );
	pHornet->pev->velocity = gpGlobals->v_forward * 1200;
	pHornet->pev->angles = UTIL_VecToAngles( pHornet->pev->velocity );

	pHornet->SetThink( &CHornet::StartDart );

	m_flRechargeTime = gpGlobals->time + GetRechargeTime();
#endif

	int flags;
#if defined( CLIENT_WEAPONS )
	flags = FEV_NOTHOST;
#else
	flags = 0;
#endif

	PLAYBACK_EVENT_FULL( flags, m_pPlayer->edict(), m_usHornetFire, 0.0, (float *)&g_vecZero, (float *)&g_vecZero, 0.0, 0.0, FIREMODE_FAST, 0, 0, 0 );


	m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]--;
	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = DIM_GUN_FLASH;

		// player "shoot" animation
	m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.1;
	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] == 0)
	{
		m_flRechargeTime = gpGlobals->time + 0.5;
		m_flNextSecondaryAttack += 0.5;
		m_flNextPrimaryAttack += 0.5;
	}
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
}


void CHgun::Reload( void )
{
	CBasePlayer* m_pPlayer = GetPlayer();
	if (!m_pPlayer)
		return;

	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] >= gSkillData.sk_ammo_max_hornets)
		return;

	while (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] < gSkillData.sk_ammo_max_hornets && m_flRechargeTime < gpGlobals->time)
	{
		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]++;
		m_flRechargeTime += GetRechargeTime();
	}
}


void CHgun::WeaponIdle( void )
{
	CBasePlayer* m_pPlayer = GetPlayer();
	if (!m_pPlayer)
		return;

	Reload( );

	if (m_flTimeWeaponIdle > UTIL_WeaponTimeBase())
		return;

	int iAnim;
	float flRand = UTIL_SharedRandomFloat( m_pPlayer->random_seed, 0, 1 );
	if (flRand <= 0.75)
	{
		iAnim = HGUN_IDLE1;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 30.0 / 16 * (2);
	}
	else if (flRand <= 0.875)
	{
		iAnim = HGUN_FIDGETSWAY;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 40.0 / 16.0;
	}
	else
	{
		iAnim = HGUN_FIDGETSHAKE;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 35.0 / 16.0;
	}
	SendWeaponAnim( iAnim );
}

#endif
