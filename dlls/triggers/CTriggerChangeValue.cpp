#include "extdll.h"
#include "eiface.h"
#include "util.h"
#include "gamerules.h"
#include "cbase.h"
#include "CRuleEntity.h"

//
// CTriggerChangeValue / trigger_changevalue -- changes an entity keyvalue

#define SF_DONT_USE_X 1
#define SF_DONT_USE_Y 2
#define SF_DONT_USE_Z 4
#define SF_CONSTANT 8
//#define SF_START_ON 16 // does nothing according to docs
#define SF_INVERT_TARGET_VALUE 32
#define SF_INVERT_SOURCE_VALUE 64 // TODO: this is pointless. Ripent this out of every map that uses it.
#define SF_MULTIPLE_DESTINATIONS 128 // TODO: When you would ever want to target only the first entity found? As a mapper, you have no idea what that will be. It just seems like a bug.

// from rehlds
#define MAX_KEY_NAME_LEN 256
#define MAX_KEY_VALUE_LEN 2048

enum changevalue_operations {
	CVAL_OP_REPLACE = 0,
	CVAL_OP_ADD = 1,
	CVAL_OP_MUL = 2,
	CVAL_OP_SUB = 3,
	CVAL_OP_DIV = 4,
	CVAL_OP_AND = 5,
	CVAL_OP_OR = 6,
	CVAL_OP_NAND = 7,
	CVAL_OP_NOR = 8,
	CVAL_OP_DIR2ANG = 9,
	CVAL_OP_ANG2DIR = 10,
	CVAL_OP_APPEND = 11, 
	CVAL_OP_MOD = 12,
	CVAL_OP_XOR = 13,
	CVAL_OP_NXOR = 14,
	CVAL_OP_POW = 16,
	CVAL_OP_SIN = 17,
	CVAL_OP_COS = 18,
	CVAL_OP_TAN = 19,
	CVAL_OP_ARCSIN = 20,
	CVAL_OP_ARCCOS = 21,
	CVAL_OP_ARCTAN = 22,
	CVAL_OP_COT = 23,
	CVAL_OP_ARCCOT = 24,
};

enum changevalue_trig_behavior {
	CVAL_TRIG_DEGREES,
	CVAL_TRIG_RADIANS
};

enum copyvalue_float_conv {
	CVAL_STR_6DP = 0, // 6 decimal places when converted to string
	CVAL_STR_5DP = 1,
	CVAL_STR_4DP = 4,
	CVAL_STR_3DP = 7,
	CVAL_STR_2DP = 10,
	CVAL_STR_1DP = 13,
	CVAL_STR_ROUND = 16, // round to nearest whole number when converted to string/int
	CVAL_STR_CEIL = 17,
	CVAL_STR_FLOOR = 18,
};

class CTriggerChangeValue : public CPointEntity
{
public:
	void Spawn(void);
	void KeyValue(KeyValueData* pkvd);
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value);
	void EXPORT TimedThink();

	int OperateInteger(int sourceVal, int targetVal);
	float OperateFloat(float sourceVal, float targetVal);
	Vector OperateVector(CKeyValue& keyvalue);
	const char* OperateString(const char* sourceVal, const char* targetVal);
	const char* Operate(CKeyValue& keyvalue);
	void HandleTarget(CBaseEntity* ent);
	void ChangeValues();
	float VectorToFloat(Vector v);
	int FloatToInteger(float v);
	std::string FloatToString(float v);
	std::string VectorToString(Vector v);

	string_t m_iszKeyName;
	string_t m_iszNewValue;
	int m_iAppendSpaces;
	int m_iOperation;

	string_t m_iszSrcKeyName;
	int m_iFloatConversion;

	// source values loaded from source entity or static keyvalue
	Vector m_vSrc;
	float m_fSrc;
	int m_iSrc;
	const char* m_sSrc;

	float m_trigIn; // multiplier for trigonemtric function input values (degrees -> degrees/radians)
	float m_trigOut; // multiplier for trigonemtric function output values (radians -> degrees/radians)

	bool isCopyValue; // this entitiy is a trigger_copyvalue not a trigger_changevalue
	bool isActive;
	EHANDLE h_activator;
	EHANDLE h_caller;
};

LINK_ENTITY_TO_CLASS(trigger_changevalue, CTriggerChangeValue);
LINK_ENTITY_TO_CLASS(trigger_copyvalue, CTriggerChangeValue);

