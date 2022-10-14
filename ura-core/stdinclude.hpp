#pragma once

#include <Windows.h>
#include <shlobj.h>

#include <cinttypes>

#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <locale>
#include <string>
#include <thread>
#include <unordered_map>

#include <MinHook.h>


#include "il2cpp/il2cpp_symbols.hpp"

extern bool compatible_mode;
extern int g_max_fps;
extern int g_vertical_sync_count;
extern bool g_skip_single_instance_check;
extern std::string g_notifier_host;
extern std::string g_savedata_path;
