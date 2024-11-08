/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
//=========================================================
// sound.cpp 
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "weapons.h"
#include "CBasePlayer.h"
#include "CTalkSquadMonster.h"
#include "gamerules.h"
#include "PluginManager.h"

#if !defined ( _WIN32 )
#include <ctype.h>
#endif


static char *memfgets( byte *pMemFile, int fileSize, int &filePos, char *pBuffer, int bufferSize );

// ==================== SENTENCE GROUPS, UTILITY FUNCTIONS  ======================================

#define CSENTENCE_LRU_MAX	32		// max number of elements per sentence group

// group of related sentences

typedef struct sentenceg
{
	char szgroupname[CBSENTENCENAME_MAX];
	int count;
	unsigned char rgblru[CSENTENCE_LRU_MAX];

} SENTENCEG;

#define CSENTENCEG_MAX 200					// max number of sentence groups
// globals

SENTENCEG rgsentenceg[CSENTENCEG_MAX];
int fSentencesInit = FALSE;

char gszallsentencenames[CVOXFILESENTENCEMAX][CBSENTENCENAME_MAX];
int gcallsentences = 0;

// randomize list of sentence name indices

void USENTENCEG_InitLRU(unsigned char *plru, int count)
{
	int i, j, k;
	unsigned char temp;
	
	if (!fSentencesInit)
		return;

	if (count > CSENTENCE_LRU_MAX)
		count = CSENTENCE_LRU_MAX;

	for (i = 0; i < count; i++)
		plru[i] = (unsigned char) i;

	// randomize array
	for (i = 0; i < (count * 4); i++)
	{
		j = RANDOM_LONG(0,count-1);
		k = RANDOM_LONG(0,count-1);
		temp = plru[j];
		plru[j] = plru[k];
		plru[k] = temp;
	}
}

// ignore lru. pick next sentence from sentence group. Go in order until we hit the last sentence, 
// then repeat list if freset is true.  If freset is false, then repeat last sentence.
// ipick is passed in as the requested sentence ordinal.
// ipick 'next' is returned.  
// return of -1 indicates an error.

int USENTENCEG_PickSequential(int isentenceg, char *szfound, int ipick, int freset, int bufsz)
{
	char *szgroupname;
	unsigned char count;
	char sznum[16];
	
	if (!fSentencesInit)
		return -1;

	if (isentenceg < 0)
		return -1;

	szgroupname = rgsentenceg[isentenceg].szgroupname;
	count = rgsentenceg[isentenceg].count;
	
	if (count == 0)
		return -1;

	if (ipick >= count)
		ipick = count-1;

	strcpy_safe(szfound, "!", bufsz);
	strcat_safe(szfound, szgroupname, bufsz);
	snprintf(sznum, 16, "%d", ipick);
	strcat_safe(szfound, sznum, bufsz);
	
	if (ipick >= count)
	{
		if (freset)
			// reset at end of list
			return 0;
		else
			return count;
	}

	return ipick + 1;
}



// pick a random sentence from rootname0 to rootnameX.
// picks from the rgsentenceg[isentenceg] least
// recently used, modifies lru array. returns the sentencename.
// note, lru must be seeded with 0-n randomized sentence numbers, with the
// rest of the lru filled with -1. The first integer in the lru is
// actually the size of the list.  Returns ipick, the ordinal
// of the picked sentence within the group.

int USENTENCEG_Pick(int isentenceg, char *szfound, int bufsz)
{
	char *szgroupname;
	unsigned char *plru;
	unsigned char i;
	unsigned char count;
	char sznum[8];
	unsigned char ipick = 0;
	int ffound = FALSE;
	
	if (!fSentencesInit)
		return -1;

	if (isentenceg < 0)
		return -1;

	szgroupname = rgsentenceg[isentenceg].szgroupname;
	count = rgsentenceg[isentenceg].count;
	plru = rgsentenceg[isentenceg].rgblru;

	while (!ffound)
	{
		for (i = 0; i < count; i++)
			if (plru[i] != 0xFF)
			{
				ipick = plru[i];
				plru[i] = 0xFF;
				ffound = TRUE;
				break;
			}

		if (!ffound)
			USENTENCEG_InitLRU(plru, count);
		else
		{
			strcpy_safe(szfound, "!", bufsz);
			strcat_safe(szfound, szgroupname, bufsz);
			snprintf(sznum, 8, "%d", ipick);
			strcat_safe(szfound, sznum, bufsz);
			return ipick;
		}
	}
	return -1;
}

