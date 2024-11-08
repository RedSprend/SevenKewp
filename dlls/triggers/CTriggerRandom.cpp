#include "extdll.h"
#include "eiface.h"
#include "util.h"
#include "gamerules.h"
#include "cbase.h"
#include "CRuleEntity.h"

//
// CTriggerRandom / trigger_random -- fires a random target

#define SF_TRAND_START_ON				1
#define SF_TRAND_TRIGGER_ONCE			2
#define SF_TRAND_REUSABLE				4
#define SF_TRAND_TIMED				8
#define SF_TRAND_UNIQUE				16

#define MAX_RANDOM_TARGETS 16

class CTriggerRandom : public CPointEntity
{
public:
	void Spawn(void);
	void KeyValue(KeyValueData* pkvd);
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	void EXPORT TimedThink();

	string_t SelectRandomTarget();

	string_t targets[MAX_RANDOM_TARGETS];
	bool firedTargets[MAX_RANDOM_TARGETS];

	int target_count;
	float min_delay;
	float max_delay;
	bool isActive;
};

LINK_ENTITY_TO_CLASS(trigger_random, CTriggerRandom)
LINK_ENTITY_TO_CLASS(trigger_random_unique, CTriggerRandom)
LINK_ENTITY_TO_CLASS(trigger_random_time, CTriggerRandom)

void CTriggerRandom::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "target_count"))
	{
		target_count = clampi(atoi(pkvd->szValue), 0, MAX_RANDOM_TARGETS);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "min_delay"))
	{
		min_delay = V_max(0, atof(pkvd->szValue));
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "max_delay"))
	{
		max_delay = V_max(0, atof(pkvd->szValue));
		pkvd->fHandled = TRUE;
	}
	else {
		static const char* targetKeys[MAX_RANDOM_TARGETS] = {
			"target1", "target2", "target3", "target4",
			"target5", "target6", "target7", "target8",
			"target9", "target10", "target11", "target12",
			"target13", "target14", "target15", "target16"
		};

		for (int i = 0; i < MAX_RANDOM_TARGETS; i++) {
			if (FStrEq(pkvd->szKeyName, targetKeys[i]))
			{
				targets[i] = ALLOC_STRING(pkvd->szValue);
				pkvd->fHandled = TRUE;
				return;
			}
		}

		CPointEntity::KeyValue(pkvd);
	}
}

void CTriggerRandom::Spawn(void)
{
	CPointEntity::Spawn();

	if (!strcmp(STRING(pev->classname), "trigger_random_unique")) {
		if (pev->spawnflags & 1) {
			// re-usable. Convert to trigger_random's reusable flag
			pev->spawnflags &= ~1;
			pev->spawnflags |= SF_TRAND_REUSABLE;
		}
		pev->spawnflags |= SF_TRAND_UNIQUE;
	}

	if (!strcmp(STRING(pev->classname), "trigger_random_time")) {
		pev->spawnflags |= SF_TRAND_TIMED;
	}
	
	if ((pev->spawnflags & SF_TRAND_TIMED) && (pev->spawnflags & SF_TRAND_START_ON)) {
		SetThink(&CTriggerRandom::TimedThink);
		pev->nextthink = gpGlobals->time + RANDOM_FLOAT(min_delay, max_delay);
	}

	// in sven, if you leave a target blank, it will be removed from the possible targets
	// so you can't ever trigger nothing. If you list 2 targets with a 4 target count,
	// then the target count becomes 2 and the empty slots are ignored. This doesn't
	// apply to slots that come after the specified target_count. So if you have a 4
	// target count, then list 1 blank followed by 4 targets, then only the first 3 targets are used.
	string_t newTargets[MAX_RANDOM_TARGETS];
	int newCount = 0;
	for (int i = 0; i < target_count; i++) {
		if (targets[i]) {
			newTargets[newCount++] = targets[i];
		}
	}
	memset(targets, 0, MAX_RANDOM_TARGETS * sizeof(string_t));
	memcpy(targets, newTargets, newCount * sizeof(string_t));
	target_count = newCount;
}

void CTriggerRandom::TimedThink() {
	string_t target = SelectRandomTarget();

	if (target) {
		FireTargets(STRING(target), this, this, USE_TOGGLE, 0);
	}

	if (pev->spawnflags & SF_TRAND_TRIGGER_ONCE) {
		SetThink(NULL);
		pev->nextthink = 0;
		isActive = false;
		return;
	}

	pev->nextthink = gpGlobals->time + RANDOM_FLOAT(min_delay, max_delay);
}

string_t CTriggerRandom::SelectRandomTarget() {
	int randomIdx;

	if (pev->spawnflags & SF_TRAND_UNIQUE) {
		int remainingTargetCount = 0;
		int remainingTargets[16];
		for (int i = 0; i < target_count; i++) {
			if (!firedTargets[i]) {
				remainingTargets[remainingTargetCount++] = i;
			}
		}

		if (remainingTargetCount == 0) {
			if (pev->spawnflags & SF_TRAND_REUSABLE) {
				memset(firedTargets, 0, sizeof(bool) * MAX_RANDOM_TARGETS);
				randomIdx = RANDOM_LONG(0, target_count - 1);
			}
			else {
				return 0; // fired all targets and is not flagged as reusable
			}
		}
		else {
			randomIdx = remainingTargets[RANDOM_LONG(0, remainingTargetCount - 1)];
		}
	}
	else {
		randomIdx = RANDOM_LONG(0, target_count - 1);
	}

	string_t target = targets[randomIdx];
	firedTargets[randomIdx] = true;

	return target;
}

void CTriggerRandom::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	isActive = !isActive; // yes, useType is ignored

	if (pev->spawnflags & SF_TRAND_TIMED) {
		if (isActive) {
			SetThink(&CTriggerRandom::TimedThink);
			pev->nextthink = gpGlobals->time + RANDOM_FLOAT(min_delay, max_delay);
		}
		else {
			SetThink(NULL);
			pev->nextthink = 0;
		}
		return;
	}

	string_t target = SelectRandomTarget();

	if (target) {
		FireTargets(STRING(target), pActivator, this, USE_TOGGLE, 0);
	}
}
