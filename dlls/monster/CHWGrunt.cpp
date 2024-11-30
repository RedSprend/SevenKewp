#include "extdll.h"
#include "plane.h"
#include "util.h"
#include "monsters.h"
#include "schedule.h"
#include "animation.h"
#include "CTalkSquadMonster.h"
#include "weapons.h"
#include "CTalkSquadMonster.h"
#include "env/CSoundEnt.h"
#include "effects.h"
#include "customentity.h"
#include "CBaseGrunt.h"

// TODO:
// - spinup has too much delay
// - can shoot you while not looking at you
// - abort route to minigun if moved or picked up
// - routing to minigun fails a lot
// - sometimes plays a minigun anim while turning with pistol
// - minigun looks broken while being picked up
// - reloads minigun if secondary empty

#define HWGRUNT_GUN_GROUP					1

#define HWGRUNT_GUN_MINIGUN					0
#define HWGRUNT_GUN_GLOCK					1
#define HWGRUNT_GUN_DEAGLE					2
#define HWGRUNT_GUN_357						3
#define HWGRUNT_GUN_NONE					4

#define HWGRUNT_GLOCK_CLIP_SIZE				9
#define HWGRUNT_DEAGLE_CLIP_SIZE			7
#define HWGRUNT_PYTHON_CLIP_SIZE			6

#define HWGRUNT_EVENT_SHOOT_PISTOL 1
#define HWGRUNT_EVENT_PICKUP_MINIGUN 12

#define HWGRUNT_MINIGUN_FIND_DISTANCE 1024

#define HWGRUNT_RANDOM_PISTOL 7

enum
{
	SCHED_HWGRUNT_FIND_MINIGUN = LAST_BASE_GRUNT_SCHEDULE + 1,
	SCHED_HWGRUNT_FIND_MINIGUN_FAIL,
	LAST_HWGRUNT_SCHEDULE
};

enum
{
	TASK_GET_PATH_TO_BEST_MINIGUN = LAST_BASE_GRUNT_TASK + 1,
	LAST_HWGRUNT_TASK
};

class CHWGrunt : public CBaseGrunt
{
public:
	void Spawn(void);
	void Precache(void);
	int  Classify(void);
	const char* DisplayName();
	void InitAiFlags();
	void PainSound(void);
	void DeathSound(void);
	void StartFollowingSound();
	void StopFollowingSound();
	void CantFollowSound();
	void Killed(entvars_t* pevAttacker, int iGib);
	void PlaySentenceSound(int sentenceType);
	void TraceAttack(entvars_t* pevAttacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType);
	Schedule_t* GetScheduleOfType(int Type);
	const char* GetTaskName(int taskIdx);
	void SetActivity(Activity NewActivity);
	int GetActivitySequence(Activity NewActivity);
	void HandleAnimEvent(MonsterEvent_t* pEvent);
	void PrescheduleThink(void);
	Schedule_t* GetEnemyOccludedSchedule(void);
	int LookupActivity(int activity);
	Schedule_t* GetMonsterStateSchedule(void);
	void StartTask(Task_t* pTask);
	void RunTask(Task_t* pTask);
	CBaseEntity* PBestMinigun(void);
	void DropMinigun(Vector vecDir);
	void PickupMinigun();

	CUSTOM_SCHEDULES;

private:
	static const char* pPainSounds[];
	static const char* pDeathSounds[];
	static const char* pGruntSentences[];

	bool minigunIsSpinning;
	float minigunSpinupTime;
	int minigunSpinupSeq;
	int minigunShootSeq;
	float nextMinigunShoot;
	int secondaryClipSize;
	int secondaryBody;
	float nextFindMinigunTime; // next time grunt is allowed to find a minigun
};

class CHWGruntRepel : public CBaseRepel
{
public:
	const char* GetMonsterType() { return "monster_hwgrunt"; };
};