// ===================== SENTENCE GROUPS, MAIN ROUTINES ========================

// Given sentence group rootname (name without number suffix),
// get sentence group index (isentenceg). Returns -1 if no such name.

int SENTENCEG_GetIndex(const char *szgroupname)
{
	int i;

	if (!fSentencesInit || !szgroupname)
		return -1;

	// search rgsentenceg for match on szgroupname

	i = 0;
	while (rgsentenceg[i].count)
	{
		if (!strcmp(szgroupname, rgsentenceg[i].szgroupname))
			return i;
	i++;
	}

	return -1;
}

// given sentence group index, play random sentence for given entity.
// returns ipick - which sentence was picked to 
// play from the group. Ipick is only needed if you plan on stopping
// the sound before playback is done (see SENTENCEG_Stop).

int SENTENCEG_PlayRndI(edict_t *entity, int isentenceg, 
					  float volume, float attenuation, int flags, int pitch)
{
	char name[64];
	int ipick;

	if (!fSentencesInit)
		return -1;

	name[0] = 0;

	ipick = USENTENCEG_Pick(isentenceg, name, 64);
	if (ipick > 0 && name[0])
		EMIT_SOUND_DYN(entity, CHAN_VOICE, name, volume, attenuation, flags, pitch);
	return ipick;
}

// same as above, but takes sentence group name instead of index

int SENTENCEG_PlayRndSz(edict_t *entity, const char *szgroupname, 
					  float volume, float attenuation, int flags, int pitch)
{
	char name[64];
	int ipick;
	int isentenceg;

	if (!fSentencesInit)
		return -1;

	name[0] = 0;

	isentenceg = SENTENCEG_GetIndex(szgroupname);
	if (isentenceg < 0)
	{
		ALERT( at_console, "No such sentence group %s\n", szgroupname );
		return -1;
	}

	ipick = USENTENCEG_Pick(isentenceg, name, 64);
	if (ipick >= 0 && name[0])
		EMIT_SOUND_DYN(entity, CHAN_VOICE, name, volume, attenuation, flags, pitch);

	return ipick;
}

// play sentences in sequential order from sentence group.  Reset after last sentence.

int SENTENCEG_PlaySequentialSz(edict_t *entity, const char *szgroupname, 
					  float volume, float attenuation, int flags, int pitch, int ipick, int freset)
{
	static char name[128];
	int ipicknext;
	int isentenceg;

	if (!fSentencesInit)
		return -1;

	name[0] = 0;

	isentenceg = SENTENCEG_GetIndex(szgroupname);
	if (isentenceg < 0)
		return -1;

	ipicknext = USENTENCEG_PickSequential(isentenceg, name, ipick, freset, 128);
	if (ipicknext >= 0 && name[0])
		EMIT_SOUND_DYN(entity, CHAN_VOICE, name, volume, attenuation, flags, pitch);
	return ipicknext;
}


// for this entity, for the given sentence within the sentence group, stop
// the sentence.

void SENTENCEG_Stop(edict_t *entity, int isentenceg, int ipick)
{
	char buffer[64];
	char sznum[12];
	
	if (!fSentencesInit)
		return;

	if (isentenceg < 0 || ipick < 0)
		return;
	
	strcpy_safe(buffer, "!", 64);
	strcat_safe(buffer, rgsentenceg[isentenceg].szgroupname, 64);
	snprintf(sznum, 12, "%d", ipick);
	strcat_safe(buffer, sznum, 64);

	STOP_SOUND(entity, CHAN_VOICE, buffer);
}

// open sentences.txt, scan for groups, build rgsentenceg
// Should be called from world spawn, only works on the
// first call and is ignored subsequently.

