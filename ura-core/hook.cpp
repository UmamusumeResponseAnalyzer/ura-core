#include <stdinclude.hpp>
#include <./notifier/notifier.hpp>

using namespace std;

namespace
{
	bool patch_game_assembly();
	void* load_library_w_orig = nullptr;
	void* set_fps_orig = nullptr;
	void* set_vSyncCount_orig = nullptr;
	void* LZ4_decompress_safe_ext_orig = nullptr;
	void* LZ4_compress_default_ext_orig = nullptr;
	void* get_DatabaseSavePath_orig = nullptr;
	void* GetMasterdataDirectory_orig = nullptr;
	void* LZ4_compress_default_ext_addr = nullptr;
	void* LZ4_decompress_safe_ext_addr = nullptr;

	HMODULE __stdcall load_library_w_hook(const wchar_t* path)
	{
		// GameAssembly.dll code must be loaded and decrypted while loading criware library
		if (path == L"libnative.dll"s)
		{
			if (patch_game_assembly()) {
				MH_DisableHook(LoadLibraryW);
				MH_RemoveHook(LoadLibraryW);

				// use original function beacuse we have unhooked that
				return LoadLibraryW(path);
			}
		}
		return reinterpret_cast<decltype(LoadLibraryW)*>(load_library_w_orig)(path);
	}

	void set_fps_hook(int value)
	{
		return reinterpret_cast<decltype(set_fps_hook)*>(set_fps_orig)(g_max_fps);
	}

	void set_vSyncCount_hook(int value) 
	{
		return reinterpret_cast<decltype(set_vSyncCount_hook)*>(set_vSyncCount_orig)(g_vertical_sync_count);
	}

	int LZ4_decompress_safe_ext_hook(
		char* src,
		char* dst,
		int compressedSize,
		int dstCapacity)
	{
		const int ret = reinterpret_cast<decltype(LZ4_decompress_safe_ext_hook)*>(LZ4_decompress_safe_ext_orig)(
			src, dst, compressedSize, dstCapacity);

		const std::string data(dst, ret);

		auto notifier_thread = std::thread([&]
			{
				notifier::notify_response(data);
			});

		notifier_thread.join();

		return ret;
	}

	int LZ4_compress_default_ext_hook(
		char* src,
		char* dst,
		int srcSize,
		int dstCapacity)
	{
		const int ret = reinterpret_cast<decltype(LZ4_compress_default_ext_hook)*>(LZ4_compress_default_ext_orig)(
			src, dst, srcSize, dstCapacity);

		const std::string data(src, srcSize);

		auto notifier_thread = std::thread([&]
			{
				notifier::notify_request(data);
			});

		notifier_thread.join();

		return ret;
	}

	Il2CppString* get_DatabaseSavePath_hook() {
		if (g_savedata_path.empty())
		{
			return reinterpret_cast<decltype(get_DatabaseSavePath_hook)*>(get_DatabaseSavePath_orig)();
		}
		else
		{
			Il2CppString* ovr = il2cpp_string_new(g_savedata_path.c_str());
			wprintf(L"Override SaveDataPath to %s\n", ovr->start_char);
			return ovr;
		}
	}

	Il2CppString* GetMasterdataDirectory_hook() {
		if (g_savedata_path.empty())
		{
			return reinterpret_cast<decltype(GetMasterdataDirectory_hook)*>(GetMasterdataDirectory_orig)();
		}
		else
		{
			Il2CppString* ovr = il2cpp_string_new(g_savedata_path.c_str());
			wprintf(L"Override GetMasterdataDirectory to %s\n", ovr->start_char);
			return ovr;
		}
	}

	bool patch_game_assembly()
	{
		printf("Trying to patch GameAssembly.dll...\n");

		auto il2cpp_module = GetModuleHandle("GameAssembly.dll");

		// load il2cpp exported functions
		il2cpp_symbols::init(il2cpp_module);

#pragma region HOOK_MACRO
#define ADD_HOOK(_name_, _fmt_) \
	auto _name_##_offset = reinterpret_cast<void*>(_name_##_addr); \
	\
	printf(_fmt_, _name_##_offset); \
	\
	MH_CreateHook(_name_##_offset, _name_##_hook, &_name_##_orig); \
	MH_EnableHook(_name_##_offset); 
#pragma endregion
#pragma region HOOK_ADDRESSES

		auto set_fps_addr = il2cpp_symbols::get_method_pointer(
			"UnityEngine.CoreModule.dll", "UnityEngine",
			"Application", "set_targetFrameRate", 1
		);

		auto set_vSyncCount_addr = il2cpp_symbols::get_method_pointer(
			"UnityEngine.CoreModule.dll", "UnityEngine",
			"QualitySettings", "set_vSyncCount", 1
		);

		auto get_DatabaseSavePath_addr = il2cpp_symbols::get_method_pointer(
			"umamusume.dll", "Gallop",
			"SaveDataManager", "get_DatabaseSavePath", 0
		);

		auto GetMasterdataDirectory_addr = il2cpp_symbols::get_method_pointer(
			"umamusume.dll", "Gallop",
			"MasterDataManager", "GetMasterdataDirectory", 0
		);

		//auto LZ4_compress_default_ext_addr = il2cpp_symbols::get_method_pointer(
		//	"LibNative.Runtime.dll", "LibNative.LZ4",
		//	"Plugin", "LZ4_compress_default_ext", 4
		//);
		//
		//auto LZ4_decompress_safe_ext_addr = il2cpp_symbols::get_method_pointer(
		//	"LibNative.Runtime.dll", "LibNative.LZ4",
		//	"Plugin", "LZ4_decompress_safe_ext", 4
		//);
		const auto libnative = GetModuleHandle("libnative.dll");
		LZ4_compress_default_ext_addr = GetProcAddress(libnative, "LZ4_compress_default_ext");
		LZ4_decompress_safe_ext_addr = GetProcAddress(libnative, "LZ4_decompress_safe_ext");
		if (LZ4_compress_default_ext_addr == nullptr || LZ4_decompress_safe_ext_addr == nullptr)
			return false;
#pragma endregion
		ADD_HOOK(set_fps, "UnityEngine.Application.set_targetFrameRate at %p \n");
		ADD_HOOK(set_vSyncCount, "UnityEngine.QualitySettings.set_vSyncCount at %p \n");
		ADD_HOOK(LZ4_decompress_safe_ext, "LibNative.LZ4.Plugin.LZ4_decompress_safe_ext at %p \n");
		ADD_HOOK(LZ4_compress_default_ext, "LibNative.LZ4.Plugin.LZ4_compress_default_ext at %p \n");
		if (!g_savedata_path.empty())
		{
			ADD_HOOK(get_DatabaseSavePath, "get_DatabaseSavePath at %p\n");
			ADD_HOOK(GetMasterdataDirectory, "GetMasterdataDirectory at %p\n");
		}
		set_vSyncCount_hook(g_vertical_sync_count);
		return true;
	}
}

bool init_hook()
{
	if (compatible_mode) {
		std::this_thread::sleep_for(std::chrono::seconds(10));
		patch_game_assembly();
	}
	else {
		MH_CreateHook(LoadLibraryW, load_library_w_hook, &load_library_w_orig);
		MH_EnableHook(LoadLibraryW);
	}

	std::thread ping_thread([]() {
		notifier::ping();
		});
	ping_thread.detach();

	return true;
}

void uninit_hook()
{
	MH_DisableHook(MH_ALL_HOOKS);
	MH_Uninitialize();
}
