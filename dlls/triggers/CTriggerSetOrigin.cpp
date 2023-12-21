#include "extdll.h"
#include "eiface.h"
#include "util.h"
#include "cbase.h"

//
// CTriggerSetOrigin / trigger_setorigin -- copies position and orientation of one entity to another

#define SF_CONSTANT					1	// Always update the target's position and orientation.
#define SF_SET_ONCE					4	// Only update the target once then remove the trigger_setorigin.

// Use the vector between the target and the copypointer plus the "Offset"-keyvalue as the offset vector.
// Also implies copy of X/Y/Z axes. TODO: X/Y/Z angles as well?
#define SF_LOCK_OFFSETS				8

#define SF_COPY_X_ANGLE				16
#define SF_COPY_Y_ANGLE				32
#define SF_COPY_Z_ANGLE				64
#define SF_COPY_X_AXIS				128
#define SF_COPY_Y_AXIS				256
#define SF_COPY_Z_AXIS				512

// If set, the target entity will not be moved to the copypointer's origin before doing
// the offset difference calculation (set this unless you want the target entity stuck
// to the center of the copypointer).
#define SF_SKIP_INITIAL_SET			1024

// TODO: test if sven entity scans for ents every frame or if it caches them
#define MAX_TARGETS 512 // 512 = max visible ents for an area. Doubtful a mapper would go beyond that.

class CTriggerSetOrigin : public CPointEntity
{
public:
	void Spawn(void);
	void KeyValue(KeyValueData* pkvd);
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	void EXPORT ConstantModeThink();

	void UpdateEntity();

	string_t m_copyPointer;
	Vector m_offset;
	Vector m_angleoffset;
	bool m_invertAngleX;
	bool m_invertAngleY;
	bool m_invertAngleZ;

	EHANDLE m_hCopyEnt;
	EHANDLE m_hTargets[MAX_TARGETS];
	Vector m_lockOffsets[MAX_TARGETS];
	Vector m_lockOffsetAngles[MAX_TARGETS];
	int m_targetCount;
	bool m_isActive;
	
};

LINK_ENTITY_TO_CLASS(trigger_setorigin, CTriggerSetOrigin);

void CTriggerSetOrigin::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "copypointer"))
	{
		m_copyPointer = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "offset"))
	{
		UTIL_StringToVector(m_offset, pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "angleoffset"))
	{
		UTIL_StringToVector(m_angleoffset, pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "invert_x"))
	{
		m_invertAngleX = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "invert_y"))
	{
		m_invertAngleY = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "invert_z"))
	{
		m_invertAngleZ = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else {
		CPointEntity::KeyValue(pkvd);
	}
}

void CTriggerSetOrigin::Spawn(void)
{
	CPointEntity::Spawn();
}

void CTriggerSetOrigin::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	m_isActive = useType == USE_TOGGLE ? !m_isActive : useType == USE_ON;

	m_hCopyEnt = m_copyPointer ? UTIL_FindEntityByTargetname(NULL, STRING(m_copyPointer)) : NULL;

	m_targetCount = 0;
	edict_t* ent = NULL;
	while (!FNullEnt(ent = FIND_ENTITY_BY_TARGETNAME(ent, STRING(pev->target)))) {
		if (m_targetCount >= MAX_TARGETS) {
			ALERT(at_console, "trigger_setorigin (%s): Max target count exceeded!", STRING(pev->targetname));
			break;
		}
		m_hTargets[m_targetCount++] = CBaseEntity::Instance(ent);
	}
	
	if (FNullEnt(m_hCopyEnt.GetEdict()) || m_targetCount == 0) {
		if (pev->spawnflags & SF_SET_ONCE) {
			UTIL_Remove(this);
		}
		return;
	}

	if (pev->spawnflags & SF_LOCK_OFFSETS) {

		for (int i = 0; i < m_targetCount; i++) {
			if (pev->spawnflags & SF_SKIP_INITIAL_SET) {
				m_lockOffsets[i] = m_hTargets[i]->pev->origin - m_hCopyEnt->pev->origin;
			}
			else {
				m_lockOffsets[i] = Vector(0, 0, 0); // TODO: just guessing after reading docs
			}

			m_lockOffsetAngles[i] = m_hTargets[i]->pev->angles - m_hCopyEnt->pev->angles;
		}
		
	}

	UpdateEntity();

	if (pev->spawnflags & SF_SET_ONCE) {
		UTIL_Remove(this);
		return;
	}

	if (pev->spawnflags & SF_CONSTANT) {
		if (m_isActive) {
			SetThink(&CTriggerSetOrigin::ConstantModeThink);
			pev->nextthink = gpGlobals->time;
		}
		else {
			SetThink(NULL);
			pev->nextthink = 0;
		}
	}
}

void CTriggerSetOrigin::ConstantModeThink() {
	UpdateEntity();
	pev->nextthink = gpGlobals->time;
}

void CTriggerSetOrigin::UpdateEntity() {
	if (FNullEnt(m_hCopyEnt.GetEdict())) {
		return;
	}

	for (int i = 0; i < m_targetCount; i++) {
		if (FNullEnt(m_hTargets[i].GetEdict())) {
			continue;
		}

		entvars_t* targetPev = m_hTargets[i]->pev;
		Vector& lockOffset = m_lockOffsets[i];
		Vector& lockOffsetAngles = m_lockOffsetAngles[i];

		if (pev->spawnflags & (SF_COPY_X_AXIS | SF_LOCK_OFFSETS)) {
			targetPev->origin.x = m_hCopyEnt->pev->origin.x + m_offset.x + lockOffset.x;
		}
		if (pev->spawnflags & (SF_COPY_Y_AXIS | SF_LOCK_OFFSETS)) {
			targetPev->origin.y = m_hCopyEnt->pev->origin.y + m_offset.y + lockOffset.y;
		}
		if (pev->spawnflags & (SF_COPY_Z_AXIS | SF_LOCK_OFFSETS)) {
			targetPev->origin.z = m_hCopyEnt->pev->origin.z + m_offset.z + lockOffset.z;
		}

		if (pev->spawnflags & (SF_COPY_X_ANGLE | SF_LOCK_OFFSETS)) {
			float v = m_hCopyEnt->pev->angles.x;
			targetPev->angles.x = (m_invertAngleX ? -v : v) + m_angleoffset.x + lockOffsetAngles.x;
		}
		if (pev->spawnflags & (SF_COPY_Y_ANGLE | SF_LOCK_OFFSETS)) {
			float v = m_hCopyEnt->pev->angles.y;
			targetPev->angles.y = (m_invertAngleY ? -v : v) + m_angleoffset.y + lockOffsetAngles.y;
		}
		if (pev->spawnflags & (SF_COPY_Z_ANGLE | SF_LOCK_OFFSETS)) {
			float v = m_hCopyEnt->pev->angles.z;
			targetPev->angles.z = (m_invertAngleZ ? -v : v) + m_angleoffset.z + lockOffsetAngles.z;
		}

		// TODO: might not be needed but I know this has fixed bugs in the past
		UTIL_SetOrigin(m_hTargets[i]->pev, m_hTargets[i]->pev->origin);
	}
}