void SENTENCEG_Init()
{
	char buffer[512];
	char szgroup[64];
	int i, j;
	int isentencegs;

	if (fSentencesInit)
		return;

	memset(gszallsentencenames, 0, CVOXFILESENTENCEMAX * CBSENTENCENAME_MAX);
	gcallsentences = 0;

	memset(rgsentenceg, 0, CSENTENCEG_MAX * sizeof(SENTENCEG));
	memset(buffer, 0, 512);
	memset(szgroup, 0, 64);
	isentencegs = -1;

	
	int filePos = 0, fileSize;
	byte *pMemFile = g_engfuncs.pfnLoadFileForMe( "sound/sentences.txt", &fileSize );
	if ( !pMemFile )
		return;

	// for each line in the file...
	while ( memfgets(pMemFile, fileSize, filePos, buffer, 511) != NULL )
	{
		// skip whitespace
		i = 0;
		while(buffer[i] && buffer[i] == ' ')
			i++;
		
		if (!buffer[i])
			continue;

		if (buffer[i] == '/' || !isalpha(buffer[i]))
			continue;

		// get sentence name
		j = i;
		while (buffer[j] && buffer[j] != ' ')
			j++;

		if (!buffer[j])
			continue;

		if (gcallsentences > CVOXFILESENTENCEMAX)
		{
			ALERT (at_error, "Too many sentences in sentences.txt!\n");
			break;
		}

		// null-terminate name and save in sentences array
		buffer[j] = 0;
		const char *pString = buffer + i;

		if ( strlen( pString ) >= CBSENTENCENAME_MAX )
			ALERT( at_warning, "Sentence %s longer than %d letters\n", pString, CBSENTENCENAME_MAX-1 );

		strcpy_safe( gszallsentencenames[gcallsentences++], pString, 16 );

		j--;
		if (j <= i)
			continue;
		if (!isdigit(buffer[j]))
			continue;

		// cut out suffix numbers
		while (j > i && isdigit(buffer[j]))
			j--;

		if (j <= i)
			continue;

		buffer[j+1] = 0;
		
		// if new name doesn't match previous group name, 
		// make a new group.

		if (strncmp(szgroup, &(buffer[i]), 64))
		{
			// name doesn't match with prev name,
			// copy name into group, init count to 1
			isentencegs++;
			if (isentencegs >= CSENTENCEG_MAX)
			{
				ALERT (at_error, "Too many sentence groups in sentences.txt!\n");
				break;
			}

			strcpy_safe(rgsentenceg[isentencegs].szgroupname, &(buffer[i]), 16);
			rgsentenceg[isentencegs].count = 1;

			strcpy_safe(szgroup, &(buffer[i]), 64);

			continue;
		}
		else
		{
			//name matches with previous, increment group count
			if (isentencegs >= 0)
				rgsentenceg[isentencegs].count++;
		}
	}

	g_engfuncs.pfnFreeFile( pMemFile );
	
	fSentencesInit = TRUE;

	// init lru lists

	i = 0;

	while (rgsentenceg[i].count && i < CSENTENCEG_MAX)
	{
		USENTENCEG_InitLRU(&(rgsentenceg[i].rgblru[0]), rgsentenceg[i].count);
		i++;
	}

}

// convert sentence (sample) name to !sentencenum, return !sentencenum

int SENTENCEG_Lookup(const char *sample, char *sentencenum, int bufsz)
{
	char sznum[32];

	int i;
	// this is a sentence name; lookup sentence number
	// and give to engine as string.
	for (i = 0; i < gcallsentences; i++)
		if (!stricmp(gszallsentencenames[i], sample+1))
		{
			if (sentencenum)
			{
				strcpy_safe(sentencenum, "!", bufsz);
				snprintf(sznum, 32, "%d", i);
				strcat_safe(sentencenum, sznum, bufsz);
			}
			return i;
		}
	// sentence name not found!
	return -1;
}

mstream* BuildStartSoundMessage(edict_t* ent, int channel, const char* sample, float fvolume, float attenuation,
	int fFlags, int pitch, const float* origin) {
	int sound_num;
	int field_mask;

	field_mask = fFlags;

	if (*sample == '!')
	{
		field_mask |= SND_SENTENCE;
		sound_num = atoi(sample + 1);
		if (sound_num >= CVOXFILESENTENCEMAX)
		{
			ALERT(at_console, "%s: invalid sentence number: %s\n", __func__, sample + 1);
			return NULL;
		}
	}
	else if (*sample == '#')
	{
		field_mask |= SND_SENTENCE;
		sound_num = atoi(sample + 1) + CVOXFILESENTENCEMAX;
	}
	else
	{
		sound_num = PRECACHE_SOUND_ENT(NULL, sample); // TODO: abort if not precached
	}

	int ient = ENTINDEX(ent);
	int volume = clampf(fvolume, 0, 1.0f) * 255;

	if (volume != DEFAULT_SOUND_PACKET_VOLUME)
		field_mask |= SND_FL_VOLUME;
	if (attenuation != DEFAULT_SOUND_PACKET_ATTENUATION)
		field_mask |= SND_FL_ATTENUATION;
	if (pitch != DEFAULT_SOUND_PACKET_PITCH)
		field_mask |= SND_FL_PITCH;
	if (sound_num > 255)
		field_mask |= SND_FL_LARGE_INDEX;

	const int maxStartSoundMessageSz = 16;
	static uint8_t msgbuffer[maxStartSoundMessageSz];
	static mstream bitbuffer((char*)msgbuffer, 16);

	memset(msgbuffer, 0, maxStartSoundMessageSz);
	bitbuffer.seek(0);

	bitbuffer.writeBits(field_mask, 9);
	if (field_mask & SND_FL_VOLUME)
		bitbuffer.writeBits(volume, 8);
	if (field_mask & SND_FL_ATTENUATION)
		bitbuffer.writeBits((uint32)(attenuation * 64.0f), 8);
	bitbuffer.writeBits(channel, 3);
	bitbuffer.writeBits(ient, MAX_EDICT_BITS);
	bitbuffer.writeBits(sound_num, (field_mask & SND_FL_LARGE_INDEX) ? 16 : 8);
	bitbuffer.writeBitVec3Coord(origin);
	if (field_mask & SND_FL_PITCH)
		bitbuffer.writeBits(pitch, 8);

	if (bitbuffer.eom()) {
		ALERT(at_error, "StartSound bit buffer overflow\n");
		return NULL;
	}

	return &bitbuffer;
}


