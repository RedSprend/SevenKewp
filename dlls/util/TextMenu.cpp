#include "extdll.h"
#include "util.h"
#include "TextMenu.h"
#include "user_messages.h"
#include "CTriggerVote.h"
#include <cstdint>
#include "CBasePlayer.h"

class CTriggerVote;

TextMenu g_textMenus[MAX_PLAYERS];
int g_exitOptionNum = 5;

// listen for any other functions/plugins opening menus, so that TextMenu knows if it's the active menu
void TextMenuMessageBeginHook(int msg_dest, int msg_type, const float* pOrigin, edict_t* ed) {
	if (msg_type != gmsgShowMenu) {
		return;
	}

	for (int i = 0; i < MAX_PLAYERS; i++) {
		g_textMenus[i].handleMenuMessage(msg_dest, ed);
	}
}

// handle player selections
bool TextMenuClientCommandHook(CBasePlayer* pPlayer) {
	if (toLowerCase(CMD_ARGV(0)) == "menuselect") {
		int selection = atoi(CMD_ARGV(1)) - 1;
		if (selection < 0 || selection >= MAX_MENU_OPTIONS) {
			return true;
		}

		for (int i = 0; i < MAX_PLAYERS; i++) {
			g_textMenus[i].handleMenuselectCmd(pPlayer, selection);
		}

		return true;
	}

	return false;
}

TextMenu* TextMenu::init(CBasePlayer* player, TextMenuCallback callback) {
	int idx = player ? player->entindex() % MAX_PLAYERS : 0;
	TextMenu* menu = &g_textMenus[idx];
	menu->initAnon(callback);
	return menu;
}

TextMenu* TextMenu::init(CBasePlayer* player, EntityTextMenuCallback callback, CBaseEntity* ent) {
	int idx = player ? player->entindex() % MAX_PLAYERS : 0;
	TextMenu* menu = &g_textMenus[idx];
	menu->initEnt(callback, ent);
	return menu;
}

TextMenu::TextMenu() {
	initAnon(NULL);
}

void TextMenu::initCommon() {
	viewers = 0;
	numOptions = 0;

	for (int i = 0; i < MAX_MENU_OPTIONS - 1; i++) {
		options[i].displayText = "";
		options[i].data = "";
	}
	TextMenuItem exitItem = { "Exit", "exit" };
	options[MAX_MENU_OPTIONS - 1] = exitItem;
}

void TextMenu::initAnon(TextMenuCallback newCallback) {
	initCommon();
	this->anonCallback = newCallback;
	this->entCallback = NULL;
	h_ent = NULL;
}

void TextMenu::initEnt(EntityTextMenuCallback newCallback, CBaseEntity* ent) {
	initCommon();
	this->entCallback = newCallback;
	this->anonCallback = NULL;
	h_ent = ent;
}

void TextMenu::handleMenuMessage(int msg_dest, edict_t* ed) {

	// Another text menu has been opened for one or more players, so this menu
	// is no longer visible and should not handle menuselect commands

	// If this message is in fact triggered by this object, then the viewer flags should be set
	// after this func finishes.

	if (!viewers) {
		return;
	}

	if ((msg_dest == MSG_ONE || msg_dest == MSG_ONE_UNRELIABLE) && ed) {
		//ALERT(at_console, "New menu opened for %s", STRING(ed->v.netname));
		viewers &= ~(PLRBIT(ed));
	}
	else if (msg_dest == MSG_ALL || msg_dest == MSG_ALL) {
		//ALERT(at_console, "New menu opened for all players");
		viewers = 0;
	}
	else {
		//ALERT(at_console, "Unhandled text menu message dest: %d", msg_dest);
	}
}

