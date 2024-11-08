#include "extdll.h"
#include "util.h"
#include "PluginManager.h"
#include "cbase.h"
#include <fstream>
#include "Scheduler.h"

PluginManager g_pluginManager;

#define MAX_PLUGIN_CVARS 256
#define MAX_PLUGIN_COMMANDS 256

struct ExternalCvar {
	int pluginId;
	std::string name;
	cvar_t cvar;
};

struct ExternalCommand {
	int pluginId;
	char name[64];
	void (*function)(void);
};

ExternalCvar g_plugin_cvars[MAX_PLUGIN_CVARS];
int g_plugin_cvar_count = 0;

ExternalCommand g_plugin_commands[MAX_PLUGIN_COMMANDS];
int g_plugin_command_count = 0;

int g_plugin_id = 0;

#ifdef _WIN32
#include "windows.h"
#define LOADLIB_FUNC_NAME "LoadLibrary"
#define PLUGIN_EXT ".dll"
#else
#include <dlfcn.h>
#define PLUGIN_EXT ".so"
#define LOADLIB_FUNC_NAME "dlopen"
#define GetProcAddress dlsym
#define GetLastError dlerror
#define FreeLibrary dlclose
#define HMODULE void*
#endif

typedef int(*PLUGIN_INIT_FUNCTION)(void* plugin, int interfaceVersion);
typedef void(*PLUGIN_EXIT_FUNCTION)(void);

bool PluginManager::AddPlugin(const char* fpath, bool isMapPlugin) {
	std::string gamePath = fpath;

	if (isMapPlugin) {
		std::string fullPath = std::string("plugins/maps/") + fpath + PLUGIN_EXT;
		gamePath = getGameFilePath(fullPath.c_str());
	}

	if (gamePath.empty()) {
		ALERT(at_error, "Failed to find plugin '%s'\n", fpath);
		return false;
	}

	fpath = gamePath.c_str();

	Plugin plugin;
	memset(&plugin, 0, sizeof(Plugin));
	plugin.isMapPlugin = isMapPlugin;

	plugin.fpath = fpath;
	plugin.id = g_plugin_id++;

	if (LoadPlugin(plugin)) {
		plugins.push_back(plugin);
		return true;
	}
	
	return false;
}