void StartSound(edict_t* entidx, int channel, const char* sample, float fvolume, float attenuation,
	int fFlags, int pitch, const float* origin, uint32_t messageTargets, BOOL reliable)
{
	CALL_HOOKS_VOID(pfnEmitSound, entidx, channel, sample, fvolume, attenuation, fFlags | SND_FL_MOD, pitch, origin, messageTargets, reliable);

	mstream* bitbuffer = BuildStartSoundMessage(entidx, channel, sample, fvolume, attenuation, fFlags, pitch, origin);

	if (!bitbuffer) {
		return;
	}

	int msgSz = bitbuffer->tell() + 1;
	//bool anyMessagesWritten = false;

	for (int i = 1; i <= gpGlobals->maxClients; i++) {
		edict_t* plent = INDEXENT(i);
		uint32_t playerBit = PLRBIT(plent);

		if (!(messageTargets & playerBit) || !IsValidPlayer(plent)) {
			continue;
		}

		// TODO: should only consider PAS (can hear reload sounds but only distant gunshots)
		MESSAGE_BEGIN(reliable ? MSG_ONE : MSG_ONE_UNRELIABLE, SVC_SOUND, NULL, plent);
		WRITE_BYTES((uint8_t*)bitbuffer->getBuffer(), msgSz);
		MESSAGE_END();
		//anyMessagesWritten = true;
	}

	/*
	if (anyMessagesWritten && mp_debugmsg.value > 1) {
		ALERT(at_logged, "[DEBUG] StartSound(%d, %d, \"%s\", 0x%X, 0x%X, 0x%X, %d, (0x%X 0x%X 0x%X), 0x%X)\n",
			(int)(entity ? ENTINDEX(entity) : 0), channel, sample, *(int*)&fvolume, *(int*)&attenuation, fFlags, pitch,
			*(int*)&origin[0], *(int*)&origin[1], *(int*)&origin[2], messageTargets);
	}
	*/
}

void StartSound(int eidx, int channel, const char* sample, float volume, float attenuation,
	int fFlags, int pitch, const float* origin, uint32_t messageTargets, BOOL reliable) {
	StartSound(INDEXENT(eidx), channel, sample, volume, attenuation, fFlags, pitch, origin,
		messageTargets, reliable);
}

