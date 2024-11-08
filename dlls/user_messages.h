#pragma once

EXPORT extern int giPrecacheGrunt;
EXPORT extern int gmsgShake;
EXPORT extern int gmsgFade;
EXPORT extern int gmsgSelAmmo;
EXPORT extern int gmsgFlashlight;
EXPORT extern int gmsgFlashBattery;
EXPORT extern int gmsgResetHUD;
EXPORT extern int gmsgInitHUD;
EXPORT extern int gmsgShowGameTitle;
EXPORT extern int gmsgCurWeapon;
EXPORT extern int gmsgHealth;
EXPORT extern int gmsgDamage;
EXPORT extern int gmsgBattery;
EXPORT extern int gmsgTrain;
EXPORT extern int gmsgLogo;
EXPORT extern int gmsgWeaponList;
EXPORT extern int gmsgAmmoX;
EXPORT extern int gmsgHudText;
EXPORT extern int gmsgDeathMsg;
EXPORT extern int gmsgScoreInfo;
EXPORT extern int gmsgTeamInfo;
EXPORT extern int gmsgTeamScore;
EXPORT extern int gmsgGameMode;
EXPORT extern int gmsgMOTD;
EXPORT extern int gmsgServerName;
EXPORT extern int gmsgAmmoPickup;
EXPORT extern int gmsgWeapPickup;
EXPORT extern int gmsgItemPickup;
EXPORT extern int gmsgHideWeapon;
EXPORT extern int gmsgSetCurWeap;
EXPORT extern int gmsgSayText;
EXPORT extern int gmsgTextMsg;
EXPORT extern int gmsgSetFOV;
EXPORT extern int gmsgShowMenu;
EXPORT extern int gmsgGeigerRange;
EXPORT extern int gmsgTeamNames;

EXPORT extern int gmsgStatusText;
EXPORT extern int gmsgStatusValue;

EXPORT extern int gmsgToxicCloud;

// Note: also update msgTypeStr() in util.cpp when adding new messages

struct UserMessage {
	const char* name;
	int id;
	int size;
};

extern std::vector<UserMessage> g_userMessages;

#ifdef CLIENT_DLL
#define REG_USER_MSG				(*g_engfuncs.pfnRegUserMsg)
#else
EXPORT int REG_USER_MSG(const char* name, int size);
#endif

// Find a usermsg, registered by the gamedll, with the corresponding
// msgname, and return remaining info about it (msgid, size). -metamod
EXPORT int GetUserMsgInfo(const char* msgname, int* size);

void LinkUserMessages(void);