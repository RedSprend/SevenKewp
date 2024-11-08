#include "extdll.h"
#include "util.h"
#include "cbase.h"

class CNullEntity : public CBaseEntity
{
public:
	virtual int	GetEntindexPriority() { return ENTIDX_PRIORITY_LOW; }
	void Spawn(void);
};

LINK_ENTITY_TO_CLASS(info_null, CNullEntity)

// Null Entity, remove on startup
void CNullEntity::Spawn(void)
{
	REMOVE_ENTITY(ENT(pev));
}