void EMIT_SOUND_DYN(edict_t *entity, int channel, const char *sample, float volume, float attenuation,
						   int flags, int pitch)
{
	// prevent invalid values crashing the engine
	volume = clampf(volume, 0, 1.0f);
	attenuation = clampf(attenuation, 0, 4.0f);
	channel = clampi(channel, 0, 7);
	pitch = clampi(pitch, 0, 255);
	
	if (!sample || sample[0] == '\0') {
		return;
	}

	if (entity->v.flags & FL_MONSTER) {
		int eidx = ENTINDEX(entity);

		if (g_monsterSoundReplacements[eidx].find(sample) != g_monsterSoundReplacements[eidx].end()) {
			sample = g_monsterSoundReplacements[eidx][sample].c_str();
		}
		else if (g_soundReplacements.find(sample) != g_soundReplacements.end()) {
			sample = g_soundReplacements[sample].c_str();
		}
	}
	else if (g_soundReplacements.find(sample) != g_soundReplacements.end()) {
		sample = g_soundReplacements[sample].c_str();
	}
	
	if (sample && *sample == '!')
	{
		char name[32];
		if (SENTENCEG_Lookup(sample, name, 32) >= 0)
				EMIT_SOUND_DYN2(entity, channel, name, volume, attenuation, flags, pitch);
		else
			ALERT( at_aiconsole, "Unable to find %s in sentences.txt\n", sample );
	}
	else {
		CBaseEntity* bent = CBaseEntity::Instance(entity);
		if (!bent) {
			return;
		}

		// the static channel is special because it ignores the PAS. However, clients won't hear
		// the sound if they can't see the entity, even if it's nearby. This block will emit
		// positional sounds for those cases. This has the drawback of sound not following the
		// entity, but is better than hearing nothing at all.
		bool isStatic = channel == CHAN_STATIC;

		// this sound will crash clients if sent through the normal engine function.
		bool isUnsafeIdx = ENTINDEX(entity) >= MAX_LEGACY_CLIENT_ENTS;

		if (isStatic || isUnsafeIdx) {
			Vector ori = entity->v.origin + (entity->v.maxs + entity->v.mins) * 0.5f;

			for (int i = 1; i <= gpGlobals->maxClients; i++) {
				edict_t* plr = INDEXENT(i);

				if (!IsValidPlayer(plr)) {
					continue;
				}

				if (isUnsafeIdx && !UTIL_isSafeEntIndex(plr, ENTINDEX(entity), "play sound")) {
					continue;
				}

				if (isStatic && !bent->InPAS(plr)) {
					ambientsound_msg(entity, ori, sample, volume, attenuation, flags, pitch, MSG_ONE, plr);
				}
				else {
					// play the sound normally for players that can see the ent
					StartSound(entity, channel, sample, volume, attenuation, flags, pitch, ori, PLRBIT(plr), true);
				}
			}
		}
		else {
			// the engine function is preferred because it uses the PAS
			EMIT_SOUND_DYN2(entity, channel, sample, volume, attenuation, flags, pitch);
		}
	}
}

void PLAY_DISTANT_SOUND(edict_t* emitter, int soundType) {
	CBaseEntity* baseEmitter = (CBaseEntity*)GET_PRIVATE(emitter);
	if (!baseEmitter) {
		return;
	}

	const char* sample = "";
	float volume = 1.0f;
	float minRange = 768.0f;

	switch (soundType) {
	case DISTANT_9MM:
		sample = "weapons/distant/crack_9mm.wav";
		volume = 0.3f;
		break;
	case DISTANT_357:
		sample = "weapons/distant/crack_357.wav";
		break;
	case DISTANT_556:
		sample = "weapons/distant/crack_556.wav";
		break;
	case DISTANT_BOOM: {
		minRange = 2048.0f;

		switch (RANDOM_LONG(0, 1))
		{
		case 0:
			sample = "weapons/distant/explode3.wav";
			break;
		case 1:
			sample = "weapons/distant/explode5.wav";
			break;
		//case 2:
		//	sample = "weapons/distant/explode4.wav";
		//	break;
		}
		break;
	}
	default:
		ALERT(at_error, "Invalid distant sound type %d\n", soundType);
		return;
	}	
	
	uint32_t pbits = 0;
	for (int i = 1; i <= gpGlobals->maxClients; i++) {
		edict_t* ent = INDEXENT(i);
		CBasePlayer* plr = (CBasePlayer*)GET_PRIVATE(ent);
		uint32_t pbit = PLRBIT(ent);

		if (ent == emitter || !plr) {
			continue;
		}

		// if listener is in the audible set and too close, don't play the sound.
		// otherwise, the player may be close, but on the other side of a wall, so they should hear the sound
		if (baseEmitter->InPAS(ent) && (ent->v.origin - emitter->v.origin).Length() < minRange) {
			continue;
		}

		pbits |= pbit;
	}

	if (pbits) {
		// TODO: dynamic attenuation
		float attn = 0.1f;
		
		// randomize pitch per entity, so you get a better idea of how many players/npcs are shooting
		int pitch = 95 + ((ENTINDEX(emitter) * 7) % 11);

		StartSound(NULL, CHAN_STATIC, sample, volume, attn, 0, pitch, emitter->v.origin, pbits, false);
	}
}

// play a specific sentence over the HEV suit speaker - just pass player entity, and !sentencename