void TextMenu::handleMenuselectCmd(CBasePlayer* pPlayer, int selection) {
	if (!viewers || !pPlayer) {
		return;
	}

	int playerbit = PLRBIT(pPlayer->edict());

	if (viewers & playerbit) {
		if (selection == g_exitOptionNum-1) {
			// exit menu
			viewers &= ~playerbit;
		}
		else if (isPaginated() && selection == BACK_OPTION_IDX) {
			Open(lastDuration, lastPage - 1, pPlayer);
		}
		else if (isPaginated() && selection == MORE_OPTION_IDX) {
			Open(lastDuration, lastPage + 1, pPlayer);
		}
		else if (selection < numOptions && IsValidPlayer(pPlayer->edict())) {
			if (anonCallback)
				anonCallback(this, pPlayer, selection, options[lastPage*ITEMS_PER_PAGE + selection]);
			if (entCallback) {
				if (h_ent)
					(((CTriggerVote*)h_ent.GetEntity())->*entCallback)(this, pPlayer, selection, options[lastPage * ITEMS_PER_PAGE + selection]);
			}

			viewers &= ~playerbit;
		}
	}
	else {
		//ALERT(at_console, "%s is not viewing the '%s' menu", STRING(pEntity->v.netname), title.c_str());
	}
}

bool TextMenu::isPaginated() {
	return numOptions > MAX_ITEMS_NO_PAGES;
}

void TextMenu::SetTitle(std::string newTitle) {
	this->title = newTitle;
}

void TextMenu::AddItem(std::string displayText, std::string optionData) {
	if (numOptions >= MAX_MENU_OPTIONS) {
		ALERT(at_console, "Maximum menu options reached! Failed to add: %s\n", optionData.c_str());
		return;
	}

	TextMenuItem item = { displayText, optionData };
	options[numOptions] = item;

	numOptions++;
}

void TextMenu::Open(uint8_t duration, uint8_t page, CBasePlayer* player) {
	std::string menuText = title + "\n\n";

	uint16_t validSlots = (1 << (g_exitOptionNum-1)); // exit option always valid

	lastPage = page;
	lastDuration = duration;
	
	int limitPerPage = isPaginated() ? ITEMS_PER_PAGE : MAX_ITEMS_NO_PAGES;
	int itemOffset = page * ITEMS_PER_PAGE;
	int totalPages = (numOptions+(ITEMS_PER_PAGE-1)) / ITEMS_PER_PAGE;

	int addedOptions = 0;
	for (int i = itemOffset, k = 0; i < itemOffset+limitPerPage && i < numOptions; i++, k++) {
		menuText += std::to_string(k+1) + ": " + options[i].displayText + "\n";
		validSlots |= (1 << k);
		addedOptions++;
	}

	while (isPaginated() && addedOptions < ITEMS_PER_PAGE) {
		menuText += "\n";
		addedOptions++;
	}

	menuText += "\n";

	if (isPaginated()) {
		if (page > 0) {
			menuText += std::to_string(BACK_OPTION_IDX+1) + ": Back\n";
			validSlots |= (1 << (ITEMS_PER_PAGE));
		}
		else {
			menuText += "\n";
		}
		if (page < totalPages - 1) {
			menuText += std::to_string(MORE_OPTION_IDX+1) + ": More\n";
			validSlots |= (1 << (ITEMS_PER_PAGE + 1));
		}
		else {
			menuText += "\n";
		}
	}

	menuText += std::to_string(g_exitOptionNum % 10) + ": Exit";

	if (player && IsValidPlayer(player->edict())) {
		MESSAGE_BEGIN(MSG_ONE, gmsgShowMenu, NULL, player->edict());
		WRITE_SHORT(validSlots);
		WRITE_CHAR(duration);
		WRITE_BYTE(FALSE); // "need more" (?)
		WRITE_STRING(menuText.c_str());
		MESSAGE_END();

		viewers |= PLRBIT(player->edict());
	}
	else {
		ALERT(at_console, "WARNING: pagination is broken for menus that don't have a destination player\n");
		MESSAGE_BEGIN(MSG_ALL, gmsgShowMenu);
		WRITE_SHORT(validSlots);
		WRITE_CHAR(duration);
		WRITE_BYTE(FALSE); // "need more" (?)
		WRITE_STRING(menuText.c_str());
		MESSAGE_END();

		viewers = 0xffffffff;
	}
}