LINK_ENTITY_TO_CLASS(monster_hwgrunt, CHWGrunt)
LINK_ENTITY_TO_CLASS(monster_hwgrunt_repel, CHWGruntRepel)

const char* CHWGrunt::pPainSounds[] =
{
	"hgrunt/gr_pain1.wav",
	"hgrunt/gr_pain2.wav",
	"hgrunt/gr_pain3.wav",
	"hgrunt/gr_pain4.wav",
	"hgrunt/gr_pain5.wav"
};

const char* CHWGrunt::pDeathSounds[] =
{
	"hgrunt/gr_die1.wav",
	"hgrunt/gr_pain2.wav",
	"hgrunt/gr_pain3.wav"
};

const char* CHWGrunt::pGruntSentences[] =
{
	"HG_GREN", // grenade scared grunt
	"HG_ALERT", // sees player
	"HG_MONSTER", // sees monster
	"HG_COVER", // running to cover
	"HG_THROW", // about to throw grenade
	"HG_CHARGE",  // running out to get the enemy
	"HG_TAUNT", // say rude things
};

void CHWGrunt::Spawn() {
	BaseSpawn();

	minigunShootSeq = LookupSequence("attack");
	minigunSpinupSeq = LookupSequence("spinup");
}

void CHWGrunt::Precache()
{
	m_voicePitch = 85 + RANDOM_LONG(0, 10);

	if (pev->weapons == 0 || pev->weapons == HWGRUNT_RANDOM_PISTOL) {
		pev->weapons = RANDOM_LONG(1, 3);

		// weapon isn't known until spawn time. For normal monsters this could be skipped
		// but then the precache list would change every map restart, so keep things simple
		// and precache everything to be safe on maps that are very close to overflows.
		//PrecacheEquipment(MEQUIP_GLOCK | MEQUIP_DEAGLE | MEQUIP_357);
	}

	m_iEquipment = MEQUIP_MINIGUN;

	pev->weapons = 3;
	/*
	switch(pev->weapons) {
	default:
	case 1:
		m_iEquipment |= MEQUIP_GLOCK;
		secondaryClipSize = HWGRUNT_GLOCK_CLIP_SIZE;
		secondaryBody = HWGRUNT_GUN_GLOCK;
		break;
	case 2:
		m_iEquipment |= MEQUIP_DEAGLE;
		secondaryClipSize = HWGRUNT_DEAGLE_CLIP_SIZE;
		secondaryBody = HWGRUNT_GUN_DEAGLE;
		break;
	case 3:
		m_iEquipment |= MEQUIP_357;
		secondaryClipSize = HWGRUNT_PYTHON_CLIP_SIZE;
		secondaryBody = HWGRUNT_GUN_357;
		break;
	}
	*/

	minigunIsSpinning = false;
	m_cAmmoLoaded = m_cClipSize = INT_MAX;
	nextMinigunShoot = 0;
	nextFindMinigunTime = 0;
	shellEjectAttachment = 1;
	m_flDistTooFar = 1536;

	BasePrecache();

	m_defaultModel = "models/hwgrunt.mdl";
	PRECACHE_MODEL(GetModel());

	PRECACHE_SOUND_ARRAY(pPainSounds);
	PRECACHE_SOUND_ARRAY(pDeathSounds);
}

int	CHWGrunt::Classify(void)
{
	return	CBaseMonster::Classify(CLASS_HUMAN_MILITARY);
}

const char* CHWGrunt::DisplayName() {
	return m_displayName ? CBaseMonster::DisplayName() : "Heavy Weapons Grunt";
}

void CHWGrunt::InitAiFlags() {
	canBeMadAtPlayer = false;
	waitForEnemyFire = false;
	runFromHeavyDamage = false;
	canCallMedic = false;
	suppressOccludedTarget = true;
	maxSuppressTime = 3.0f;
	maxShootDist = 2048;
}

