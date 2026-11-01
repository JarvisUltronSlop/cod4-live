#pragma once
#include "desktop_ui.hpp"
#include <algorithm>

namespace live::setup {
// The complete bundled document is embedded by packaging as UTF-8 RCDATA 104.
// This dialog records an explicit choice only for the Microsoft runtime.
inline bool ReviewRuntimeTerms(HWND owner, HFONT font) {
    const auto resource = FindResourceW(nullptr, MAKEINTRESOURCEW(104), RT_RCDATA);
    const DWORD bytes = resource ? SizeofResource(nullptr, resource) : 0;
    const auto loaded = resource ? LoadResource(nullptr, resource) : nullptr;
    const auto data = loaded ? static_cast<const char *>(LockResource(loaded)) : nullptr;
    if (!data || !bytes || bytes > 1024 * 1024 || std::memchr(data, 0, bytes)) {
        live::desktop::DarkMessageBox(owner, L"The Microsoft runtime terms could not be loaded. Download a complete installer before continuing.",
                                     L"Runtime terms unavailable", MB_OK | MB_ICONERROR);
        return false;
    }
    std::string normalized;
    normalized.reserve(bytes + bytes / 20);
    for (DWORD i = 0; i < bytes; ++i) {
        if (data[i] == '\n' && (i == 0 || data[i - 1] != '\r')) normalized += '\r';
        normalized += data[i];
    }
    struct State {
        HWND owner{}, window{}, notice{}, text{}, agree{}, accept{}, cancel{};
        HFONT font{};
        unsigned dpi = 96;
        bool accepted = false;
        std::wstring terms;
        int Scale(int value) const { return MulDiv(value, dpi, 96); }
        static RECT WorkArea(HWND reference) {
            MONITORINFO monitor{sizeof(monitor)};
            if (GetMonitorInfoW(MonitorFromWindow(reference, MONITOR_DEFAULTTONEAREST), &monitor))
                return monitor.rcWork;
            RECT result{};
            if (!SystemParametersInfoW(SPI_GETWORKAREA, 0, &result, 0))
                result = {0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
            return result;
        }
        void Layout() const {
            RECT rect{}; GetClientRect(window, &rect);
            const int width = rect.right, height = rect.bottom;
            const int margin = std::min(Scale(20), std::max(4, width / 24));
            const int available = std::max(1, width - 2 * margin), gap = Scale(10);
            const int cancelWidth = std::min(Scale(100), available / 3);
            const int acceptWidth = std::min(Scale(190), std::max(1, available - cancelWidth - gap));
            const int buttonY = std::max(0, height - margin - Scale(30));
            const int agreementY = std::max(0, buttonY - gap - Scale(44));
            const int textY = margin + Scale(55) + gap;
            MoveWindow(notice, margin, margin, available, Scale(55), TRUE);
            MoveWindow(text, margin, textY, available, std::max(1, agreementY - gap - textY), TRUE);
            MoveWindow(agree, margin, agreementY, available, Scale(44), TRUE);
            MoveWindow(accept, width - margin - cancelWidth - gap - acceptWidth, buttonY, acceptWidth, Scale(30), TRUE);
            MoveWindow(cancel, width - margin - cancelWidth, buttonY, cancelWidth, Scale(30), TRUE);
        }
        static LRESULT CALLBACK Proc(HWND window, UINT msg, WPARAM w, LPARAM l) {
            if (msg == WM_NCCREATE) {
                auto state = static_cast<State *>(reinterpret_cast<CREATESTRUCTW *>(l)->lpCreateParams);
                state->window = window;
                SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
            }
            auto state = reinterpret_cast<State *>(GetWindowLongPtrW(window, GWLP_USERDATA));
            LRESULT themed;
            if (live::desktop::DarkMessage(window, msg, w, l, themed)) return themed;
            if (!state) return DefWindowProcW(window, msg, w, l);
            if (msg == WM_CREATE) {
                using live::desktop::Control;
                live::desktop::DarkTitleBar(window);
                state->notice = Control(window, L"STATIC",
                    L"These terms apply only to the bundled Microsoft D3DX runtime.\r\n"
                    L"cod4-live remains GPLv3; these terms do not restrict the project's GPL code.",
                    0, 0, 0, 1, 1, 0, state->font, state->dpi);
                state->text = Control(window, L"EDIT", L"", 200, 0, 0, 1, 1,
                    WS_TABSTOP | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL, state->font, state->dpi);
                SendMessageW(state->text, EM_SETLIMITTEXT, 1024 * 1024, 0);
                SetWindowTextW(state->text, state->terms.c_str());
                state->agree = Control(window, L"BUTTON", L"I agree to these terms for the Microsoft D3DX runtime.",
                    201, 0, 0, 1, 1, WS_TABSTOP | BS_AUTOCHECKBOX | BS_MULTILINE, state->font, state->dpi);
                state->accept = Control(window, L"BUTTON", L"Accept and continue", IDOK, 0, 0, 1, 1,
                    WS_TABSTOP | BS_DEFPUSHBUTTON | BS_MULTILINE, state->font, state->dpi);
                state->cancel = Control(window, L"BUTTON", L"Cancel", IDCANCEL, 0, 0, 1, 1,
                    WS_TABSTOP, state->font, state->dpi);
                EnableWindow(state->accept, FALSE);
                state->Layout();
                return 0;
            }
            if (msg == WM_SIZE) { if (state->text) state->Layout(); return 0; }
            if (msg == WM_GETMINMAXINFO) {
                auto limits = reinterpret_cast<MINMAXINFO *>(l);
                const auto work = WorkArea(window);
                const int width = std::max(1, static_cast<int>(work.right - work.left));
                const int height = std::max(1, static_cast<int>(work.bottom - work.top));
                limits->ptMinTrackSize = {std::min(state->Scale(650), width), std::min(state->Scale(420), height)};
                limits->ptMaxTrackSize = {width, height};
                return 0;
            }
            if (msg == WM_COMMAND) {
                const auto id = LOWORD(w);
                if (id == 201) EnableWindow(state->accept, SendMessageW(state->agree, BM_GETCHECK, 0, 0) == BST_CHECKED);
                else if (id == IDOK && SendMessageW(state->agree, BM_GETCHECK, 0, 0) == BST_CHECKED) {
                    state->accepted = true; DestroyWindow(window);
                } else if (id == IDCANCEL) DestroyWindow(window);
                return 0;
            }
            if (msg == WM_CLOSE) { DestroyWindow(window); return 0; }
            return DefWindowProcW(window, msg, w, l);
        }
    } state;
    state.owner = owner; state.font = font; state.dpi = GetDpiForWindow(owner);
    state.terms = live::desktop::Wide(normalized);
    if (state.terms.empty()) return false;
    WNDCLASSW cls{};
    cls.hInstance = GetModuleHandleW(nullptr); cls.lpszClassName = L"COD4LiveRuntimeTerms";
    cls.lpfnWndProc = State::Proc; cls.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    cls.hbrBackground = live::desktop::DarkBrush();
    RegisterClassW(&cls);
    RECT ownerRect{}; GetWindowRect(owner, &ownerRect);
    const auto work = State::WorkArea(owner);
    const int width = std::min(state.Scale(780), std::max(1, static_cast<int>(work.right - work.left)));
    const int height = std::min(state.Scale(610), std::max(1, static_cast<int>(work.bottom - work.top)));
    const int x = std::clamp(static_cast<int>(ownerRect.left + (ownerRect.right - ownerRect.left - width) / 2),
        static_cast<int>(work.left), static_cast<int>(work.right) - width);
    const int y = std::clamp(static_cast<int>(ownerRect.top + (ownerRect.bottom - ownerRect.top - height) / 2),
        static_cast<int>(work.top), static_cast<int>(work.bottom) - height);
    const auto window = CreateWindowExW(WS_EX_CONTROLPARENT, cls.lpszClassName, L"Microsoft D3DX runtime terms",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME,
        x, y, width, height, owner, nullptr, cls.hInstance, &state);
    if (!window) return false;
    EnableWindow(owner, FALSE);
    ShowWindow(window, SW_SHOW); SetFocus(state.text);
    MSG message{};
    while (IsWindow(window)) {
        const auto result = GetMessageW(&message, nullptr, 0, 0);
        if (result <= 0) { if (!result) PostQuitMessage(static_cast<int>(message.wParam)); break; }
        if (!IsDialogMessageW(window, &message)) { TranslateMessage(&message); DispatchMessageW(&message); }
    }
    if (IsWindow(window)) DestroyWindow(window);
    EnableWindow(owner, TRUE); SetActiveWindow(owner);
    return state.accepted;
}
} // namespace live::setup