bool PluginManager::LoadPlugin(Plugin& plugin) {
	g_engfuncs.pfnServerPrint(UTIL_VarArgs("Loading plugin '%s'\n", plugin.fpath.c_str()));

#ifdef _WIN32
	plugin.h_module = LoadLibraryA(plugin.fpath.c_str());
#else
	plugin.h_module = dlopen(plugin.fpath.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif

	if (!plugin.h_module) {
		ALERT(at_error, "Plugin load failed '%s' (" LOADLIB_FUNC_NAME " error code %d)\n",
			plugin.fpath.c_str(), GetLastError());
		return false;
	}

	PLUGIN_INIT_FUNCTION apiFunc =
		(PLUGIN_INIT_FUNCTION)GetProcAddress((HMODULE)plugin.h_module, "PluginInit");

	if (apiFunc) {
		int apiVersion = HLCOOP_API_VERSION;
		if (apiFunc(&plugin, apiVersion)) {
			// success
		}
		else {
			ALERT(at_error, "PluginInit call failed in plugin '%s'.\n", plugin.fpath.c_str());
			FreeLibrary((HMODULE)plugin.h_module);
			return false;
		}
	}
	else {
		ALERT(at_error, "PluginInit not found in plugin '%s'\n", plugin.fpath.c_str());
		FreeLibrary((HMODULE)plugin.h_module);
		return false;
	}

	return true;
}

void PluginManager::UnloadPlugin(const Plugin& plugin) {
	g_engfuncs.pfnServerPrint(UTIL_VarArgs("Removing plugin: '%s'\n", plugin.fpath.c_str()));

	PLUGIN_EXIT_FUNCTION apiFunc =
		(PLUGIN_EXIT_FUNCTION)GetProcAddress((HMODULE)plugin.h_module, "PluginExit");

	if (apiFunc) {
		apiFunc();
	}
	else {
		ALERT(at_console, "PluginExit not found in plugin '%s'\n", plugin.fpath.c_str());
	}

	g_Scheduler.RemoveTimers(plugin.name);

	FreeLibrary((HMODULE)plugin.h_module);
}

void PluginManager::RemovePlugin(const Plugin& plugin) {
	UnloadPlugin(plugin);

	for (int i = 0; i < (int)plugins.size(); i++) {
		if (&plugins[i] == &plugin) {
			plugins.erase(plugins.begin() + i);
			return;
		}
	}
}

void PluginManager::RemovePlugin(const char* name) {
	Plugin* plug = FindPlugin(name);

	if (plug) {
		RemovePlugin(*plug);
	}
}

void PluginManager::ReloadPlugin(const char* name) {
	Plugin* plug = FindPlugin(name);

	if (plug) {
		UnloadPlugin(*plug);
		LoadPlugin(*plug);
	}
}

std::string PluginManager::GetUpdatedPluginPath(Plugin& plugin) {
	std::string updatePath = plugin.fpath;
	updatePath = updatePath.substr(updatePath.find("/") + 1); // strip content folder
	return std::string(pluginupdatepath.string) + updatePath;
}

bool PluginManager::UpdatePlugin(const char* name) {
	Plugin* plug = FindPlugin(name);

	if (plug)
		return UpdatePlugin(*plug);

	return false;
}

bool PluginManager::UpdatePlugin(Plugin& plugin) {
	std::string updatePath = GetUpdatedPluginPath(plugin);
	std::string currentPath = plugin.fpath;

	if (!fileExists(updatePath.c_str())) {
		g_engfuncs.pfnServerPrint(UTIL_VarArgs("Update aborted. Updated plugin file not found \"%s\"\n", updatePath.c_str()));
		return false;
	}
	if (!fileExists(currentPath.c_str())) {
		g_engfuncs.pfnServerPrint(UTIL_VarArgs("Update aborted. Current plugin file not found \"%s\"\n", currentPath.c_str()));
		return false;
	}

	std::string backupPath = currentPath + ".backup";

	errno = 0;
	if (fileExists(backupPath.c_str()) && remove(backupPath.c_str())) {
		g_engfuncs.pfnServerPrint(UTIL_VarArgs("Update aborted. Failed to delete backup plugin file \"%s\" (error %d)\n", backupPath.c_str(), errno));
		return false;
	}
	if (rename(currentPath.c_str(), backupPath.c_str()) == -1) {
		g_engfuncs.pfnServerPrint(UTIL_VarArgs("Update aborted. Failed to create backup plugin file \"%s\" (error %d)\n", backupPath.c_str(), errno));
		return false;
	}

	UnloadPlugin(plugin);

	if (rename(updatePath.c_str(), currentPath.c_str()) == -1) {
		g_engfuncs.pfnServerPrint(UTIL_VarArgs("Update failed. File move from \"%s\" -> \"%s\" failed (error %d)\n",
			updatePath.c_str(), currentPath.c_str(), errno));

		if (rename(backupPath.c_str(), currentPath.c_str()) == -1) {
			g_engfuncs.pfnServerPrint(UTIL_VarArgs("Failed to restore backup plugin file \"%s\" (error %d)\n", backupPath.c_str(), errno));
		}
		else if (!LoadPlugin(plugin)) {
			g_engfuncs.pfnServerPrint("Failed to reload the plugin\n");
		}

		return false;
	}

	g_engfuncs.pfnServerPrint(UTIL_VarArgs("Moved \"%s\" -> \"%s\" \n",
		updatePath.c_str(), currentPath.c_str(), errno));

	if (!LoadPlugin(plugin)) {
		g_engfuncs.pfnServerPrint("Update failed. Restoring from backup...\n");

		if (rename(currentPath.c_str(), updatePath.c_str()) == -1) {
			g_engfuncs.pfnServerPrint(UTIL_VarArgs("Failed to move the plugin back to the update path \"%s\" (error %d)\n", updatePath.c_str(), errno));
			if (remove(currentPath.c_str())) {
				g_engfuncs.pfnServerPrint(UTIL_VarArgs("Failed to delete the updated plugin \"%s\" (error %d)\n", currentPath.c_str(), errno));
			}
		}

		if (rename(backupPath.c_str(), currentPath.c_str()) == -1) {
			g_engfuncs.pfnServerPrint(UTIL_VarArgs("Failed to restore backup plugin file \"%s\" (error %d)\n", backupPath.c_str(), errno));
		}
		else {
			if (!LoadPlugin(plugin)) {
				g_engfuncs.pfnServerPrint("Failed to load the backup plugin\n");
			}
		}

		return false;
	}
	
	return true;
}

bool PluginManager::UpdatePlugins() {
	int updatecount = 0;

	for (Plugin& plugin : plugins) {
		std::string updatePath = GetUpdatedPluginPath(plugin);

		if (!fileExists(updatePath.c_str())) {
			continue;
		}

		if (UpdatePlugin(plugin)) {
			updatecount++;
		}
	}

	if (updatecount) {
		g_engfuncs.pfnServerPrint(UTIL_VarArgs("Updated %d plugins\n", updatecount));
		return true;
	}
	else {
		return false;
	}
}

void PluginManager::RemovePlugins(bool mapPluginsOnly) {
	std::vector<Plugin> newPluginList;

	for (const Plugin& plugin : plugins) {
		if (mapPluginsOnly && !plugin.isMapPlugin) {
			newPluginList.push_back(plugin);
			continue;
		}

		UnloadPlugin(plugin);
	}

	plugins = newPluginList;
}

void PluginManager::UpdatePluginsFromList(bool forceUpdate) {
	static uint64_t lastEditTime = 0;
	const char* configPath = CVAR_GET_STRING("pluginlistfile");

	std::string path = getGameFilePath(configPath);

	if (path.empty()) {
		g_engfuncs.pfnServerPrint(UTIL_VarArgs("Missing plugin config: '%s'\n", configPath));
	}

	uint64_t editTime = getFileModifiedTime(path.c_str());

	if (!forceUpdate && lastEditTime == editTime) {
		return; // no changes made
	}

	std::ifstream infile(path);

	if (!infile.is_open()) {
		g_engfuncs.pfnServerPrint(UTIL_VarArgs("Failed to open plugin config: '%s'\n", path.c_str()));
		return;
	}

	lastEditTime = editTime;

	std::vector<std::string> relativePluginPaths;
	std::vector<std::string> pluginPaths;

	int lineNum = 0;
	std::string line;
	while (std::getline(infile, line))
	{
		lineNum++;

		int endPos = line.find("//");
		if (endPos != -1)
			line = trimSpaces(line.substr(0, endPos));

		if (line.empty()) {
			continue;
		}

		std::string pluginPath = normalize_path("plugins/server/" + line + PLUGIN_EXT);
		std::string gamePath = getGameFilePath(pluginPath.c_str());

		if (gamePath.empty()) {
			g_engfuncs.pfnServerPrint(UTIL_VarArgs("Error on line %d of '%s'. Plugin not found: '%s'\n",
				lineNum, path.c_str(), pluginPath.c_str()));
			continue;
		}

		relativePluginPaths.push_back(pluginPath);
		pluginPaths.push_back(gamePath);
	}

	// remove plugins that are no longer in the list
	std::vector<Plugin> newPlugins;
	for (Plugin& plugin : plugins) {
		plugin.sortId = 0; // reset id for sorting later

		if (plugin.isMapPlugin) {
			newPlugins.push_back(plugin);
			continue;
		}

		bool found = false;
		for (std::string& p : pluginPaths) {
			if (std::string(plugin.fpath) == p) {
				found = true;
				break;
			}
		}

		if (!found) {
			UnloadPlugin(plugin);
		}
		else {
			newPlugins.push_back(plugin);
		}
	}
	plugins = newPlugins;

	int numLoaded = 0;
	int numFailed = 0;

	// add new plugins
	for (std::string& p : pluginPaths) {
		bool found = false;
		for (const Plugin& plugin : plugins) {
			if (std::string(plugin.fpath) == p) {
				found = true;
				continue;
			}
		}

		if (!found) {
			if (AddPlugin(p.c_str(), false)) {
				numLoaded++;
			}
			else {
				numFailed++;
			}
		}
	}

	// assign ids and sort
	for (int i = 0; i < (int)pluginPaths.size(); i++) {
		std::string& p = pluginPaths[i];

		for (Plugin& plugin : plugins) {
			if (std::string(plugin.fpath) == p) {
				plugin.sortId = i+1;
			}
		}
	}

	std::sort(plugins.begin(), plugins.end(), [](const Plugin& a, const Plugin& b) {
		return a.isMapPlugin*1000 + a.sortId < b.isMapPlugin*1000 + b.sortId;
	});

	if (numFailed) {
		g_engfuncs.pfnServerPrint(UTIL_VarArgs("Loaded %d plugins. %d plugins failed to load.\n",
			numLoaded, numFailed));
	}
	else {
		g_engfuncs.pfnServerPrint(UTIL_VarArgs("Loaded %d plugins\n", numLoaded));
	}
}

void PluginManager::ReloadPlugins() {
	std::vector<Plugin> loadedMapPlugins;
	
	for (const Plugin& plugin : plugins) {
		if (plugin.isMapPlugin) {
			loadedMapPlugins.push_back(plugin);
			continue;
		}

		UnloadPlugin(plugin);
	}
	plugins = loadedMapPlugins;

	UpdatePluginsFromList(true);
}

void PluginManager::ListPlugins(edict_t* plr) {
	std::vector<std::string> lines;

	bool isAdmin = !plr || AdminLevel(plr) > ADMIN_NO;

	lines.push_back(UTIL_VarArgs("\n    %-20s %-8s %-44s\n", "Name", "Type", isAdmin ? "File path" : ""));
	lines.push_back("--------------------------------------------------------------------------------\n");

	for (int i = 0; i < (int)plugins.size(); i++) {
		const Plugin& plugin = plugins[i];

		const char* type = plugin.isMapPlugin ? "MAP" : "SERVER";
		
		if (isAdmin) {
			lines.push_back(UTIL_VarArgs("%2d) %-20s %-8s %-44s\n", i + 1, plugin.name, type, plugin.fpath.c_str()));
		}
		else {
			// player shouldn't know how the server files are laid out
			lines.push_back(UTIL_VarArgs("%2d) %-20s %-8s\n", i + 1, plugin.name, type));
		}
	}

	if (isAdmin) {
		lines.push_back(UTIL_VarArgs("\n Registered %d cvars, %d commands\n", 
			g_plugin_cvar_count, g_plugin_command_count));
	}

	lines.push_back("--------------------------------------------------------------------------------\n");

	for (std::string& line : lines) {
		if (plr) {
			UTIL_ClientPrint(plr, print_console, line.c_str());
		}
		else {
			g_engfuncs.pfnServerPrint(line.c_str());
		}
	}
}

Plugin* PluginManager::FindPlugin(int id) {
	for (Plugin& plugin : plugins) {
		if (plugin.id == id) {
			return &plugin;
		}
	}

	return NULL;
}

Plugin* PluginManager::FindPlugin(const char* name) {
	int bestIdx = -1;
	int numFound = 0;

	std::string lowerName = toLowerCase(name);

	for (int i = 0; i < (int)plugins.size(); i++) {
		if (toLowerCase(plugins[i].fpath).find(lowerName) != std::string::npos) {
			bestIdx = i;
			numFound++;
		}
	}

	if (numFound == 1) {
		return &plugins[bestIdx];
	}
	else if (numFound > 1) {
		g_engfuncs.pfnServerPrint(UTIL_VarArgs("Multiple plugins contain '%s'. Be more specific.\n", name));
	}
	else {
		g_engfuncs.pfnServerPrint(UTIL_VarArgs("No plugin found by name '%s'\n", name));
	}

	return NULL;
}

ENTITYINIT PluginManager::GetCustomEntityInitFunc(const char* pname) {
	for (const Plugin& plugin : plugins) {
		ENTITYINIT initFunc = (ENTITYINIT)GetProcAddress((HMODULE)plugin.h_module, pname);
		
		if (initFunc) {
			return initFunc;
		}
	}

	return NULL;
}

cvar_t* RegisterPluginCVar(void* pluginptr, const char* name, const char* strDefaultValue, int intDefaultValue, int flags) {
	if (!pluginptr) {
		return NULL;
	}

	Plugin* plugin = (Plugin*)pluginptr;

	if (g_plugin_cvar_count >= MAX_PLUGIN_CVARS) {
		ALERT(at_error, "Plugin cvar limit exceeded! Failed to register: %s\n", name);
		return NULL;
	}

	cvar_t* existing = CVAR_GET_POINTER(name);
	if (existing) {
		//g_engfuncs.pfnServerPrint(UTIL_VarArgs("Plugin cvar already registered: %s\n", name));

		// update the owner of the cvar
		for (int i = 0; i < MAX_PLUGIN_CVARS; i++) {
			if (existing == &g_plugin_cvars[i].cvar) {
				g_plugin_cvars[i].pluginId = plugin->id;
			}
		}

		return existing;
	}

	ExternalCvar& extvar = g_plugin_cvars[g_plugin_cvar_count];
	
	extvar.name = name;
	extvar.cvar.name = extvar.name.c_str();
	extvar.cvar.string = STRING(MAKE_STRING(strDefaultValue));
	extvar.cvar.flags = flags | FCVAR_EXTDLL;
	extvar.cvar.value = intDefaultValue;
	extvar.cvar.next = NULL;
	extvar.pluginId = plugin->id;

	g_plugin_cvar_count++;

	CVAR_REGISTER(&extvar.cvar);

	return CVAR_GET_POINTER(name);
}

void ExternalPluginCommand() {
	const char* cmd = CMD_ARGV(0);

	ExternalCommand* ecmd = NULL;

	for (int i = 0; i < g_plugin_command_count; i++) {
		if (!strcmp(g_plugin_commands[i].name, cmd)) {
			ecmd = &g_plugin_commands[i];
			break;
		}
	}

	if (!ecmd) {
		// should never happen
		g_engfuncs.pfnServerPrint(UTIL_VarArgs("Unrecognized external plugin command: %s\n", cmd));
		return;
	}
	
	Plugin* plugin = g_pluginManager.FindPlugin(ecmd->pluginId);

	if (!plugin) {
		g_engfuncs.pfnServerPrint(UTIL_VarArgs("Command from unloaded plugin can't be called: %s\n", cmd));
		return;
	}

	ecmd->function();
}

void RegisterPluginCommand(void* pluginptr, const char* cmd, void (*function)(void)) {
	if (!pluginptr) {
		return;
	}

	Plugin* plugin = (Plugin*)pluginptr;

	if (g_plugin_command_count >= MAX_PLUGIN_COMMANDS) {
		ALERT(at_error, "Plugin command limit exceeded! Failed to register: %s\n", cmd);
		return;
	}

	for (int i = 0; i < g_plugin_command_count; i++) {
		if (!strcmp(g_plugin_commands[i].name, cmd)) {
			//g_engfuncs.pfnServerPrint(UTIL_VarArgs("Plugin command already registered: %s\n", cmd));
			g_plugin_commands[i].pluginId = plugin->id;
			g_plugin_commands[i].function = function;
			return;
		}
	}

	ExternalCommand& ecmd = g_plugin_commands[g_plugin_command_count];
	ecmd.pluginId = plugin->id;
	ecmd.function = function;
	strcpy_safe(ecmd.name, cmd, sizeof(ecmd.name));
	g_plugin_command_count++;

	g_engfuncs.pfnAddServerCommand(ecmd.name, ExternalPluginCommand);
}

void RegisterPlugin(void* pluginptr, HLCOOP_PLUGIN_HOOKS* hooks, const char* name) {
	if (!pluginptr) {
		return;
	}

	Plugin* plugin = (Plugin*)pluginptr;

	plugin->name = name;

	if (hooks)
		memcpy(&plugin->hooks, hooks, sizeof(HLCOOP_PLUGIN_HOOKS));
	else
		memset(&plugin->hooks, 0, sizeof(HLCOOP_PLUGIN_HOOKS));
}

// custom entity loader called by the engine during map load
extern "C" DLLEXPORT void custom(entvars_t * pev) {
	ENTITYINIT initFunc = g_pluginManager.GetCustomEntityInitFunc(STRING(pev->classname));

	if (initFunc) {
		initFunc(pev);
	}
	else {
		ALERT(at_console, "Invalid entity class '%s'\n", STRING(pev->classname));
	}
}