void CHWGrunt::PainSound(void)
{
	if (gpGlobals->time > m_flNextPainTime)
	{
		EMIT_SOUND(ENT(pev), CHAN_VOICE, RANDOM_SOUND_ARRAY(pPainSounds), 1, ATTN_NORM);
		m_flNextPainTime = gpGlobals->time + 1;
	}
}

void CHWGrunt::DeathSound(void)
{
	EMIT_SOUND(ENT(pev), CHAN_VOICE, RANDOM_SOUND_ARRAY(pDeathSounds), 1, ATTN_NORM);
}

void CHWGrunt::StartFollowingSound() {
	int r = RANDOM_LONG(0, 2);

	switch (r) {
	case 0:
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "!HG_CHARGE1", GRUNT_SENTENCE_VOLUME, ATTN_NORM);
		break;
	case 1:
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "!HG_CHARGE2", GRUNT_SENTENCE_VOLUME, ATTN_NORM);
		break;
	case 2:
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "!HG_CHARGE3", GRUNT_SENTENCE_VOLUME, ATTN_NORM);
		break;
	}

	JustSpoke();
}

void CHWGrunt::StopFollowingSound() {
	int r = RANDOM_LONG(0, 2);

	switch (r) {
	case 0:
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "!HG_QUEST4", GRUNT_SENTENCE_VOLUME, ATTN_NORM);
		break;
	case 1:
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "!HG_ANSWER0", GRUNT_SENTENCE_VOLUME, ATTN_NORM);
		break;
	case 2:
		EMIT_SOUND(ENT(pev), CHAN_VOICE, "!HG_ANSWER1", GRUNT_SENTENCE_VOLUME, ATTN_NORM);
		break;
	}
	JustSpoke();
}

void CHWGrunt::CantFollowSound() {
	EMIT_SOUND(ENT(pev), CHAN_VOICE, "!HG_ANSWER3", GRUNT_SENTENCE_VOLUME, ATTN_NORM);
	JustSpoke();
}

void CHWGrunt::PlaySentenceSound(int sentenceType) {
	if (sentenceType >= (int)ARRAYSIZE(pGruntSentences)) {
		return;
	}
	SENTENCEG_PlayRndSz(ENT(pev), pGruntSentences[sentenceType], GRUNT_SENTENCE_VOLUME, GRUNT_ATTN, 0, m_voicePitch);
}

void CHWGrunt::TraceAttack(entvars_t* pevAttacker, float flDamage, Vector vecDir, TraceResult* ptr, int bitsDamageType) {
	
	if (HasEquipment(MEQUIP_MINIGUN) && bitsDamageType & DMG_BLAST && flDamage > 50) {
		// TODO: Add minigun player weapon
		//DropMinigun(vecDir);
	}

	CBaseGrunt::TraceAttack(pevAttacker, flDamage, vecDir, ptr, bitsDamageType);
}

void CHWGrunt::DropMinigun(Vector vecDir) {
	//Vector launchDir = Vector(vecDir.x * 200, vecDir.y * 200, 400);
	//DropEquipment(2, MEQUIP_MINIGUN, launchDir, Vector(0, RANDOM_FLOAT(200, 400), 0));
	m_cClipSize = m_cAmmoLoaded = secondaryClipSize;
	m_IdealActivity = ACT_SMALL_FLINCH;
	SetBodygroup(HWGRUNT_GUN_GROUP, secondaryBody);
}

void CHWGrunt::Killed(entvars_t* pevAttacker, int iGib)
{
	// stop minigun spin sound
	minigunIsSpinning = false;
	EMIT_SOUND_DYN(ENT(pev), CHAN_ITEM, "common/null.wav", 1.0, ATTN_NORM, 0, m_voicePitch);
	CBaseGrunt::Killed(pevAttacker, iGib);
}

