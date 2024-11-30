#include "extdll.h"
#include "util.h"
#include "CBasePlayer.h"
#include "trains.h"
#include "nodes.h"
#include "shake.h"
#include "decals.h"
#include "gamerules.h"

#define SF_STRIP_SUIT_TOO 1

class CStripWeapons : public CPointEntity
{
public:
	void	Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);

private:
};

LINK_ENTITY_TO_CLASS(player_weaponstrip, CStripWeapons)

void CStripWeapons::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	CBasePlayer* pPlayer = NULL;

	if (pActivator && pActivator->IsPlayer())
	{
		pPlayer = (CBasePlayer*)pActivator;
	}
	else if (!g_pGameRules->IsDeathmatch())
	{
		pPlayer = (CBasePlayer*)CBaseEntity::Instance(g_engfuncs.pfnPEntityOfEntIndex(1));
	}

	if (pPlayer)
		pPlayer->RemoveAllItems(pev->spawnflags & SF_STRIP_SUIT_TOO);
}
