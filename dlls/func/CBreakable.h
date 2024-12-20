#ifndef FUNC_BREAK_H
#define FUNC_BREAK_H
#include "CBaseDelay.h"

typedef enum { expRandom, expDirected } Explosions;
typedef enum { matGlass = 0, matWood, matMetal, matFlesh, matCinderBlock, matCeilingTile, matComputer, matUnbreakableGlass, matRocks, matNone, matLastMaterial } Materials;

#define	NUM_SHARDS 6 // this many shards spawned when breakable objects break;
#define BREAK_INSTANT_WRENCH 20

class CBreakable : public CBaseDelay
{
public:
	// basic functions
	virtual int	GetEntindexPriority() { return ENTIDX_PRIORITY_NORMAL; }
	void Spawn(void);
	void Precache(void);
	void KeyValue(KeyValueData* pkvd);
	void EXPORT BreakTouch(CBaseEntity* pOther);
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	void DamageSound(void);
	const char* DisplayName();
	virtual Vector GetTargetOrigin() { return Center(); }

	// breakables use an overridden takedamage
	virtual int TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType);
	// To spark when hit
	void TraceAttack(entvars_t* pevAttacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType);

	BOOL IsBreakable(void);
	BOOL SparkWhenHit(void);

	int	 DamageDecal(int bitsDamageType);

	void EXPORT		Die();
	virtual int		ObjectCaps(void) { return (CBaseEntity::ObjectCaps() & ~FCAP_ACROSS_TRANSITION); }
	virtual int		Save(CSave& save);
	virtual int		Restore(CRestore& restore);

	inline BOOL		Explodable(void) { return ExplosionMagnitude() > 0; }
	inline int		ExplosionMagnitude(void) { return pev->impulse; }
	inline void		ExplosionSetMagnitude(int magnitude) { pev->impulse = magnitude; }

	static void MaterialSoundPrecache(Materials precacheMaterial);
	static void MaterialSoundRandom(edict_t* pEdict, Materials soundMaterial, float volume);
	static const char** MaterialSoundList(Materials precacheMaterial, int& soundCount);

	static const char* pSoundsWood[];
	static const char* pSoundsFlesh[];
	static const char* pSoundsGlass[];
	static const char* pSoundsMetal[];
	static const char* pSoundsConcrete[];
	static const char* pSpawnObjects[];

	static	TYPEDESCRIPTION m_SaveData[];

	Materials	m_Material;
	Explosions	m_Explosion;
	int			m_idShard;
	float		m_angle;
	int			m_iszGibModel;
	int			m_iszSpawnObject;
	string_t	m_displayName;
	EHANDLE		m_hActivator;
	int			m_instantBreakWeapon;
};

#endif	// FUNC_BREAK_H