void CHWGrunt::PickupMinigun() {

	CBaseEntity* minigun = NULL;
	CBaseEntity* ent = NULL;
	while ((ent = UTIL_FindEntityInSphere(ent, pev->origin, 32)) != NULL)
	{
		if (ent->pev->effects != EF_NODRAW && FClassnameIs(ent->pev, "weapon_9mmAR")) { // TODO: Minigun
			minigun = ent;
			break;
		}
	}

	if (!minigun) {
		return;
	}

	UTIL_Remove(minigun);

	m_cClipSize = m_cAmmoLoaded = INT_MAX;
	SetBodygroup(HWGRUNT_GUN_GROUP, HWGRUNT_GUN_MINIGUN);
	m_iEquipment |= MEQUIP_MINIGUN;
}

Task_t	tlFindMinigun[] =
{
	{ TASK_SET_FAIL_SCHEDULE,		(float)SCHED_HWGRUNT_FIND_MINIGUN_FAIL	},
	{ TASK_STOP_MOVING,				(float)0				},
	{ TASK_GET_PATH_TO_BEST_MINIGUN,(float)0				},
	{ TASK_RUN_PATH,				(float)0				},
	{ TASK_WAIT_FOR_MOVEMENT,		(float)0				},
	{ TASK_PLAY_SEQUENCE,			(float)ACT_EAT			}, // mmm miniguns *licks* ooooooa
	{ TASK_EAT,						(float)50				},
};

Schedule_t	slFindMinigun[] =
{
	{
		tlFindMinigun,
		ARRAYSIZE(tlFindMinigun),
		bits_COND_HEAVY_DAMAGE,

		0,
		"HWGRUNT_FIND_MINIGUN"
	},
};

DEFINE_CUSTOM_SCHEDULES(CHWGrunt)
{
	slFindMinigun
};

IMPLEMENT_CUSTOM_SCHEDULES(CHWGrunt, CBaseGrunt)

Schedule_t* CHWGrunt::GetScheduleOfType(int Type)
{
	switch (Type)
	{
	case SCHED_RANGE_ATTACK1:
		if (HasEquipment(MEQUIP_MINIGUN) && !minigunIsSpinning) {
			if (!minigunIsSpinning) {
				minigunIsSpinning = true;
				minigunSpinupTime = gpGlobals->time;
				return &slMinigunSpinup[0];
			}
			return &slGruntRangeAttack1C[0]; // prevent crouching or angry idle animations
		}
		return &slGruntRangeAttack1C[0]; // TODO: crouch shooting
	case SCHED_HWGRUNT_FIND_MINIGUN:
		nextFindMinigunTime = gpGlobals->time + RANDOM_LONG(4, 8);
		return &slFindMinigun[0];
	case SCHED_HWGRUNT_FIND_MINIGUN_FAIL:
		nextFindMinigunTime = gpGlobals->time + 20; // might be unreachable, don't try again for a long time
		return &slGruntFail[0];
	case SCHED_COWER:
		return &slGruntFail[0];
	case SCHED_SMALL_FLINCH:
	case SCHED_COMBAT_FACE:
	case SCHED_DIE:
		return CBaseGrunt::GetScheduleOfType(Type);
	case SCHED_TAKE_COVER_FROM_BEST_SOUND:
		if (minigunIsSpinning)
			return CBaseMonster::GetSchedule(); // don't take cover from sounds (too slow to react)
		else
			return CBaseGrunt::GetScheduleOfType(Type);
	default:
		if (minigunIsSpinning) {
			minigunIsSpinning = false;
			minigunSpinupTime = 0;
			//STOP_SOUND(edict(), CHAN_ITEM, "hassault/hw_spin.wav");
			EMIT_SOUND_DYN(ENT(pev), CHAN_ITEM, "common/null.wav", 1.0, ATTN_NORM, 0, m_voicePitch);
			EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "hassault/hw_spindown.wav", 1.0, ATTN_NORM, 0, m_voicePitch);
			return &slMinigunSpindown[0];
		}

		return CBaseGrunt::GetScheduleOfType(Type);
	}
}

