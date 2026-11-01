#include "install_package.hpp"
#include <windows.h>
#include <cassert>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;
static void put(const fs::path &file, const std::string &text) {
    fs::create_directories(file.parent_path());
    std::ofstream out(file, std::ios::binary);
    out << text;
    assert(out);
}
static std::string get(const fs::path &file) {
    std::ifstream in(file, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), {}};
}
static std::string vdfPath(const fs::path &path) {
    const auto utf8 = path.u8string();
    std::string result;
    for (char8_t c : utf8) {
        if (c == '\\' || c == '"') result += '\\';
        result += static_cast<char>(c);
    }
    return result;
}
static void checkSteamDetection(const fs::path &root, const fs::path &retail) {
    using live::setup::DetectSteam;
    const auto steam = root / "Steam", library = root / L"Steam Library with spaces \u00e9";
    const auto primary = steam / "steamapps/common/Call of Duty 4";
    const auto custom = library / "steamapps/common/Custom CoD4 install";
    for (const auto &game : {primary, custom}) {
        fs::create_directories(game.parent_path());
        fs::copy(retail, game, fs::copy_options::recursive);
    }
    const auto folders = steam / "steamapps/libraryfolders.vdf";
    const auto manifest = library / "steamapps/appmanifest_7940.acf";
    const std::string validManifest = "\"AppState\" { \"appid\" \"7940\" \"installdir\" \"Custom CoD4 install\" }";
    const std::string modern = "\xEF\xBB\xBF// library fixture\n\"libraryfolders\" {\n\"1\" {\n\"path\"\n\"" +
        vdfPath(library) + "\"\n\"apps\" { \"7940\" \"123\" } } }\n";
    assert(DetectSteam(steam).empty()); // A plausible folder/executable is not an app-7940 manifest.
    put(folders, modern);
    put(manifest, validManifest);
    assert(DetectSteam(steam) == fs::weakly_canonical(custom));
    put(manifest, "\"AppState\" { \"appid\" \"7941\" \"installdir\" \"Custom CoD4 install\" }");
    assert(DetectSteam(steam).empty());
    put(manifest, "\"AppState\" { \"appid\" \"7940\" \"installdir\" \"../Custom CoD4 install\" }");
    assert(DetectSteam(steam).empty());
    put(manifest, validManifest);
    put(folders, "\"LibraryFolders\" { \"1\" \"" + vdfPath(library) + "\" }");
    assert(DetectSteam(steam) == fs::weakly_canonical(custom)); // Legacy flat library list.
    put(folders, modern.substr(0, modern.size() - 3));
    assert(DetectSteam(steam).empty()); // Truncated nested VDF is not partially accepted.
    put(folders, "\"libraryfolders\" { \"path\" \"" + vdfPath(library) + "\" }");
    assert(DetectSteam(steam).empty()); // Ignore unrelated path metadata.
    put(folders, std::string(262145, ' '));
    assert(DetectSteam(steam).empty());
    put(folders, "\"oversized\" \"" + std::string(32769, 'x') + "\"\n" + modern.substr(3));
    assert(DetectSteam(steam).empty());
    std::string deep;
    for (int i = 0; i < 18; ++i) deep += "\"nested\" { ";
    deep += std::string(18, '}') + modern.substr(3);
    put(folders, deep);
    assert(DetectSteam(steam).empty());
    std::string many;
    for (int i = 0; i < 4097; ++i) many += "\"entry\" \"value\"\n";
    put(folders, many + modern.substr(3));
    assert(DetectSteam(steam).empty());
    put(folders, modern);
    fs::remove(custom / "binkw32.dll");
    assert(DetectSteam(steam).empty()); // Incomplete installation stays available to manual browse only.
    put(custom / "binkw32.dll", "owned middleware fixture");
    put(steam / "steamapps/appmanifest_7940.acf",
        "\"AppState\" { \"appid\" \"7940\" \"installdir\" \"Call of Duty 4\" }");
    assert(DetectSteam(steam) == fs::weakly_canonical(primary));
    assert(get(retail / "main/iw_00.iwd") == "retail fixture");
}
int main(int argc, char **argv) {
    if (argc == 2 && std::string_view(argv[1]) == "--hold") {
        Sleep(30000);
        return 0;
    }
    using namespace live::setup;
    auto root =
        fs::temp_directory_path() / ("khz-installer-test-" + std::to_string(GetCurrentProcessId()));
    assert(!fs::exists(root));
    fs::create_directory(root);
    auto source = root / "Steam game with spaces", destination = root / "Output with spaces";
    IMAGE_DOS_HEADER dos{};
    dos.e_magic = IMAGE_DOS_SIGNATURE;
    dos.e_lfanew = sizeof(dos);
    IMAGE_FILE_HEADER header{};
    header.Machine = IMAGE_FILE_MACHINE_I386;
    DWORD signature = IMAGE_NT_SIGNATURE;
    fs::create_directory(source);
    std::ofstream exe(source / "iw3mp.exe", std::ios::binary);
    exe.write(reinterpret_cast<char *>(&dos), sizeof(dos));
    exe.write(reinterpret_cast<char *>(&signature), sizeof(signature));
    exe.write(reinterpret_cast<char *>(&header), sizeof(header));
    exe.close();
    put(source / "main/iw_00.iwd", "retail fixture");
    put(source / "binkw32.dll", "owned middleware fixture");
    put(source / "mss32.dll", "owned middleware fixture");
    put(source / "localization.txt", "english\n");
    for (auto name :
         {"mp_killhouse.ff", "common_mp.ff", "code_post_gfx_mp.ff", "localized_common_mp.ff"})
        put(source / "zone/english" / name, "asset fixture");
    Plan plan;
    std::string error;
    checkSteamDetection(root, source);
    assert(!Inspect({}, destination, plan, error));
    assert(!Inspect(source, {}, plan, error));
    assert(!Inspect(source, L"   ", plan, error));
    assert(!Inspect(source, source, plan, error));
    assert(!Inspect(source, root, plan, error));
    assert(!Inspect(source, source / "overlap", plan, error));
    assert(!Inspect(source, root.root_path(), plan, error));
    put(destination / "unknown.txt", "must preserve");
    assert(!Inspect(source, destination, plan, error));
    fs::remove(destination / "unknown.txt");
    assert(Inspect(source, destination, plan, error));
    assert(!plan.repair && plan.assets.size() == 7);
    assert(!SafeRelative("../bad") && !SafeRelative("root/con.txt") &&
           !SafeRelative("root/trailing.") && !SafeRelative("C:/bad"));
    assert(SafeRelative("runtime/x64/cod4-live_server.exe"));
    std::atomic_bool cancelled = false;
    assert(
        !Install(plan, cancelled, [&](unsigned, const std::string &) { cancelled = true; }, error));
    assert(!fs::exists(destination) || fs::is_empty(destination));
    for (auto &entry : fs::directory_iterator(root))
        assert(!entry.path().filename().wstring().starts_with(L".cod4-live-install-"));
    cancelled = false;
    assert(Install(plan, cancelled, [](unsigned, const std::string &) {}, error));
    assert(fs::is_regular_file(destination / "cod4-live.exe") &&
           fs::is_regular_file(destination / "cod4-live_server.exe"));
    assert(get(destination / "main/iw_00.iwd") == "retail fixture");
    auto clientHash = HashFile(destination / "cod4-live.exe");
    auto dependencyHash = HashFile(destination / "D3DX9_43.dll");
    fs::remove(destination / "D3DX9_43.dll");
    put(destination / "players/profiles/player/config_mp.cfg",
        "player choices");
    put(destination / "servers/stable/admins.dat", "admins");
    put(destination / "servers/stable/bans.dat", "bans");
    put(destination / "servers/stable/extra.cfg", "custom config");
    put(destination / "config/update-policy.txt", "custom update policy");
    put(destination / "cod4-live.exe", "corrupted project binary");
    assert(Inspect(source, destination, plan, error) && plan.repair && plan.assets.empty());
    assert(Install(plan, cancelled, [](unsigned, const std::string &) {}, error));
    assert(HashFile(destination / "cod4-live.exe") == clientHash);
    assert(HashFile(destination / "D3DX9_43.dll") == dependencyHash);
    assert(get(destination / "players/profiles/player/config_mp.cfg") ==
           "player choices");
    assert(get(destination / "servers/stable/admins.dat") == "admins" &&
           get(destination / "servers/stable/bans.dat") == "bans" &&
           get(destination / "servers/stable/extra.cfg") == "custom config");
    assert(get(destination / "config/update-policy.txt") == "custom update policy");
    put(destination / "cod4-live.exe", "rollback sentinel");
    auto locked = CreateFileW((destination / "cod4-live_server.exe").c_str(), GENERIC_READ,
                              FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    assert(locked != INVALID_HANDLE_VALUE);
    assert(!Install(plan, cancelled, [](unsigned, const std::string &) {}, error));
    CloseHandle(locked);
    assert(get(destination / "cod4-live.exe") == "rollback sentinel");
    assert(Install(plan, cancelled, [](unsigned, const std::string &) {}, error));
    wchar_t image[32768];
    assert(GetModuleFileNameW(nullptr, image, 32768));
    auto child = destination / "owned-fixture.exe";
    fs::copy_file(image, child);
    std::wstring command = L"\"" + child.wstring() + L"\" --hold";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    assert(CreateProcessW(child.c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                          nullptr, destination.c_str(), &startup, &process));
    assert(!Inspect(source, destination, plan, error) &&
           error.find("Close the installed") != error.npos);
    TerminateProcess(process.hProcess, 0);
    WaitForSingleObject(process.hProcess, 5000);
    CloseHandle(process.hProcess);
    CloseHandle(process.hThread);
    fs::remove(child);
    assert(Inspect(source, destination, plan, error));
    auto manifest = get(destination / "cod4-live-install.txt");
    auto first = manifest.find("file=cod4-live.exe\t");
    assert(first != manifest.npos);
    auto end = manifest.find('\n', first);
    manifest.erase(first, end - first + 1);
    put(destination / "cod4-live-install.txt", manifest);
    assert(Inspect(source, destination, plan, error));
    assert(!Install(plan, cancelled, [](unsigned, const std::string &) {}, error));
    assert(error.find("unowned") != error.npos &&
           HashFile(destination / "cod4-live.exe") == clientHash);
    fs::remove(source / "zone/english/localized_common_mp.ff");
    assert(!Inspect(source, root / "another", plan, error));
    // Only this unique test-owned temporary directory is removed.
    assert(fs::weakly_canonical(root.parent_path()) ==
           fs::weakly_canonical(fs::temp_directory_path()));
    fs::remove_all(root);
    std::cout << "Steam app-7940/custom-library detection, bounded VDF parsing, installer spaces, assets, destination checks, cancellation, repair, profile "
                 "preservation and unowned-file protection passed\n";
}