void EMIT_SOUND_SUIT(edict_t *entity, const char *sample)
{
	float fvol;
	int pitch = PITCH_NORM;

	fvol = CVAR_GET_FLOAT("suitvolume");
	if (RANDOM_LONG(0,1))
		pitch = RANDOM_LONG(0,6) + 98;

	if (fvol > 0.05)
		EMIT_SOUND_DYN(entity, CHAN_STATIC, sample, fvol, ATTN_NORM, 0, pitch);
}

// play a sentence, randomly selected from the passed in group id, over the HEV suit speaker

void EMIT_GROUPID_SUIT(edict_t *entity, int isentenceg)
{
	float fvol;
	int pitch = PITCH_NORM;

	fvol = CVAR_GET_FLOAT("suitvolume");
	if (RANDOM_LONG(0,1))
		pitch = RANDOM_LONG(0,6) + 98;

	if (fvol > 0.05)
		SENTENCEG_PlayRndI(entity, isentenceg, fvol, ATTN_NORM, 0, pitch);
}

// play a sentence, randomly selected from the passed in groupname

void EMIT_GROUPNAME_SUIT(edict_t *entity, const char *groupname)
{
	float fvol;
	int pitch = PITCH_NORM;

	fvol = CVAR_GET_FLOAT("suitvolume");
	if (RANDOM_LONG(0,1))
		pitch = RANDOM_LONG(0,6) + 98;

	if (fvol > 0.05)
		SENTENCEG_PlayRndSz(entity, groupname, fvol, ATTN_NORM, 0, pitch);
}

void UTIL_ShuffleSoundArray(const char** arr, size_t n) {
	if (n > 1)
	{
		size_t i;
		for (i = 0; i < n - 1; i++)
		{
			size_t j = RANDOM_LONG(i + 1, n - 1);
			const char* t = arr[j];
			arr[j] = arr[i];
			arr[i] = t;
		}
	}
}

// ===================== MATERIAL TYPE DETECTION, MAIN ROUTINES ========================
// 
// Used to detect the texture the player is standing on, map the
// texture name to a material type.  Play footstep sound based
// on material type.

int fTextureTypeInit = FALSE;

#define CTEXTURESMAX		512			// max number of textures loaded

int gcTextures = 0;
char grgszTextureName[CTEXTURESMAX][CBTEXTURENAMEMAX];	// texture names
char grgchTextureType[CTEXTURESMAX];						// parallel array of texture types

// open materials.txt,  get size, alloc space, 
// save in array.  Only works first time called, 
// ignored on subsequent calls.

static char *memfgets( byte *pMemFile, int fileSize, int &filePos, char *pBuffer, int bufferSize )
{
	// Bullet-proofing
	if ( !pMemFile || !pBuffer )
		return NULL;

	if ( filePos >= fileSize )
		return NULL;

	int i = filePos;
	int last = fileSize;

	// fgets always NULL terminates, so only read bufferSize-1 characters
	if ( last - filePos > (bufferSize-1) )
		last = filePos + (bufferSize-1);

	int stop = 0;

	// Stop at the next newline (inclusive) or end of buffer
	while ( i < last && !stop )
	{
		if ( pMemFile[i] == '\n' )
			stop = 1;
		i++;
	}


	// If we actually advanced the pointer, copy it over
	if ( i != filePos )
	{
		// We read in size bytes
		int size = i - filePos;
		// copy it out
		memcpy( pBuffer, pMemFile + filePos, sizeof(byte)*size );
		
		// If the buffer isn't full, terminate (this is always true)
		if ( size < bufferSize )
			pBuffer[size] = 0;

		// Update file pointer
		filePos = i;
		return pBuffer;
	}

	// No data read, bail
	return NULL;
}