const char* CHWGrunt::GetTaskName(int taskIdx) {
	switch (taskIdx) {
	case TASK_GET_PATH_TO_BEST_MINIGUN: return "TASK_GET_PATH_TO_BEST_MINIGUN";
	default:
		return CBaseGrunt::GetTaskName(taskIdx);
	}
}

void CHWGrunt::SetActivity(Activity NewActivity) {
	CBaseGrunt::SetActivity(NewActivity);

	if (NewActivity == ACT_THREAT_DISPLAY) {
		EMIT_SOUND_DYN(ENT(pev), CHAN_WEAPON, "hassault/hw_spinup.wav", 1.0, ATTN_NORM, 0, m_voicePitch);
		PointAtEnemy(); // otherwise he looks at the ground when starting the animation
	}
}

int CHWGrunt::GetActivitySequence(Activity NewActivity) {
	int iSequence = ACTIVITY_NOT_AVAILABLE;

	switch (NewActivity)
	{
	case ACT_IDLE:
		iSequence = HasEquipment(MEQUIP_MINIGUN) ? LookupActivity(NewActivity) : LookupSequence("pistol_idle");
		break;
	case ACT_WALK:
		iSequence = HasEquipment(MEQUIP_MINIGUN) ? LookupSequence("creeping_walk") : LookupSequence("pistol_walk");
		break;
	case ACT_RUN:
		iSequence = HasEquipment(MEQUIP_MINIGUN) ? LookupSequence("run") : LookupSequence("pistol_run");
		break;
	case ACT_RANGE_ATTACK1:
		if (HasEquipment(MEQUIP_MINIGUN)) {
			iSequence = LookupSequence("attack");
		} else {
			if (RANDOM_LONG(0, 9) == 0)
				m_fStanding = RANDOM_LONG(0, 1); // randomly stand or crouch

			iSequence = m_fStanding ? LookupSequence("pistol_shoot") : LookupSequence("pistol_crouchshoot");
		}
		break;
	case ACT_DISARM:
		iSequence = LookupSequence("spindown");
		break;
	case ACT_EAT:
		iSequence = LookupSequence("pickup_minigun");
		break;
	case ACT_TURN_LEFT:
	case ACT_TURN_RIGHT:
		iSequence = HasEquipment(MEQUIP_MINIGUN) ? LookupActivity(NewActivity) : LookupSequence("pistol_idle");
		break;
	default:
		iSequence = LookupActivity(NewActivity);
		break;
	}

	return iSequence;
}

void CHWGrunt::HandleAnimEvent(MonsterEvent_t* pEvent)
{
	Vector	vecShootDir;
	Vector	vecShootOrigin;

	switch (pEvent->event)
	{
	case HWGRUNT_EVENT_SHOOT_PISTOL:
		Shoot(false);
		break;
	case HWGRUNT_EVENT_PICKUP_MINIGUN:
		PickupMinigun();
		break;
	case HGRUNT_AE_DROP_GUN:
		if (DropEquipment(0, false))
			SetBodygroup(HWGRUNT_GUN_GROUP, GUN_NONE);
		break;
	default:
		CBaseGrunt::HandleAnimEvent(pEvent);
		break;
	}
}

void CHWGrunt::PrescheduleThink(void) {
	CBaseGrunt::PrescheduleThink();

	if (HasEquipment(MEQUIP_MINIGUN)) {
		if (pev->sequence == minigunShootSeq && gpGlobals->time >= nextMinigunShoot) {
			pev->nextthink = nextMinigunShoot = gpGlobals->time + 0.07f;
			Shoot(false);
		}
		if (minigunIsSpinning && minigunSpinupTime && gpGlobals->time - minigunSpinupTime > 1.0f) {
			EMIT_SOUND_DYN(edict(), CHAN_ITEM, "hassault/hw_spin.wav", 0.7f, ATTN_STATIC, 0, 100);
			minigunSpinupTime = 0; // sound loops automatically, don't need to keep playing
		}
		if (pev->sequence == minigunSpinupSeq) {
			PointAtEnemy();
		}
	}
}

