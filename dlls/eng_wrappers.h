// engine functions that have been wrapped to extend functionality

#pragma once
#include "Bsp.h"

class CBaseEntity;

extern Bsp g_bsp;

// resources that were successfully precached
extern std::unordered_map<std::string, std::string> g_precachedModels; // storing values so GET_MODEL can be used with MAKE_STRING
extern std::unordered_set<std::string> g_missingModels; // storing values so GET_MODEL can be used with MAKE_STRING
extern std::unordered_set<std::string> g_precachedSounds;
extern std::unordered_set<std::string> g_precachedGeneric;
extern std::unordered_map<std::string, int> g_precachedEvents;

// resources that attempted to precache but may have been replaced with a failure model
extern std::unordered_set<std::string> g_tryPrecacheModels;
extern std::unordered_set<std::string> g_tryPrecacheSounds;
extern std::unordered_set<std::string> g_tryPrecacheGeneric;
extern std::unordered_set<std::string> g_tryPrecacheEvents;

#ifdef CLIENT_DLL
#define PRECACHE_MODEL	(*g_engfuncs.pfnPrecacheModel)
#define SET_MODEL		(*g_engfuncs.pfnSetModel)
#define PRECACHE_SOUND	(*g_engfuncs.pfnPrecacheSound)
#define PRECACHE_EVENT	(*g_engfuncs.pfnPrecacheEvent)
#define MODEL_INDEX		(*g_engfuncs.pfnModelIndex)
#define GET_MODEL(model) model
inline void MESSAGE_BEGIN(int msg_dest, int msg_type, const float* pOrigin = NULL, edict_t* ed = NULL) {
	(*g_engfuncs.pfnMessageBegin)(msg_dest, msg_type, pOrigin, ed);
}
#define MESSAGE_END		(*g_engfuncs.pfnMessageEnd)
#define WRITE_BYTE		(*g_engfuncs.pfnWriteByte)
#define WRITE_CHAR		(*g_engfuncs.pfnWriteChar)
#define WRITE_SHORT		(*g_engfuncs.pfnWriteShort)
#define WRITE_LONG		(*g_engfuncs.pfnWriteLong)
#define WRITE_ANGLE		(*g_engfuncs.pfnWriteAngle)
#define WRITE_COORD		(*g_engfuncs.pfnWriteCoord)
#define WRITE_STRING	(*g_engfuncs.pfnWriteString)
#define WRITE_ENTITY	(*g_engfuncs.pfnWriteEntity)
#define GET_MODEL_PTR	(*g_engfuncs.pfnGetModelPtr)
#define CREATE_NAMED_ENTITY		(*g_engfuncs.pfnCreateNamedEntity)
#else
// engine wrappers which handle model/sound replacement logic
EXPORT int PRECACHE_GENERIC(const char* path);
EXPORT int PRECACHE_SOUND_ENT(CBaseEntity* ent, const char* path);
EXPORT int PRECACHE_SOUND_NULLENT(const char* path);
EXPORT int PRECACHE_MODEL(const char* model);
EXPORT int PRECACHE_REPLACEMENT_MODEL(const char* model); // only precache the model if it will be replaced
EXPORT int PRECACHE_EVENT(int id, const char* path);
EXPORT bool SET_MODEL(edict_t* edict, const char* model); // returns true if the given model was swapped with something else
EXPORT bool SET_MODEL_MERGED(edict_t* edict, const char* model, int mergeId); // will set the merged model and body if the given model was not replaced
EXPORT const char* GET_MODEL(const char* model); // return replacement model, if one exists, or the given model
EXPORT int MODEL_INDEX(const char* model);
EXPORT void* GET_MODEL_PTR(edict_t* edict);
EXPORT edict_t* CREATE_NAMED_ENTITY(string_t cname);
#define PRECACHE_SOUND(path) PRECACHE_SOUND_ENT(this, path)

EXPORT void PRECACHE_DETAIL_TEXTURES();

// called automatically for custom weapons during registration
EXPORT void PRECACHE_HUD_FILES(const char* path);

EXPORT void MESSAGE_BEGIN(int msg_dest, int msg_type, const float* pOrigin = NULL, edict_t* ed = NULL);
EXPORT void MESSAGE_END();
EXPORT void WRITE_BYTE(int iValue);
EXPORT void WRITE_CHAR(int iValue);
EXPORT void WRITE_SHORT(int iValue);
EXPORT void WRITE_LONG(int iValue);
EXPORT void WRITE_ANGLE(float fValue);
EXPORT void WRITE_COORD(float iValue);
EXPORT void WRITE_STRING(const char* sValue);
EXPORT void WRITE_ENTITY(int iValue);
#endif