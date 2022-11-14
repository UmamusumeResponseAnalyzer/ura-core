#include <rapidjson/document.h>
#include <rapidjson/encodings.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/stringbuffer.h>
#include <stdinclude.hpp>
#include <./dllproxy/proxy.hpp>

extern bool init_hook();
extern void uninit_hook();

bool compatible_mode = false;
int g_max_fps = -1;
int g_vertical_sync_count = 0;
bool g_skip_single_instance_check = true;
std::string g_notifier_host = "http://127.0.0.1:4693";
std::string g_savedata_path = "";

namespace
{
	void read_config()
	{
		std::ifstream config_stream{ "config.json" };

		if (!config_stream.is_open())
			return;

		rapidjson::IStreamWrapper wrapper{ config_stream };
		rapidjson::Document document;

		document.ParseStream(wrapper);

		if (!document.HasParseError() && !compatible_mode)
		{
			if (document.HasMember("maxFps")) {
				g_max_fps = document["maxFps"].GetInt();
			}
			else {
				DEVMODE monitor{};
				monitor.dmSize = sizeof(DEVMODE);
				EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &monitor);
				g_max_fps = monitor.dmDisplayFrequency;
			}
			if (document.HasMember("notifier_host"))
				g_notifier_host = document["notifier_host"].GetString();
			if (document.HasMember("savedata_path"))
				g_savedata_path = document["savedata_path"].GetString();
			if (document.HasMember("skip_single_instance_check"))
				g_skip_single_instance_check = document["skip_single_instance_check"].GetBool();
			if (document.HasMember("vertical_sync_count"))
				g_vertical_sync_count = document["vertical_sync_count"].GetInt();
		}

		config_stream.close();
	}
}


void* CreateMutex_orig = nullptr;
void* CreateMutexDetour(
	LPSECURITY_ATTRIBUTES lpMutexAttributes,
	BOOL                  bInitialOwner,
	LPCSTR                lpName)
{
	if (lpName != NULL && strstr(lpName, "-SingleInstanceMutex-")) {
		std::ostringstream oss;
		oss << lpName << '-' << std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		return reinterpret_cast<decltype(CreateMutex)*>(CreateMutex_orig)(lpMutexAttributes, bInitialOwner, oss.str().c_str());
	}

	return reinterpret_cast<decltype(CreateMutex)*>(CreateMutex_orig)(lpMutexAttributes, bInitialOwner, lpName);
}
int __stdcall DllMain(HINSTANCE hInstance, DWORD reason, LPVOID)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		// the DMM Launcher set start path to system32 wtf????
		std::string module_name;
		module_name.resize(MAX_PATH);
		module_name.resize(GetModuleFileName(nullptr, module_name.data(), MAX_PATH));

		std::filesystem::path module_path(module_name);

		LPSTR proxy_filename_buffer = new CHAR[MAX_PATH];
		GetModuleFileNameA(hInstance, proxy_filename_buffer, MAX_PATH);
		std::filesystem::path proxy_filename_path(proxy_filename_buffer);
		std::string proxy_filename = proxy_filename_path.filename().string();
		std::for_each(proxy_filename.begin(), proxy_filename.end(), [](char& character) { character = ::tolower(character); });
		compatible_mode = proxy_filename != "version.dll";
		proxy::init_proxy(proxy_filename);

		// check name
		if (module_path.filename() != "umamusume.exe")
			return 1;

		std::filesystem::current_path(
			module_path.parent_path()
		);

		read_config();

#if defined _DEBUG
		AllocConsole();

		// open stdout stream
		FILE* stream;
		freopen_s(&stream, "CONOUT$", "w+t", stdout);
		freopen_s(&stream, "CONOUT$", "w", stderr);
		freopen_s(&stream, "CONIN$", "r", stdin);

		SetConsoleTitle("URA Core - Debug Console");

		// set this to avoid turn japanese texts into question mark
		SetConsoleOutputCP(65001);
		std::locale::global(std::locale(""));
#endif

		MH_Initialize();
		if (g_skip_single_instance_check) {
			MH_CreateHook(&CreateMutex, &CreateMutexDetour, &CreateMutex_orig);
			MH_EnableHook(&CreateMutex);
		}
		std::thread init_thread([]() {
			init_hook();
			});
		init_thread.detach();
	}
	else if (reason == DLL_PROCESS_DETACH)
	{
		uninit_hook();
	}

	return 1;
}