Schedule_t* CHWGrunt::GetEnemyOccludedSchedule(void)
{
	return CBaseGrunt::GetEnemyOccludedSchedule();
}

int CHWGrunt::LookupActivity(int activity) {
	// model only has turning animations for the minigun stance
	if (!HasEquipment(MEQUIP_MINIGUN)) {
		if (activity == ACT_TURN_RIGHT || activity == ACT_TURN_LEFT) {
			return ACTIVITY_NOT_AVAILABLE;
		}
	}
	return CBaseAnimating::LookupActivity(activity);
}

Schedule_t* CHWGrunt::GetMonsterStateSchedule(void) {
	// todo: bits_COND_CAN_RANGE_ATTACK1?
	bool timeToFindMinigun = gpGlobals->time >= nextFindMinigunTime && !HasEquipment(MEQUIP_MINIGUN);
	if (timeToFindMinigun && !HasConditions(bits_COND_NEW_ENEMY | bits_COND_LIGHT_DAMAGE | bits_COND_HEAVY_DAMAGE))
	{
		return GetScheduleOfType(SCHED_HWGRUNT_FIND_MINIGUN);
	}

	return CBaseGrunt::GetMonsterStateSchedule();
}

CBaseEntity* CHWGrunt::PBestMinigun(void)
{
	CBaseEntity* bestEnt = NULL;
	float bestDist = FLT_MAX;
	bool bestIsReachable = false;

	Vector eyePos = EyePosition();

	CBaseEntity* ent = NULL;
	while ((ent = UTIL_FindEntityInSphere(ent, eyePos, HWGRUNT_MINIGUN_FIND_DISTANCE)) != NULL)
	{
		if (ent->pev->effects != EF_NODRAW && FClassnameIs(ent->pev, "weapon_9mmAR")) { // TODO: Minigun
			TraceResult tr;
			UTIL_TraceLine(eyePos, ent->pev->origin, ignore_monsters, dont_ignore_glass, ENT(pev), &tr);
			bool reachable = tr.flFraction >= 1.0f;

			if (bestIsReachable && !reachable) {
				continue;
			}

			float fDist = ((tr.vecEndPos - eyePos)*tr.flFraction).Length();

			// prefer the closest weapon which isn't blocked. Route to the closest blocked weapon otherwise.
			if (fDist < bestDist || (!bestIsReachable && reachable)) {
				bestDist = fDist;
				bestEnt = ent;
				bestIsReachable = reachable;
			}
		}
	}

	return bestEnt;
}

void CHWGrunt::StartTask(Task_t* pTask) {
	m_iTaskStatus = TASKSTATUS_RUNNING;

	switch (pTask->iTask)
	{
	case TASK_GET_PATH_TO_BEST_MINIGUN:
	{
		CBaseEntity* pGun = PBestMinigun();

		if (pGun && MoveToLocation(m_movementActivity, 2, pGun->pev->origin))
		{
			TaskComplete();
			ALERT(at_console, "MOVE COMPLETE!\n");
		}
		else
		{
			// no way to get there =(
			ALERT(at_aiconsole, "GetPathToBestMinigun failed!!\n");

			TaskFail();
		}
		break;
	}
	default:
		CBaseGrunt::StartTask(pTask);
	}
}

void CHWGrunt::RunTask(Task_t* pTask)
{
	switch (pTask->iTask)
	{
	case TASK_MOVE_TO_TARGET_RANGE:
	{
		CBaseMonster::RunTask(pTask);

		// always run when following someone because the walk speed is painfully slow
		m_movementActivity = ACT_RUN;
		break;
	}
	default:
	{
		CBaseMonster::RunTask(pTask);
	}
	}
}
