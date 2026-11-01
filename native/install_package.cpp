#include "install_package.hpp"
#include "desktop_ui.hpp"
#include <fdi.h>
#include <bcrypt.h>
#include <tlhelp32.h>
#include <io.h>
#include <fcntl.h>
#include <share.h>
#include <sys/stat.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <set>
#include <cstring>
#include <cctype>

namespace live::setup {
namespace fs = std::filesystem;
namespace {
fs::path normalized(const fs::path &p) { return fs::weakly_canonical(fs::absolute(p)); }
bool inside(const fs::path &path, const fs::path &root) {
    auto a = normalized(path).wstring(), b = normalized(root).wstring();
    for (auto &ch : a)
        ch = towlower(ch);
    for (auto &ch : b)
        ch = towlower(ch);
    return a == b ||
           (a.size() > b.size() && a.compare(0, b.size(), b) == 0 && a[b.size()] == L'\\');
}
bool reparse(const fs::path &p) {
    auto at = p;
    while (!at.empty()) {
        auto flags = GetFileAttributesW(at.c_str());
        if (flags != INVALID_FILE_ATTRIBUTES && (flags & FILE_ATTRIBUTE_REPARSE_POINT))
            return true;
        auto parent = at.parent_path();
        if (parent == at)
            break;
        at = parent;
    }
    return false;
}
bool legacyPath(const fs::path &path) {
    auto text = path.wstring();
    BOOL used = FALSE;
    char buffer[170];
    auto n = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, text.c_str(), -1, buffer,
                                 sizeof(buffer), nullptr, &used);
    return n > 0 && !used;
}
bool running(const fs::path &root) {
    auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return true;
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    bool found = false;
    if (Process32FirstW(snapshot, &entry))
        do {
            if (entry.th32ProcessID == GetCurrentProcessId())
                continue;
            auto process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE,
                                       entry.th32ProcessID);
            if (process) {
                wchar_t image[32768];
                DWORD size = std::size(image);
                if (QueryFullProcessImageNameW(process, 0, image, &size) &&
                    inside(fs::path(std::wstring(image, size)), root) &&
                    WaitForSingleObject(process, 0) == WAIT_TIMEOUT)
                    found = true;
                CloseHandle(process);
            } else if (std::wstring_view(entry.szExeFile).starts_with(L"cod4-live") &&
                       GetLastError() != ERROR_INVALID_PARAMETER)
                found = true;
        } while (!found && Process32NextW(snapshot, &entry));
    CloseHandle(snapshot);
    return found;
}
std::string read(const fs::path &path, unsigned maximum = 262144) {
    std::error_code ec;
    auto bytes = fs::file_size(path, ec);
    if (ec || bytes > maximum)
        return {};
    std::ifstream file(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(file), {});
}
std::set<std::string> installedPaths(const fs::path &root) {
    auto text = read(root / "cod4-live-install.txt");
    std::set<std::string> result;
    if (!text.starts_with("COD4LIVE_INSTALL_1\nproject=cod4-live\n"))
        return result;
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line))
        if (line.starts_with("file=")) {
            auto end = line.find('\t');
            auto name = line.substr(5, end == line.npos ? line.size() - 5 : end - 5);
            if (SafeRelative(name))
                result.insert(name);
        }
    return result;
}
bool executable(const fs::path &path) {
    std::ifstream file(path, std::ios::binary);
    IMAGE_DOS_HEADER dos{};
    DWORD signature = 0;
    IMAGE_FILE_HEADER header{};
    file.read(reinterpret_cast<char *>(&dos), sizeof(dos));
    if (!file || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew < sizeof(dos) ||
        dos.e_lfanew > 1048576)
        return false;
    file.seekg(dos.e_lfanew);
    file.read(reinterpret_cast<char *>(&signature), sizeof(signature));
    file.read(reinterpret_cast<char *>(&header), sizeof(header));
    return file && signature == IMAGE_NT_SIGNATURE && header.Machine == IMAGE_FILE_MACHINE_I386;
}
struct Payload {
    std::string file, relative, hash;
};
void *DIAMONDAPI allocate(ULONG bytes) { return malloc(bytes); }
void DIAMONDAPI release(void *data) { free(data); }
INT_PTR DIAMONDAPI cabinetOpen(char *name, int flags, int mode) {
    int file = -1;
    auto path = fs::path(name);
    if (_wsopen_s(&file, path.c_str(), flags | _O_BINARY, _SH_DENYNO, mode))
        return -1;
    return file;
}
UINT DIAMONDAPI cabinetRead(INT_PTR file, void *data, UINT bytes) {
    return _read(int(file), data, bytes);
}
UINT DIAMONDAPI cabinetWrite(INT_PTR file, void *data, UINT bytes) {
    return _write(int(file), data, bytes);
}
int DIAMONDAPI cabinetClose(INT_PTR file) { return _close(int(file)); }
long DIAMONDAPI cabinetSeek(INT_PTR file, long distance, int origin) {
    return _lseek(int(file), distance, origin);
}
INT_PTR DIAMONDAPI notification(FDINOTIFICATIONTYPE type, PFDINOTIFICATION info) {
    auto *folder = reinterpret_cast<fs::path *>(info->pv);
    if (type == fdintCOPY_FILE) {
        std::string name = info->psz1;
        if (name != "manifest.tsv" && (name.size() != 9 || name.substr(0, 4) != "file" ||
                                       name.find_first_not_of("0123456789", 4) != name.npos))
            return -1;
        auto file = *folder / name;
        int handle = -1;
        if (_wsopen_s(&handle, file.c_str(), _O_BINARY | _O_CREAT | _O_EXCL | _O_WRONLY, _SH_DENYNO,
                      _S_IREAD | _S_IWRITE))
            return -1;
        return handle;
    }
    if (type == fdintCLOSE_FILE_INFO) {
        _close(int(info->hf));
        return TRUE;
    }
    return 0;
}
bool unpack(const fs::path &stage, std::vector<Payload> &entries, std::string &error) {
    auto resource = FindResourceW(nullptr, MAKEINTRESOURCEW(100), RT_RCDATA);
    auto bytes = resource ? SizeofResource(nullptr, resource) : 0;
    auto loaded = resource ? LoadResource(nullptr, resource) : nullptr;
    auto data = loaded ? LockResource(loaded) : nullptr;
    if (!data || !bytes) {
        error = "Installer payload is missing";
        return false;
    }
    auto cab = stage / "payload.cab";
    std::ofstream output(cab, std::ios::binary);
    output.write(static_cast<const char *>(data), bytes);
    output.close();
    if (!output) {
        error = "Cannot write embedded installer payload";
        return false;
    }
    auto extracted = stage / "payload";
    fs::create_directory(extracted);
    ERF status{};
    auto fdi = FDICreate(allocate, release, cabinetOpen, cabinetRead, cabinetWrite, cabinetClose,
                         cabinetSeek, cpuUNKNOWN, &status);
    if (!fdi) {
        error = "Cannot initialize Windows cabinet extraction";
        return false;
    }
    std::string folder = stage.string() + "\\", name = "payload.cab";
    bool ok =
        FDICopy(fdi, name.data(), folder.data(), 0, notification, nullptr, &extracted) != FALSE;
    FDIDestroy(fdi);
    if (!ok) {
        error = "Embedded cabinet is corrupt";
        return false;
    }
    std::istringstream lines(read(extracted / "manifest.tsv"));
    std::string line;
    std::set<std::string> seen;
    while (std::getline(lines, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        auto first = line.find('\t'), second = line.find('\t', first + 1);
        if (first == line.npos || second == line.npos) {
            error = "Invalid payload manifest";
            return false;
        }
        Payload entry{line.substr(0, first), line.substr(second + 1),
                      line.substr(first + 1, second - first - 1)};
        if (!SafeRelative(entry.relative) || entry.hash.size() != 64 ||
            !seen.insert(entry.relative).second || !fs::is_regular_file(extracted / entry.file) ||
            HashFile(extracted / entry.file) != entry.hash) {
            error = "Payload checksum/path verification failed";
            return false;
        }
        entries.push_back(entry);
    }
    for (auto required : {"cod4-live.exe", "cod4-live_server.exe", "D3DX9_43.dll", "steam_api.dll"}) {
        if (!seen.contains(required)) {
            error = "Installer payload is missing a required binary/runtime dependency";
            return false;
        }
    }
    return true;
}
DWORD CALLBACK copyProgress(LARGE_INTEGER, LARGE_INTEGER, LARGE_INTEGER, LARGE_INTEGER, DWORD,
                            DWORD, HANDLE, HANDLE, LPVOID state) {
    return static_cast<std::atomic_bool *>(state)->load() ? PROGRESS_CANCEL : PROGRESS_CONTINUE;
}
bool copy(const fs::path &source, const fs::path &target, std::atomic_bool &cancel) {
    fs::create_directories(target.parent_path());
    return CopyFileExW(source.c_str(), target.c_str(), copyProgress, &cancel, nullptr,
                       COPY_FILE_FAIL_IF_EXISTS) != FALSE;
}
void cleanup(const fs::path &stage, const fs::path &parent) {
    if (stage.filename().wstring().starts_with(L".cod4-live-install-") && inside(stage, parent) &&
        !reparse(stage)) {
        std::error_code ignored;
        fs::remove_all(stage, ignored);
    }
}
} // namespace
bool SafeRelative(std::string_view text) {
    if (text.empty() || text.size() > 200 || text.front() == '/' ||
        text.find_first_of("\\:\r\n\t<>\"|?*") != text.npos || text.find('\0') != text.npos)
        return false;
    std::istringstream parts{std::string(text)};
    std::string part;
    while (std::getline(parts, part, '/')) {
        if (part.empty() || part == "." || part == ".." || part.back() == '.' || part.back() == ' ')
            return false;
        auto base = part.substr(0, part.find('.'));
        for (auto &ch : base)
            ch = char(std::tolower(static_cast<unsigned char>(ch)));
        if (base == "con" || base == "prn" || base == "aux" || base == "nul" ||
            (base.size() == 4 && (base.starts_with("com") || base.starts_with("lpt")) &&
             base[3] >= '0' && base[3] <= '9'))
            return false;
    }
    return text.back() != '/';
}
std::string HashFile(const fs::path &path) {
    BCRYPT_ALG_HANDLE provider = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    if (BCryptOpenAlgorithmProvider(&provider, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
        return {};
    DWORD size = 0, written = 0;
    BCryptGetProperty(provider, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&size), sizeof(size),
                      &written, 0);
    std::vector<unsigned char> object(size), buffer(1024 * 1024);
    std::string result;
    if (BCryptCreateHash(provider, &hash, object.data(), size, nullptr, 0, 0) >= 0) {
        std::ifstream file(path, std::ios::binary);
        bool good = bool(file);
        while (good && file) {
            file.read(reinterpret_cast<char *>(buffer.data()), buffer.size());
            auto bytes = file.gcount();
            if (bytes && BCryptHashData(hash, buffer.data(), ULONG(bytes), 0) < 0)
                good = false;
        }
        unsigned char digest[32];
        if (good && file.eof() && BCryptFinishHash(hash, digest, 32, 0) >= 0) {
            constexpr auto hex = "0123456789abcdef";
            for (auto value : digest) {
                result += hex[value >> 4];
                result += hex[value & 15];
            }
        }
        BCryptDestroyHash(hash);
    }
    BCryptCloseAlgorithmProvider(provider, 0);
    return result;
}
bool Inspect(const fs::path &game, const fs::path &destination, Plan &result, std::string &error) {
    try {
        if (game.wstring().find_first_not_of(L" \t\r\n") == std::wstring::npos ||
            destination.wstring().find_first_not_of(L" \t\r\n") == std::wstring::npos) {
            error = "Select the Steam game folder and a separate output folder";
            return false;
        }
        Plan plan;
        plan.game = normalized(game);
        plan.destination = normalized(destination);
        if (!executable(plan.game / "iw3mp.exe") || !fs::is_directory(plan.game / "main") ||
            !fs::is_directory(plan.game / "zone") ||
            !fs::is_regular_file(plan.game / "localization.txt")) {
            error = "Choose the Steam Call of Duty 4 (2007) folder containing the original "
                    "iw3mp.exe, main, zone and localization.txt";
            return false;
        }
        if (plan.destination == plan.destination.root_path() ||
            inside(plan.destination, plan.game) || inside(plan.game, plan.destination)) {
            error = "Choose a separate output folder outside the original game installation";
            return false;
        }
        if (reparse(plan.game) || reparse(plan.destination) || !legacyPath(plan.destination)) {
            error = "Choose a regular local folder with a short path representable by the game's "
                    "Windows code page";
            return false;
        }
        plan.repair = !installedPaths(plan.destination).empty();
        if (fs::exists(plan.destination) && (!fs::is_directory(plan.destination) ||
                                             (!fs::is_empty(plan.destination) && !plan.repair))) {
            error = "Output folder must be empty or contain a cod4-live installation";
            return false;
        }
        if (plan.repair && running(plan.destination)) {
            error =
                "Close the installed game and dedicated server before repairing";
            return false;
        }
        bool multiplayer = false, iwd = false, common = false, code = false, localized = false;
        for (auto directory : {"main", "zone"})
            for (auto &entry : fs::recursive_directory_iterator(plan.game / directory)) {
                if (entry.is_symlink() || reparse(entry.path())) {
                    error = "Game assets contain a link/reparse point; select a regular Steam "
                            "installation";
                    return false;
                }
                if (!entry.is_regular_file())
                    continue;
                const auto extension = entry.path().extension().wstring();
                // Copy game assets, not the source installation's logs or personal configs.
                if (extension != L".iwd" && extension != L".ff" && extension != L".bik")
                    continue;
                auto name = entry.path().filename().wstring();
                if (name.starts_with(L"mp_") && entry.path().extension() == L".ff")
                    multiplayer = true;
                if (entry.path().extension() == L".iwd")
                    iwd = true;
                if (name == L"common_mp.ff")
                    common = true;
                if (name == L"code_post_gfx_mp.ff")
                    code = true;
                if (name == L"localized_common_mp.ff")
                    localized = true;
                auto relative = entry.path().lexically_relative(plan.game);
                auto target = plan.destination / relative;
                if (!SafeRelative(relative.generic_string()) || target.wstring().size() >= 250) {
                    error = "An asset path exceeds the engine's supported path limits";
                    return false;
                }
                if (plan.repair && fs::is_regular_file(target))
                    continue;
                auto bytes = entry.file_size();
                plan.assets.push_back({entry.path(), relative, bytes});
                plan.bytes += bytes;
            }
        if (!multiplayer || !iwd || !common || !code || !localized) {
            error =
                "Required multiplayer, common/code, localized fastfiles or IWD assets are missing";
            return false;
        }
        if (!plan.repair || !fs::is_regular_file(plan.destination / "localization.txt")) {
            auto source = plan.game / "localization.txt";
            auto bytes = fs::file_size(source);
            plan.assets.push_back({source, "localization.txt", bytes});
            plan.bytes += bytes;
        }
        // Proprietary middleware is copied only from the player's owned installation.
        for (auto name : {"binkw32.dll"}) {
            auto source = plan.game / name;
            if (!fs::is_regular_file(source)) { error = std::string("Missing retail runtime: ") + name; return false; }
            if (!plan.repair || !fs::is_regular_file(plan.destination / name)) {
                auto bytes = fs::file_size(source);
                plan.assets.push_back({source, name, bytes}); plan.bytes += bytes;
            }
        }
        auto parent = plan.destination.parent_path();
        if (!fs::is_directory(parent)) {
            error = "Create/select an output folder whose parent directory exists";
            return false;
        }
        auto available = fs::space(parent).available;
        if (available < plan.bytes + 1024ull * 1024 * 1024) {
            error =
                "Not enough disk space for copied assets, project files and installation staging";
            return false;
        }
        char nonce[33];
        live::hosting::NewToken(nonce);
        auto probe = parent / (".khz-write-test-" + std::string(nonce));
        std::ofstream test(probe);
        test << "test";
        test.close();
        bool writable = bool(test);
        fs::remove(probe);
        if (!writable) {
            error = "Output parent folder is not writable";
            return false;
        }
        result = std::move(plan);
        return true;
    } catch (const std::exception &e) {
        error = e.what();
        return false;
    }
}
bool Install(const Plan &plan, std::atomic_bool &cancel, Progress progress, std::string &error) {
    auto lockPath = plan.destination.parent_path() /
                    (L"." + plan.destination.filename().wstring() + L"-khz-install.lock");
    struct Lock {
        HANDLE handle;
        ~Lock() {
            if (handle != INVALID_HANDLE_VALUE)
                CloseHandle(handle);
        }
    } lock{CreateFileW(lockPath.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                       FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr)};
    if (lock.handle == INVALID_HANDLE_VALUE) {
        error = "Another installation is using this destination, or its parent is not writable";
        return false;
    }
    char nonce[33];
    if (!live::hosting::NewToken(nonce)) {
        error = "Cannot create installation transaction";
        return false;
    }
    auto stage = plan.destination.parent_path() / (".cod4-live-install-" + std::string(nonce));
    std::vector<fs::path> added;
    std::vector<std::pair<fs::path, fs::path>> replaced;
    try {
        if (!fs::create_directory(stage)) {
            error = "Cannot create unique installation stage";
            return false;
        }
        std::vector<Payload> payload;
        progress(0, "Verifying embedded project files");
        if (!unpack(stage, payload, error)) {
            cleanup(stage, plan.destination.parent_path());
            return false;
        }
        auto installed = stage / "installed";
        fs::create_directory(installed);
        std::uint64_t done = 0;
        for (auto &entry : plan.assets) {
            if (cancel.load()) {
                error = "Installation cancelled";
                cleanup(stage, plan.destination.parent_path());
                return false;
            }
            progress(unsigned(done * 85 / std::max<std::uint64_t>(plan.bytes, 1)),
                     "Copying " + entry.relative.generic_string());
            if (!copy(entry.source, installed / entry.relative, cancel))
                throw std::runtime_error(cancel.load() ? "Installation cancelled"
                                                       : "Game asset copy failed (Windows error " +
                                                             std::to_string(GetLastError()) + ")");
            done += entry.bytes;
        }
        std::ostringstream manifest;
        manifest << "COD4LIVE_INSTALL_1\nproject=cod4-live\n";
        for (auto &entry : payload) {
            if (cancel.load())
                throw std::runtime_error("Installation cancelled");
            auto target = installed / fs::path(entry.relative);
            // Mutable project defaults belong to the user after the first installation.
            // Keep them in the ownership manifest without staging a replacement on repair.
            auto existing = plan.destination / fs::path(entry.relative);
            if (plan.repair && entry.relative.starts_with("config/") &&
                fs::is_regular_file(existing) &&
                installedPaths(plan.destination).contains(entry.relative)) {
                manifest << "file=" << entry.relative << '\t' << HashFile(existing) << '\n';
                continue;
            }
            if (!copy(stage / "payload" / entry.file, target, cancel))
                throw std::runtime_error("Cannot stage project file");
            manifest << "file=" << entry.relative << '\t' << entry.hash << '\n';
        }
        std::ofstream record(installed / "cod4-live-install.txt", std::ios::binary);
        record << manifest.str();
        record.close();
        if (!record)
            throw std::runtime_error("Cannot write installation manifest");
        progress(90, "Finishing installation");
        if (cancel.load())
            throw std::runtime_error("Installation cancelled");
        if (running(plan.destination))
            throw std::runtime_error(
                "Close running installed applications before finishing installation");
        if (!plan.repair) {
            if (fs::exists(plan.destination)) {
                if (!fs::is_empty(plan.destination))
                    throw std::runtime_error("Output folder changed during installation");
                fs::remove(plan.destination);
            }
            fs::rename(installed, plan.destination);
        } else {
            auto ownership = installedPaths(plan.destination);
            auto backup = stage / "backup";
            fs::create_directory(backup);
            std::vector<fs::path> files;
            for (auto &entry : fs::recursive_directory_iterator(installed))
                if (entry.is_regular_file())
                    files.push_back(entry.path());
            for (auto &file : files) {
                auto relative = file.lexically_relative(installed);
                auto target = plan.destination / relative;
                if (reparse(target))
                    throw std::runtime_error("Destination contains a reparse point");
                if (fs::exists(target) && relative != "cod4-live-install.txt" &&
                    !ownership.contains(relative.generic_string()))
                    throw std::runtime_error("Repair would overwrite an unowned file: " +
                                             relative.generic_string());
            }
            std::sort(files.begin(), files.end(), [&](const auto &a, const auto &b) {
                return a.filename() == L"cod4-live-install.txt"   ? false
                       : b.filename() == L"cod4-live-install.txt" ? true
                                                                  : a < b;
            });
            for (auto &file : files) {
                auto relative = file.lexically_relative(installed);
                auto target = plan.destination / relative;
                fs::create_directories(target.parent_path());
                if (fs::exists(target)) {
                    auto saved = backup / relative;
                    fs::create_directories(saved.parent_path());
                    fs::rename(target, saved);
                    replaced.emplace_back(target, saved);
                } else
                    added.push_back(target);
                fs::rename(file, target);
            }
        }
        cleanup(stage, plan.destination.parent_path());
        progress(100, "Installed successfully. Launch cod4-live or host a server.");
        return true;
    } catch (const std::exception &e) {
        error = e.what();
        bool recovered = true;
        for (auto &file : added) {
            std::error_code ec;
            fs::remove(file, ec);
            if (ec)
                recovered = false;
        }
        for (auto i = replaced.rbegin(); i != replaced.rend(); ++i) {
            std::error_code ec;
            fs::remove(i->first, ec);
            if (ec) {
                recovered = false;
                continue;
            }
            fs::rename(i->second, i->first, ec);
            if (ec)
                recovered = false;
        }
        if (recovered)
            cleanup(stage, plan.destination.parent_path());
        else
            error += ". Recovery files retained at " + live::desktop::Utf8(stage.wstring());
        return false;
    }
}
namespace {
struct VdfNode {
    std::string key, value;
    std::vector<VdfNode> children;
    bool object = false;
};
class VdfReader {
    enum class Kind { Text, Open, Close, End, Error };
    std::string_view text;
    std::size_t at = 0, nodes = 0;
    Kind Token(std::string &value) {
        value.clear();
        for (;;) {
            while (at < text.size() && std::isspace(static_cast<unsigned char>(text[at]))) ++at;
            if (at + 1 < text.size() && text[at] == '/' && text[at + 1] == '/') {
                while (at < text.size() && text[at] != '\n') ++at;
            } else break;
        }
        if (at == text.size()) return Kind::End;
        const char first = text[at++];
        if (!first) return Kind::Error;
        if (first == '{') return Kind::Open;
        if (first == '}') return Kind::Close;
        if (first == '"') {
            while (at < text.size()) {
                char c = text[at++];
                if (c == '"') return Kind::Text;
                if (c == '\\' && at < text.size() && (text[at] == '\\' || text[at] == '"')) c = text[at++];
                if (!c || value.size() >= 32768) return Kind::Error;
                value += c;
            }
            return Kind::Error;
        }
        value += first;
        while (at < text.size() && !std::isspace(static_cast<unsigned char>(text[at])) && text[at] != '{' && text[at] != '}') {
            if (!text[at] || value.size() >= 32768) return Kind::Error;
            value += text[at++];
        }
        return Kind::Text;
    }
    bool Object(std::vector<VdfNode> &output, unsigned depth, bool enclosed) {
        if (depth > 16) return false;
        for (;;) {
            VdfNode node;
            const auto kind = Token(node.key);
            if (kind == Kind::End) return !enclosed;
            if (kind == Kind::Close) return enclosed;
            if (kind != Kind::Text || ++nodes > 4096) return false;
            const auto valueKind = Token(node.value);
            if (valueKind == Kind::Open) {
                node.object = true;
                if (!Object(node.children, depth + 1, true)) return false;
            } else if (valueKind != Kind::Text) return false;
            output.push_back(std::move(node));
        }
    }
public:
    explicit VdfReader(std::string_view input) : text(input) {
        if (text.starts_with("\xEF\xBB\xBF")) at = 3;
    }
    bool Read(std::vector<VdfNode> &output) { return Object(output, 0, false); }
};
const VdfNode *FindVdf(const std::vector<VdfNode> &nodes, const char *key) {
    for (const auto &node : nodes)
        if (!_stricmp(node.key.c_str(), key)) return &node;
    return nullptr;
}
std::vector<VdfNode> ReadVdf(const fs::path &path) {
    const auto text = read(path); // 256 KiB maximum, plus bounded depth/nodes/strings.
    std::vector<VdfNode> result;
    if (!VdfReader(text).Read(result)) result.clear();
    return result;
}
fs::path GameInSteamLibrary(const fs::path &library) {
    const auto manifest = ReadVdf(library / "steamapps/appmanifest_7940.acf");
    const auto *state = FindVdf(manifest, "AppState");
    if (!state || !state->object) return {};
    const auto *app = FindVdf(state->children, "appid"), *directory = FindVdf(state->children, "installdir");
    if (!app || app->object || app->value != "7940" || !directory || directory->object ||
        !SafeRelative(directory->value)) return {};
    const fs::path folder = live::desktop::Wide(directory->value);
    if (folder.has_parent_path() || folder.is_absolute()) return {};
    const auto candidate = library / "steamapps/common" / folder;
    if (reparse(candidate) || !executable(candidate / "iw3mp.exe") ||
        !fs::is_directory(candidate / "main") || !fs::is_directory(candidate / "zone") ||
        !fs::is_regular_file(candidate / "localization.txt") || !fs::is_regular_file(candidate / "binkw32.dll")) return {};
    return normalized(candidate);
}
} // namespace

fs::path DetectSteam(const fs::path &steamDirectory) {
    try {
        if (steamDirectory.empty()) return {};
        std::vector<fs::path> roots{steamDirectory};
        const auto document = ReadVdf(steamDirectory / "steamapps/libraryfolders.vdf");
        if (const auto *libraries = FindVdf(document, "libraryfolders"); libraries && libraries->object) {
            for (const auto &library : libraries->children) {
                if (library.key.empty() || library.key.find_first_not_of("0123456789") != std::string::npos) continue;
                const auto *path = library.object ? FindVdf(library.children, "path") : &library;
                if (path && !path->object && !path->value.empty()) {
                    fs::path root = live::desktop::Wide(path->value);
                    if (root.is_absolute()) roots.push_back(std::move(root));
                }
            }
        }
        for (const auto &library : roots)
            if (const auto game = GameInSteamLibrary(library); !game.empty()) return game;
    } catch (const std::exception &) {
        // Discovery is optional; the user can still browse to a valid source.
    }
    return {};
}

fs::path DetectSteam() {
    auto registryRoot = [](HKEY hive, const wchar_t *value, DWORD view) {
        wchar_t path[32768]{};
        DWORD size = sizeof(path);
        return RegGetValueW(hive, L"Software\\Valve\\Steam", value, RRF_RT_REG_SZ | view,
                           nullptr, path, &size) == ERROR_SUCCESS ? fs::path(path) : fs::path{};
    };
    for (const auto &root : {registryRoot(HKEY_CURRENT_USER, L"SteamPath", 0),
                            registryRoot(HKEY_LOCAL_MACHINE, L"InstallPath", RRF_SUBKEY_WOW6432KEY),
                            registryRoot(HKEY_LOCAL_MACHINE, L"InstallPath", RRF_SUBKEY_WOW6464KEY)})
        if (const auto game = DetectSteam(root); !game.empty()) return game;
    return {};
}
} // namespace live::setup