void TEXTURETYPE_Init()
{
	char buffer[512];
	int i, j;
	byte *pMemFile;
	int fileSize, filePos = 0;

	if (fTextureTypeInit)
		return;

	memset(&(grgszTextureName[0][0]), 0, CTEXTURESMAX * CBTEXTURENAMEMAX);
	memset(grgchTextureType, 0, CTEXTURESMAX);

	gcTextures = 0;
	memset(buffer, 0, 512);

	pMemFile = g_engfuncs.pfnLoadFileForMe( "sound/materials.txt", &fileSize );
	if ( !pMemFile )
		return;

	// for each line in the file...
	while (memfgets(pMemFile, fileSize, filePos, buffer, 511) != NULL && (gcTextures < CTEXTURESMAX))
	{
		// skip whitespace
		i = 0;
		while(buffer[i] && isspace(buffer[i]))
			i++;
		
		if (!buffer[i])
			continue;

		// skip comment lines
		if (buffer[i] == '/' || !isalpha(buffer[i]))
			continue;

		// get texture type
		grgchTextureType[gcTextures] = toupper(buffer[i++]);

		// skip whitespace
		while(buffer[i] && isspace(buffer[i]))
			i++;
		
		if (!buffer[i])
			continue;

		// get sentence name
		j = i;
		while (buffer[j] && !isspace(buffer[j]))
			j++;

		if (!buffer[j])
			continue;

		// null-terminate name and save in sentences array
		j = V_min (j, CBTEXTURENAMEMAX-1+i);
		buffer[j] = 0;
		strcpy_safe(&(grgszTextureName[gcTextures++][0]), &(buffer[i]), 13);
	}

	g_engfuncs.pfnFreeFile( pMemFile );
	
	fTextureTypeInit = TRUE;
}

// given texture name, find texture type
// if not found, return type 'concrete'

// NOTE: this routine should ONLY be called if the 
// current texture under the player changes!

char TEXTURETYPE_Find(char *name)
{
	// CONSIDER: pre-sort texture names and perform faster binary search here

	for (int i = 0; i < gcTextures; i++)
	{
		if (!strnicmp(name, &(grgszTextureName[i][0]), CBTEXTURENAMEMAX-1))
			return (grgchTextureType[i]);
	}

	return CHAR_TEX_CONCRETE;
}

// play a strike sound based on the texture that was hit by the attack traceline.  VecSrc/VecEnd are the
// original traceline endpoints used by the attacker, iBulletType is the type of bullet that hit the texture.
// returns volume of strike instrument (crowbar) to play

