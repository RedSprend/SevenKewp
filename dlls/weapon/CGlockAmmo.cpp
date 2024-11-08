#include "CBasePlayerAmmo.h"
#include "CGlock.h"

class CGlockAmmo : public CBasePlayerAmmo
{
	void Spawn( void )
	{ 
		Precache( );
		SET_MODEL_MERGED(ENT(pev), "models/w_9mmclip.mdl", MERGE_MDL_W_9MMCLIP);
		CBasePlayerAmmo::Spawn( );
	}
	void Precache( void )
	{
		PRECACHE_REPLACEMENT_MODEL ("models/w_9mmclip.mdl");
		PRECACHE_SOUND("items/9mmclip1.wav");
	}
	BOOL AddAmmo( CBaseEntity *pOther ) 
	{ 
		if (pOther->GiveAmmo( AMMO_GLOCKCLIP_GIVE, "9mm", gSkillData.sk_ammo_max_9mm ) != -1)
		{
			EMIT_SOUND(ENT(pev), CHAN_ITEM, "items/9mmclip1.wav", 1, ATTN_NORM);
			return TRUE;
		}
		return FALSE;
	}
};

LINK_ENTITY_TO_CLASS( ammo_glockclip, CGlockAmmo )
LINK_ENTITY_TO_CLASS( ammo_9mmclip, CGlockAmmo )
LINK_ENTITY_TO_CLASS( ammo_9mm, CGlockAmmo )