void CTriggerChangeValue::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "m_iszValueName") || FStrEq(pkvd->szKeyName, "m_iszDstValueName"))
	{
		m_iszKeyName = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszNewValue"))
	{
		m_iszNewValue = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iAppendSpaces"))
	{
		m_iAppendSpaces = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszValueType"))
	{
		m_iOperation = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_trigonometricBehaviour"))
	{
		int behavior = atoi(pkvd->szValue);

		if (behavior == CVAL_TRIG_DEGREES) {
			// trig functions work with radians, so input/output values need conversion
			m_trigIn = M_PI / 180.0f;
			m_trigOut = 180.0f / M_PI;
		}
		else if (behavior == CVAL_TRIG_RADIANS) {
			m_trigIn = 1.0f;
			m_trigOut = 1.0f;
		}
		else {
			ALERT(at_warning, "trigger_changevalue: invalid trig behavior %d\n", behavior);
		}
		
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iszSrcValueName"))
	{
		m_iszSrcKeyName = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iFloatConversion"))
	{
		m_iFloatConversion = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else {
		CPointEntity::KeyValue(pkvd);
	}
}

void CTriggerChangeValue::Spawn(void)
{
	CPointEntity::Spawn();

	if (m_iAppendSpaces > 0 && (m_iOperation == CVAL_OP_APPEND || m_iOperation == CVAL_OP_REPLACE)) {
		static char spacesVal[MAX_KEY_VALUE_LEN];
		const int maxStrLen = MAX_KEY_VALUE_LEN - 2; // exludes the null char

		int oldLen = V_min(maxStrLen, strlen(STRING(m_iszNewValue)));
		int spacesToAppend = V_min(maxStrLen - oldLen, m_iAppendSpaces);

		strncpy(spacesVal, STRING(m_iszNewValue), maxStrLen);
		memset(spacesVal + oldLen, ' ', spacesToAppend);
		spacesVal[oldLen + spacesToAppend] = '\0';

		m_iszNewValue = ALLOC_STRING(spacesVal);
	}

	isCopyValue = !strcmp(STRING(pev->classname), "trigger_copyvalue");

	if (!isCopyValue) {
		// disable copyvalue-specific settings
		pev->spawnflags &= ~(SF_CONSTANT | SF_MULTIPLE_DESTINATIONS);
		pev->netname = 0;
		pev->dmg = 0;
		m_iszSrcKeyName = 0;
		m_iFloatConversion = 0;
	}
	else {
		// disable changevalue-specific settings
		m_iszNewValue = 0;
	}

	SetUse(&CTriggerChangeValue::Use);
}

int CTriggerChangeValue::OperateInteger(int sourceVal, int targetVal) {
	if (pev->spawnflags & SF_INVERT_TARGET_VALUE) {
		targetVal = -targetVal;
	}
	if (pev->spawnflags & SF_INVERT_SOURCE_VALUE) {
		sourceVal = -sourceVal;
	}

	switch (m_iOperation) {
	case CVAL_OP_REPLACE: return sourceVal;
	case CVAL_OP_ADD: return targetVal + sourceVal;
	case CVAL_OP_MUL: return targetVal * sourceVal;
	case CVAL_OP_SUB: return targetVal - sourceVal;
	case CVAL_OP_DIV: return targetVal / sourceVal;
	case CVAL_OP_POW: return powf(targetVal, sourceVal);
	case CVAL_OP_MOD: return targetVal % sourceVal;
	case CVAL_OP_AND: return targetVal & sourceVal;
	case CVAL_OP_OR: return targetVal | sourceVal;
	case CVAL_OP_XOR: return targetVal ^ sourceVal;
	case CVAL_OP_NAND: return ~(targetVal & sourceVal);
	case CVAL_OP_NOR: return ~(targetVal | sourceVal);
	case CVAL_OP_NXOR: return ~(targetVal ^ sourceVal);
	case CVAL_OP_SIN: return sinf(sourceVal * m_trigIn) * m_trigOut;
	case CVAL_OP_COS: return cosf(sourceVal * m_trigIn) * m_trigOut;
	case CVAL_OP_TAN: return tanf(sourceVal * m_trigIn) * m_trigOut;
	case CVAL_OP_COT: return (1.0f / tanf(sourceVal * m_trigIn)) * m_trigOut;
	case CVAL_OP_ARCSIN: return asinf(sourceVal * m_trigIn) * m_trigOut;
	case CVAL_OP_ARCCOS: return acosf(sourceVal * m_trigIn) * m_trigOut;
	case CVAL_OP_ARCTAN: return atanf(sourceVal * m_trigIn) * m_trigOut;
	case CVAL_OP_ARCCOT: return atanf(1.0f / (sourceVal * m_trigIn)) * m_trigOut;
	default:
		ALERT(at_warning, "'%s' (%s): invalid operation %d on integer value\n",\
			pev->targetname ? STRING(pev->targetname) : "", STRING(pev->classname), m_iOperation);
		return targetVal;
	}
}

float CTriggerChangeValue::OperateFloat(float sourceVal, float targetVal) {
	if (pev->spawnflags & SF_INVERT_TARGET_VALUE) {
		targetVal = -targetVal;
	}
	if (pev->spawnflags & SF_INVERT_SOURCE_VALUE) {
		sourceVal = -sourceVal;
	}

	switch (m_iOperation) {
	case CVAL_OP_REPLACE: return sourceVal;
	case CVAL_OP_ADD: return targetVal + sourceVal;
	case CVAL_OP_MUL: return targetVal * sourceVal;
	case CVAL_OP_SUB: return targetVal - sourceVal;
	case CVAL_OP_DIV: return targetVal / sourceVal;
	case CVAL_OP_POW: return powf(targetVal, sourceVal);
	case CVAL_OP_SIN: return sinf(sourceVal * m_trigIn) * m_trigOut;
	case CVAL_OP_COS: return cosf(sourceVal * m_trigIn) * m_trigOut;
	case CVAL_OP_TAN: return tanf(sourceVal * m_trigIn) * m_trigOut;
	case CVAL_OP_COT: return (1.0f / tanf(sourceVal * m_trigIn)) * m_trigOut;
	case CVAL_OP_ARCSIN: return asinf(sourceVal * m_trigIn) * m_trigOut;
	case CVAL_OP_ARCCOS: return acosf(sourceVal * m_trigIn) * m_trigOut;
	case CVAL_OP_ARCTAN: return atanf(sourceVal * m_trigIn) * m_trigOut;
	case CVAL_OP_ARCCOT: return atanf(1.0f / (sourceVal * m_trigIn)) * m_trigOut;
	default:
		ALERT(at_warning, "'%s' (%s): invalid operation %d on float value\n",
			pev->targetname ? STRING(pev->targetname) : "", STRING(pev->classname), m_iOperation);
		return targetVal;
	}
}

Vector CTriggerChangeValue::OperateVector(CKeyValue& keyvalue) {
	Vector vTemp = keyvalue.vVal;

	switch (m_iOperation) {
	case CVAL_OP_DIR2ANG:
		return UTIL_VecToAngles(vTemp.Normalize());
	case CVAL_OP_ANG2DIR:
		UTIL_MakeVectors(vTemp);
		return gpGlobals->v_forward;
	default:
		if (!(pev->spawnflags & SF_DONT_USE_X)) {
			vTemp[0] = OperateFloat(m_vSrc[0], keyvalue.vVal[0]);
		}
		if (!(pev->spawnflags & SF_DONT_USE_Y)) {
			vTemp[1] = OperateFloat(m_vSrc[1], keyvalue.vVal[1]);
		}
		if (!(pev->spawnflags & SF_DONT_USE_Z)) {
			vTemp[2] = OperateFloat(m_vSrc[2], keyvalue.vVal[2]);
		}
		return vTemp;
	}
}

const char* CTriggerChangeValue::Operate(CKeyValue& keyvalue) {
	switch (keyvalue.keyType) {
	case KEY_TYPE_FLOAT:
		return UTIL_VarArgs("%f", OperateFloat(m_fSrc, keyvalue.fVal));
	case KEY_TYPE_INT:
		return UTIL_VarArgs("%d", OperateInteger(m_iSrc, keyvalue.iVal));
	case KEY_TYPE_VECTOR: {
		Vector vTemp = OperateVector(keyvalue);
		return UTIL_VarArgs("%f %f %f", vTemp.x, vTemp.y, vTemp.z);
	}
	case KEY_TYPE_STRING:
		return OperateString(m_sSrc, keyvalue.sVal ? STRING(keyvalue.sVal) : "");
	default:
		ALERT(at_warning, "'%s' (%s): operation on keyvalue %s not allowed\n", 
			pev->targetname ? STRING(pev->targetname) : "", STRING(pev->classname), keyvalue.desc->fieldName);
		return "";
	}
}

const char* CTriggerChangeValue::OperateString(const char* sourceVal, const char* targetVal) {
	switch (m_iOperation) {
	case CVAL_OP_REPLACE:
		return sourceVal;
	case CVAL_OP_APPEND:
		return UTIL_VarArgs("%s%s", targetVal, sourceVal);
	default:
		ALERT(at_warning, "'%s' (%s): invalid operation %d on string value\n",
			pev->targetname ? STRING(pev->targetname) : "", STRING(pev->classname), m_iOperation);
		return targetVal;
	}
}

void CTriggerChangeValue::HandleTarget(CBaseEntity* pent) {
	if (!pent)
		return;

	KeyValueData dat;
	dat.fHandled = false;
	dat.szKeyName = (char*)STRING(m_iszKeyName);
	dat.szValue = (char*)m_sSrc;
	dat.szClassName = (char*)STRING(pent->pev->classname);

	CKeyValue keyvalue = pent->GetKeyValue(dat.szKeyName);

	if (keyvalue.desc) {
		// TODO: operate on entvars data directly, don't make a new keyvalue
		dat.szValue = Operate(keyvalue);
	}
	else {
		ALERT(at_warning, "'%s' (%s): keyvalue '%s' in entity '%s' can only be replaced\n",
			pev->targetname ? STRING(pev->targetname) : "", STRING(pev->classname), dat.szKeyName, STRING(pent->pev->classname));
	}

	DispatchKeyValue(pent->edict(), &dat); // using this for private fields
	UTIL_SetOrigin(pent->pev, pent->pev->origin); // in case any changes to solid/origin were made
}

float CTriggerChangeValue::VectorToFloat(Vector v) {
	if (pev->spawnflags & SF_DONT_USE_X) {
		v.x = 0;
	}
	if (pev->spawnflags & SF_DONT_USE_Y) {
		v.y = 0;
	}
	if (pev->spawnflags & SF_DONT_USE_Z) {
		v.z = 0;
	}

	return v.Length();
}

int CTriggerChangeValue::FloatToInteger(float f) {
	switch (m_iFloatConversion) {
	case CVAL_STR_ROUND: return f + 0.5f;
	case CVAL_STR_CEIL: return ceilf(f);
	case CVAL_STR_FLOOR: 
	default:
		return f;
	}
}

std::string CTriggerChangeValue::FloatToString(float f) {
	switch (m_iFloatConversion) {
	case CVAL_STR_5DP: return UTIL_VarArgs("%.5f", f);
	case CVAL_STR_4DP: return UTIL_VarArgs("%.4f", f);
	case CVAL_STR_3DP: return UTIL_VarArgs("%.3f", f);
	case CVAL_STR_2DP: return UTIL_VarArgs("%.2f", f);
	case CVAL_STR_1DP: return UTIL_VarArgs("%.1f", f);
	case CVAL_STR_ROUND: return UTIL_VarArgs("%d", (int)(f + 0.5f));
	case CVAL_STR_CEIL: return UTIL_VarArgs("%d", (int)ceilf(f));
	case CVAL_STR_FLOOR: return UTIL_VarArgs("%d", (int)f);
	case CVAL_STR_6DP:
	default:
		return UTIL_VarArgs("%f", f);
	}
}

std::string CTriggerChangeValue::VectorToString(Vector v) {
	std::string s;

	if (!(pev->spawnflags & SF_DONT_USE_X)) {
		s += FloatToString(v.x);
	}
	if (!(pev->spawnflags & SF_DONT_USE_Y)) {
		if (s.length()) {
			s += " ";
		}
		s += FloatToString(v.y);
	}
	if (!(pev->spawnflags & SF_DONT_USE_Z)) {
		if (s.length()) {
			s += " ";
		}
		s += FloatToString(v.z);
	}

	return s;
}

void CTriggerChangeValue::ChangeValues() {
	if (isCopyValue) {
		if (!pev->netname) {
			ALERT(at_warning, "'%s' (%s): missing source entity\n",
				pev->targetname ? STRING(pev->targetname) : "", STRING(pev->classname));
			return;
		}
		if (!m_iszSrcKeyName) {
			ALERT(at_warning, "'%s' (%s): missing source key\n",
				pev->targetname ? STRING(pev->targetname) : "", STRING(pev->classname));
			return;
		}
	}
	else {
		if (!m_iszKeyName) {
			ALERT(at_warning, "'%s' (%s): missing key name\n",
				pev->targetname ? STRING(pev->targetname) : "", STRING(pev->classname));
			return;
		}
	}	

	// defined here to prevent destruction before keyvalues are sent,
	// and this exists at all because UTIL_VarArgs uses a single static buffer
	std::string temp;

	// set up source values
	if (isCopyValue) {
		edict_t* ent = FIND_ENTITY_BY_TARGETNAME(NULL, STRING(pev->netname));
		CBaseEntity* pent = CBaseEntity::Instance(ent);
		if (pent) {
			CKeyValue srcKey = pent->GetKeyValue(STRING(m_iszSrcKeyName));
			if (srcKey.desc) {
				switch (srcKey.keyType) {
				case KEY_TYPE_FLOAT:
					m_iSrc = m_fSrc = srcKey.fVal;
					m_vSrc = Vector(srcKey.fVal, srcKey.fVal, srcKey.fVal);
					temp = FloatToString(srcKey.fVal);
					m_sSrc = temp.c_str();
					break;
				case KEY_TYPE_INT:
					m_fSrc = m_iSrc = srcKey.iVal;
					m_vSrc = Vector(srcKey.iVal, srcKey.iVal, srcKey.iVal);
					m_sSrc = UTIL_VarArgs("%d", srcKey.iVal);
					break;
				case KEY_TYPE_VECTOR: {
					m_iSrc = m_fSrc = VectorToFloat(srcKey.vVal);
					m_vSrc = srcKey.vVal;
					temp = VectorToString(srcKey.vVal);
					m_sSrc = temp.c_str();
					break;
				}
				case KEY_TYPE_STRING:
					m_fSrc = m_iSrc = 0;
					m_vSrc = g_vecZero;
					m_sSrc = srcKey.sVal ? STRING(srcKey.sVal) : "";
					break;
				default:
					ALERT(at_warning, "'%s' (%s): operation on keyvalue %s not allowed\n",
						pev->targetname ? STRING(pev->targetname) : "", STRING(pev->classname), srcKey.desc->fieldName);
					return;
				}
			}
		}
		else {
			m_iSrc = m_fSrc = 0;
			m_sSrc = "";
			m_vSrc = g_vecZero;
		}
	}
	else {
		// trigger_changevalue static value
		if (m_iszNewValue) {
			const char* cNewVal = STRING(m_iszNewValue);
			UTIL_StringToVector(m_vSrc, cNewVal);

			if (m_vSrc.y != 0 || m_vSrc.z != 0) {
				// length of vector used when assigning a vector to a float/int
				m_iSrc = m_fSrc = VectorToFloat(m_vSrc);
			}
			else {
				// single value used
				m_fSrc = atof(cNewVal);
				m_iSrc = atoi(cNewVal);
				m_vSrc = Vector(m_fSrc, m_fSrc, m_fSrc);
			}

			m_sSrc = STRING(m_iszNewValue);
		}
		else {
			m_iSrc = m_fSrc = 0;
			m_sSrc = "";
			m_vSrc = g_vecZero;
		}
	}

	const char* target = STRING(pev->target);

	if (!strcmp(target, "!activator")) {
		HandleTarget(h_activator);
	}
	else if (!strcmp(target, "!caller")) {
		HandleTarget(h_caller);
	}
	else {
		edict_t* ent = NULL;

		while (!FNullEnt(ent = FIND_ENTITY_BY_TARGETNAME(ent, target))) {
			HandleTarget(CBaseEntity::Instance(ent));

			if (isCopyValue && !(pev->spawnflags & SF_MULTIPLE_DESTINATIONS)) {
				break;
			}
		}
	}

	if (pev->message) {
		FireTargets(STRING(pev->message), h_activator, this, USE_TOGGLE, 0.0f);
	}
}

void CTriggerChangeValue::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	h_activator = pActivator;
	h_caller = pCaller;

	ChangeValues();

	if (pev->spawnflags & SF_CONSTANT) {
		isActive = useType == USE_TOGGLE ? !isActive : useType;

		if (isActive) {
			SetThink(&CTriggerChangeValue::TimedThink);
			pev->nextthink = gpGlobals->time + pev->dmg;
		}
		else {
			SetThink(NULL);
			pev->nextthink = 0;
		}
	}
}

void CTriggerChangeValue::TimedThink() {
	ChangeValues();
	pev->nextthink = gpGlobals->time + pev->dmg;
}