float TEXTURETYPE_PlaySound(TraceResult *ptr,  Vector vecSrc, Vector vecEnd, int iBulletType)
{
// hit the world, try to play sound based on texture material type
	
	char chTextureType;
	float fvol;
	float fvolbar;
	char szbuffer[64];
	const char *pTextureName;
	float rgfl1[3];
	float rgfl2[3];
	const char *rgsz[4];
	int cnt;
	float fattn = ATTN_NORM;

	if ( !g_pGameRules->PlayTextureSounds() )
		return 0.0;

	CBaseEntity *pEntity = CBaseEntity::Instance(ptr->pHit);

	chTextureType = 0;

	if (pEntity && pEntity->Classify() != CLASS_NONE && pEntity->Classify() != CLASS_MACHINE)
		// hit body
		chTextureType = CHAR_TEX_FLESH;
	else
	{
		// hit world

		// find texture under strike, get material type

		// copy trace vector into array for trace_texture

		vecSrc.CopyToArray(rgfl1);
		vecEnd.CopyToArray(rgfl2);

		// get texture from entity or world (world is ent(0))
		if (pEntity)
			pTextureName = TRACE_TEXTURE( ENT(pEntity->pev), rgfl1, rgfl2 );
		else
			pTextureName = TRACE_TEXTURE( ENT(0), rgfl1, rgfl2 );
			
		if ( pTextureName )
		{
			// strip leading '-0' or '+0~' or '{' or '!'
			if (*pTextureName == '-' || *pTextureName == '+')
				pTextureName += 2;

			if (*pTextureName == '{' || *pTextureName == '!' || *pTextureName == '~' || *pTextureName == ' ')
				pTextureName++;
			// '}}'
			strcpy_safe(szbuffer, pTextureName, 64);
			szbuffer[CBTEXTURENAMEMAX - 1] = 0;
				
			// ALERT ( at_console, "texture hit: %s\n", szbuffer);

			// get texture type
			chTextureType = TEXTURETYPE_Find(szbuffer);	
		}
	}

	switch (chTextureType)
	{
	default:
	case CHAR_TEX_CONCRETE: fvol = 0.9;	fvolbar = 0.6;
		rgsz[0] = "player/pl_step1.wav";
		rgsz[1] = "player/pl_step2.wav";
		cnt = 2;
		break;
	case CHAR_TEX_METAL: fvol = 0.9; fvolbar = 0.3;
		rgsz[0] = "player/pl_metal1.wav";
		rgsz[1] = "player/pl_metal2.wav";
		cnt = 2;
		break;
	case CHAR_TEX_DIRT:	fvol = 0.9; fvolbar = 0.1;
		rgsz[0] = "player/pl_dirt1.wav";
		rgsz[1] = "player/pl_dirt2.wav";
		rgsz[2] = "player/pl_dirt3.wav";
		cnt = 3;
		break;
	case CHAR_TEX_VENT:	fvol = 0.5; fvolbar = 0.3;
		rgsz[0] = "player/pl_duct1.wav";
		rgsz[1] = "player/pl_duct1.wav";
		cnt = 2;
		break;
	case CHAR_TEX_GRATE: fvol = 0.9; fvolbar = 0.5;
		rgsz[0] = "player/pl_grate1.wav";
		rgsz[1] = "player/pl_grate4.wav";
		cnt = 2;
		break;
	case CHAR_TEX_TILE:	fvol = 0.8; fvolbar = 0.2;
		rgsz[0] = "player/pl_tile1.wav";
		rgsz[1] = "player/pl_tile3.wav";
		rgsz[2] = "player/pl_tile2.wav";
		rgsz[3] = "player/pl_tile4.wav";
		cnt = 4;
		break;
	case CHAR_TEX_SLOSH: fvol = 0.9; fvolbar = 0.0;
		rgsz[0] = "player/pl_slosh1.wav";
		rgsz[1] = "player/pl_slosh3.wav";
		rgsz[2] = "player/pl_slosh2.wav";
		rgsz[3] = "player/pl_slosh4.wav";
		cnt = 4;
		break;
	case CHAR_TEX_WOOD: fvol = 0.9; fvolbar = 0.2;
		rgsz[0] = "debris/wood1.wav";
		rgsz[1] = "debris/wood2.wav";
		rgsz[2] = "debris/wood3.wav";
		cnt = 3;
		break;
	case CHAR_TEX_GLASS:
	case CHAR_TEX_COMPUTER:
		fvol = 0.8; fvolbar = 0.2;
		rgsz[0] = "debris/glass1.wav";
		rgsz[1] = "debris/glass2.wav";
		rgsz[2] = "debris/glass3.wav";
		cnt = 3;
		break;
	case CHAR_TEX_FLESH:
		if (iBulletType == BULLET_PLAYER_CROWBAR)
			return 0.0; // crowbar already makes this sound
		fvol = 1.0;	fvolbar = 0.2;
		rgsz[0] = "weapons/bullet_hit1.wav";
		rgsz[1] = "weapons/bullet_hit2.wav";
		fattn = 1.0;
		cnt = 2;
		break;
	}

	// did we hit a breakable?

	if (pEntity && FClassnameIs(pEntity->pev, "func_breakable"))
	{
		// drop volumes, the object will already play a damaged sound
		fvol /= 1.5;
		fvolbar /= 2.0;	
	}
	else if (chTextureType == CHAR_TEX_COMPUTER)
	{
		// play random spark if computer

		if ( ptr->flFraction != 1.0 && RANDOM_LONG(0,1))
		{
			UTIL_Sparks( ptr->vecEndPos );

			float flVolume = RANDOM_FLOAT ( 0.7 , 1.0 );//random volume range
			switch ( RANDOM_LONG(0,1) )
			{
				case 0: UTIL_EmitAmbientSound(ENT(0), ptr->vecEndPos, "buttons/spark5.wav", flVolume, ATTN_NORM, 0, 100); break;
				case 1: UTIL_EmitAmbientSound(ENT(0), ptr->vecEndPos, "buttons/spark6.wav", flVolume, ATTN_NORM, 0, 100); break;
				// case 0: EMIT_SOUND(ENT(pev), CHAN_VOICE, "buttons/spark5.wav", flVolume, ATTN_NORM);	break;
				// case 1: EMIT_SOUND(ENT(pev), CHAN_VOICE, "buttons/spark6.wav", flVolume, ATTN_NORM);	break;
			}
		}
	}

	// play material hit sound
	UTIL_EmitAmbientSound(ENT(0), ptr->vecEndPos, rgsz[RANDOM_LONG(0,cnt-1)], fvol, fattn, 0, 96 + RANDOM_LONG(0,0xf));
	//EMIT_SOUND_DYN( ENT(m_pPlayer->pev), CHAN_WEAPON, rgsz[RANDOM_LONG(0,cnt-1)], fvol, ATTN_NORM, 0, 96 + RANDOM_LONG(0,0xf));
			
	return fvolbar;
}

// ===================================================================================
//
// Speaker class. Used for announcements per level, for door lock/unlock spoken voice. 
//
