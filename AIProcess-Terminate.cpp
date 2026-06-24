// AIProcess-Terminate.cpp
//
// Terminates AI-assistant processes on Windows 10/11.
// Runs fully in userland — no elevation required.
// Can only terminate processes owned by the current user.
//
// Build: see build.bat
//
// UI: Windows dialog — Up/Down navigate, Space toggle, Enter confirm, Esc abort.

#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "comctl32.lib")
// UAC manifest level set to asInvoker in build.bat via /MANIFESTUAC linker flag.


#include <windows.h>
#include <commctrl.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>

// ---------------------------------------------------------------------------
// Loads target keywords from <exedir>\AIProcess-Terminate.ini, section
// [Targets], key "keywords" (comma-separated list).
// Falls back to a built-in default list if the file or key is absent.
// ---------------------------------------------------------------------------
static std::vector<std::wstring> LoadTargetNames()
{
    // Build INI path: same directory as the running executable.
    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    wchar_t* lastSlash = wcsrchr(exePath, L'\\');
    if (lastSlash) *(lastSlash + 1) = L'\0';
    const std::wstring iniPath = std::wstring(exePath) + L"AIProcess-Terminate.ini";

    wchar_t raw[4096] = {};
    GetPrivateProfileStringW(L"Targets", L"keywords", L"",
                             raw, _countof(raw), iniPath.c_str());

    // Split on commas, trim whitespace from each token.
    std::vector<std::wstring> result;
    std::wstring token;
    for (wchar_t ch : std::wstring(raw))
    {
        if (ch == L',')
        {
            const size_t s = token.find_first_not_of(L" \t");
            const size_t e = token.find_last_not_of(L" \t");
            if (s != std::wstring::npos)
                result.push_back(token.substr(s, e - s + 1));
            token.clear();
        }
        else
        {
            token += ch;
        }
    }
    // Last (or only) token after the final comma.
    {
        const size_t s = token.find_first_not_of(L" \t");
        const size_t e = token.find_last_not_of(L" \t");
        if (s != std::wstring::npos)
            result.push_back(token.substr(s, e - s + 1));
    }

    // Built-in fallback — used when the INI file is absent or keywords is empty.
    if (result.empty())
        result = { L"claude", L"copilot", L"perplexity",
                   L"codex",  L"gemini",  L"manus", L"antigravity" };

    return result;
}

// ---------------------------------------------------------------------------
// One entry per tool category (one row in the dialog).
// ---------------------------------------------------------------------------
struct CategoryInfo {
    std::wstring keyword;   // e.g. L"claude"
    int          count;     // number of running instances
    bool         selected;  // checked in dialog = will be terminated
};

// ---------------------------------------------------------------------------
static std::wstring GetWin32ErrorMessage(DWORD code)
{
    LPWSTR buf = nullptr;
    DWORD  len = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, code,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&buf), 0, nullptr);

    std::wstring msg;
    if (len > 0 && buf != nullptr)
    {
        msg.assign(buf, static_cast<size_t>(len));
        while (!msg.empty() &&
               (msg.back() == L'\r' || msg.back() == L'\n' || msg.back() == L' '))
            msg.pop_back();
    }
    if (buf != nullptr) LocalFree(buf);
    return msg;
}

// ---------------------------------------------------------------------------
// Returns true when exeName contains keyword (case-insensitive substring).
// ---------------------------------------------------------------------------
static bool MatchesKeyword(const std::wstring& exeName, const std::wstring& keyword)
{
    std::wstring el(exeName.size(),   L'\0');
    std::wstring kl(keyword.size(),   L'\0');
    std::transform(exeName.begin(),  exeName.end(),  el.begin(),
        [](wchar_t c){ return static_cast<wchar_t>(::towlower(c)); });
    std::transform(keyword.begin(),  keyword.end(),  kl.begin(),
        [](wchar_t c){ return static_cast<wchar_t>(::towlower(c)); });
    return el.find(kl) != std::wstring::npos;
}

// ---------------------------------------------------------------------------
// Counts running instances per keyword.
// Returns only categories that have at least one running process.
// ---------------------------------------------------------------------------
static std::vector<CategoryInfo> ScanCategories(const std::vector<std::wstring>& targetNames)
{
    std::vector<CategoryInfo> cats;
    cats.reserve(targetNames.size());
    for (const auto& kw : targetNames)
    {
        CategoryInfo ci;
        ci.keyword  = kw;
        ci.count    = 0;
        ci.selected = true;
        cats.push_back(std::move(ci));
    }

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return cats; // return empty counts

    PROCESSENTRY32W pe = {};
    pe.dwSize = sizeof(pe);

    if (Process32FirstW(snap, &pe))
    {
        do
        {
            for (auto& cat : cats)
                if (MatchesKeyword(pe.szExeFile, cat.keyword))
                    ++cat.count;
        }
        while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);

    // Remove categories with no running processes.
    cats.erase(
        std::remove_if(cats.begin(), cats.end(),
            [](const CategoryInfo& c){ return c.count == 0; }),
        cats.end());

    return cats;
}

// ===========================================================================
// SELECTION DIALOG
// ===========================================================================

static const wchar_t* const DLG_CLASS_NAME = L"AIProcess-TerminateSelDlg";
static const int            IDC_LISTVIEW   = 100;
static const int            IDC_INSTR      = 101;

static inline HMENU ToMenuId(int id) noexcept
{
    return reinterpret_cast<HMENU>(static_cast<INT_PTR>(id));
}

struct DlgContext
{
    std::vector<CategoryInfo>& cats;
    bool                       confirmed;
    HWND                       hwndList;
    HWND                       hwndInstr;
    HWND                       hwndOK;
    HWND                       hwndAbort;

    explicit DlgContext(std::vector<CategoryInfo>& c)
        : cats(c), confirmed(false),
          hwndList(nullptr), hwndInstr(nullptr),
          hwndOK(nullptr),   hwndAbort(nullptr) {}
};

// ---------------------------------------------------------------------------
// Repositions and resizes all child controls to fill the current client area.
// All layout constants are scaled to the window's current DPI.
// Called from WM_CREATE, WM_SIZE, and WM_DPICHANGED.
// ---------------------------------------------------------------------------
static void LayoutControls(HWND hwnd, DlgContext* ctx)
{
    RECT rc = {};
    GetClientRect(hwnd, &rc);
    const int cw = rc.right;
    const int ch = rc.bottom;

    // Scale every pixel constant to the window's actual DPI.
    const UINT dpi = GetDpiForWindow(hwnd);
    const UINT d   = (dpi > 0) ? dpi : 96;
    const auto Sc  = [d](int v){ return MulDiv(v, static_cast<int>(d), 96); };

    const int M         = Sc(8);
    const int BTN_H     = Sc(28);
    const int INSTR_H   = Sc(22);
    const int lvH       = ch - 4 * M - INSTR_H - BTN_H;
    const int instrY    = M + lvH + M;
    const int btnY      = instrY + INSTR_H + M;

    const int BTN_ABORT_W = Sc(90);
    const int BTN_OK_W    = Sc(190);
    const int BTN_GAP     = Sc(6);
    const int abortX      = cw - M - BTN_ABORT_W;
    const int okX         = abortX - BTN_GAP - BTN_OK_W;

    HDWP hdwp = BeginDeferWindowPos(4);
    if (hdwp)
    {
        hdwp = DeferWindowPos(hdwp, ctx->hwndList,  nullptr,
                              M,      M,      cw - 2*M,    lvH,    SWP_NOZORDER);
        hdwp = DeferWindowPos(hdwp, ctx->hwndInstr, nullptr,
                              M,      instrY, cw - 2*M,    INSTR_H, SWP_NOZORDER);
        hdwp = DeferWindowPos(hdwp, ctx->hwndOK,    nullptr,
                              okX,    btnY,   BTN_OK_W,    BTN_H,  SWP_NOZORDER);
        hdwp = DeferWindowPos(hdwp, ctx->hwndAbort, nullptr,
                              abortX, btnY,   BTN_ABORT_W, BTN_H,  SWP_NOZORDER);
        EndDeferWindowPos(hdwp);
    }

    // Force a clean repaint so static controls don't smear on resize.
    InvalidateRect(hwnd, nullptr, TRUE);

    // Keep the ListView column flush with the new width.
    if (ctx->hwndList)
    {
        const int colW = cw - 2*M - GetSystemMetrics(SM_CXVSCROLL) - 4;
        ListView_SetColumnWidth(ctx->hwndList, 0, max(colW, 80));
    }
}

// ---------------------------------------------------------------------------
static LRESULT CALLBACK DlgWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    DlgContext* ctx = reinterpret_cast<DlgContext*>(
        GetWindowLongPtr(hwnd, GWLP_USERDATA));

    switch (msg)
    {
    case WM_CREATE:
    {
        const LPCREATESTRUCT cs = reinterpret_cast<LPCREATESTRUCT>(lParam);
        ctx = reinterpret_cast<DlgContext*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(ctx));

        // ── ListView ─────────────────────────────────────────────────────
        ctx->hwndList = CreateWindowEx(
            WS_EX_CLIENTEDGE, WC_LISTVIEW, L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP |
            LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SINGLESEL,
            0, 0, 0, 0,
            hwnd, ToMenuId(IDC_LISTVIEW), cs->hInstance, nullptr);

        ListView_SetExtendedListViewStyle(ctx->hwndList,
            LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

        // Single column — width set by LayoutControls.
        LVCOLUMN col = {};
        col.mask    = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        col.cx      = 100;
        col.pszText = const_cast<LPWSTR>(L"Tool");
        ListView_InsertColumn(ctx->hwndList, 0, &col);

        // Rows — one per category
        LVITEM item = {};
        item.mask = LVIF_TEXT;
        for (int i = 0; i < static_cast<int>(ctx->cats.size()); ++i)
        {
            const CategoryInfo& ci = ctx->cats[static_cast<size_t>(i)];

            wchar_t label[64] = {};
            swprintf_s(label, _countof(label), L"%s (%d)", ci.keyword.c_str(), ci.count);

            item.iItem    = i;
            item.iSubItem = 0;
            item.pszText  = label;
            ListView_InsertItem(ctx->hwndList, &item);

            ListView_SetCheckState(ctx->hwndList, static_cast<UINT>(i),
                ci.selected ? TRUE : FALSE);
        }

        if (!ctx->cats.empty())
        {
            ListView_SetItemState(ctx->hwndList, 0,
                LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            ListView_EnsureVisible(ctx->hwndList, 0, FALSE);
        }

        // ── Instruction bar ───────────────────────────────────────────────
        ctx->hwndInstr = CreateWindowEx(0, L"STATIC",
            L"Up/Dn  Navigate    SPACE  Toggle    ENTER  Confirm    ESC  Abort",
            WS_CHILD | WS_VISIBLE | SS_CENTER,
            0, 0, 0, 0,
            hwnd, ToMenuId(IDC_INSTR), cs->hInstance, nullptr);

        // ── Buttons ───────────────────────────────────────────────────────
        ctx->hwndOK = CreateWindowEx(0, L"BUTTON", L"Terminate Selected",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
            0, 0, 0, 0,
            hwnd, ToMenuId(IDOK), cs->hInstance, nullptr);

        ctx->hwndAbort = CreateWindowEx(0, L"BUTTON", L"Abort",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            0, 0, 0, 0,
            hwnd, ToMenuId(IDCANCEL), cs->hInstance, nullptr);

        LayoutControls(hwnd, ctx);
        SetFocus(ctx->hwndList);
        return 0;
    }

    case WM_COMMAND:
    {
        if (ctx == nullptr) break;
        const int cmdId = static_cast<int>(LOWORD(wParam));

        if (cmdId == IDOK)
        {
            const int n = ListView_GetItemCount(ctx->hwndList);
            for (int i = 0; i < n; ++i)
                ctx->cats[static_cast<size_t>(i)].selected =
                    (ListView_GetCheckState(ctx->hwndList, static_cast<UINT>(i)) != 0);
            ctx->confirmed = true;
            DestroyWindow(hwnd);
        }
        else if (cmdId == IDCANCEL)
        {
            DestroyWindow(hwnd);
        }
        return 0;
    }

    case WM_NOTIFY:
    {
        if (ctx == nullptr) break;
        const NMHDR* nmh = reinterpret_cast<const NMHDR*>(lParam);
        if (nmh->idFrom == static_cast<UINT_PTR>(IDC_LISTVIEW) &&
            nmh->code   == static_cast<UINT>(LVN_KEYDOWN))
        {
            const NMLVKEYDOWN* nkd = reinterpret_cast<const NMLVKEYDOWN*>(lParam);
            if (nkd->wVKey == VK_RETURN)
                SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDOK,     BN_CLICKED), 0);
            else if (nkd->wVKey == VK_ESCAPE)
                SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDCANCEL, BN_CLICKED), 0);
        }
        return 0;
    }

    case WM_SIZE:
        if (ctx && wParam != SIZE_MINIMIZED)
            LayoutControls(hwnd, ctx);
        return 0;

    case WM_GETMINMAXINFO:
    {
        const UINT dpi = GetDpiForWindow(hwnd);
        const UINT d   = (dpi > 0) ? dpi : 96;
        MINMAXINFO* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
        mmi->ptMinTrackSize.x = MulDiv(400, static_cast<int>(d), 96);
        mmi->ptMinTrackSize.y = MulDiv(280, static_cast<int>(d), 96);
        return 0;
    }

    case WM_DPICHANGED:
    {
        // The OS supplies the suggested window rect already scaled to the new DPI.
        const RECT* r = reinterpret_cast<const RECT*>(lParam);
        SetWindowPos(hwnd, nullptr,
                     r->left, r->top,
                     r->right  - r->left,
                     r->bottom - r->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        // LayoutControls is called automatically by the WM_SIZE that follows.
        return 0;
    }

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ---------------------------------------------------------------------------
static bool ShowSelectionDialog(std::vector<CategoryInfo>& cats)
{
    HINSTANCE hInst = GetModuleHandle(nullptr);

    INITCOMMONCONTROLSEX icc = {};
    icc.dwSize = sizeof(icc);
    icc.dwICC  = ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icc);

    WNDCLASSEX wc    = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = DlgWndProc;
    wc.hInstance     = hInst;
    wc.hbrBackground = reinterpret_cast<HBRUSH>(static_cast<INT_PTR>(COLOR_BTNFACE + 1));
    wc.lpszClassName = DLG_CLASS_NAME;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon         = LoadIconW(hInst, MAKEINTRESOURCEW(101));
    wc.hIconSm       = LoadIconW(hInst, MAKEINTRESOURCEW(101));
    RegisterClassEx(&wc);

    // Scale default size to the system DPI so the window isn't tiny on HiDPI.
    const UINT sysDpi  = GetDpiForSystem();
    const UINT d       = (sysDpi > 0) ? sysDpi : 96;
    const int CLIENT_W = MulDiv(640, static_cast<int>(d), 96);
    const int CLIENT_H = MulDiv(460, static_cast<int>(d), 96);

    const DWORD style   = WS_OVERLAPPEDWINDOW;   // includes thick frame + maximize box
    const DWORD exStyle = 0;
    RECT adj = { 0, 0, CLIENT_W, CLIENT_H };
    AdjustWindowRectEx(&adj, style, FALSE, exStyle);
    const int winW = adj.right  - adj.left;
    const int winH = adj.bottom - adj.top;

    const int screenW = GetSystemMetrics(SM_CXSCREEN);
    const int screenH = GetSystemMetrics(SM_CYSCREEN);

    DlgContext ctx(cats);

    HWND hwnd = CreateWindowEx(
        exStyle, DLG_CLASS_NAME,
        L"AI Process-Terminate -- Select tools to terminate",
        style,
        (screenW - winW) / 2, (screenH - winH) / 2, winW, winH,
        nullptr, nullptr, hInst, &ctx);

    if (hwnd == nullptr) return false;

    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);
    UpdateWindow(hwnd);

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0) > 0)
    {
        if (!IsDialogMessage(hwnd, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    UnregisterClass(DLG_CLASS_NAME, hInst);
    return ctx.confirmed;
}

// ===========================================================================
int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR lpCmdLine, int)
// ===========================================================================
{
    // Check for -auto / /auto flag: terminate all without showing the dialog.
    const bool autoMode = (wcsstr(lpCmdLine, L"-auto") != nullptr ||
                           wcsstr(lpCmdLine, L"/auto") != nullptr);

    // ── 1. SCAN ───────────────────────────────────────────────────────────────
    const std::vector<std::wstring> targetNames = LoadTargetNames();
    std::vector<CategoryInfo> cats = ScanCategories(targetNames);

    if (cats.empty())
    {
        if (!autoMode)
            MessageBoxW(nullptr, L"No AI processes found running.",
                        L"AI Process-Terminate", MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    // ── 2. SELECTION DIALOG (skipped in auto mode) ────────────────────────────
    if (!autoMode)
    {
        if (!ShowSelectionDialog(cats))
            return 0;
    }

    // Collect selected keywords (in auto mode all categories are pre-selected).
    std::vector<std::wstring> selectedKeywords;
    for (const auto& c : cats)
        if (c.selected)
            selectedKeywords.push_back(c.keyword);

    if (selectedKeywords.empty())
        return 0;

    // ── 3. TERMINATE LOOP (multi-instance drain, keyword-based matching) ───────────
    std::wostringstream errors;
    int       exitCode   = 0;
    bool      foundAny   = true;
    int       iterations = 0;
    const int MAX_IT     = 20;

    while (foundAny && iterations < MAX_IT)
    {
        foundAny = false;
        ++iterations;

        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE) break;

        PROCESSENTRY32W pe = {};
        pe.dwSize = sizeof(pe);

        if (Process32FirstW(snap, &pe))
        {
            do
            {
                bool match = false;
                for (const auto& kw : selectedKeywords)
                    if (MatchesKeyword(pe.szExeFile, kw)) { match = true; break; }
                if (!match) continue;

                foundAny              = true;
                const DWORD      pid  = pe.th32ProcessID;
                const std::wstring nm = pe.szExeFile;

                HANDLE hProc = OpenProcess(
                    PROCESS_TERMINATE | PROCESS_QUERY_LIMITED_INFORMATION,
                    FALSE, pid);

                if (hProc == nullptr)
                {
                    const DWORD err  = GetLastError();
                    HANDLE      hChk = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
                    if (hChk != nullptr)
                    {
                        CloseHandle(hChk);
                        errors << L"[ERROR] Failed to terminate " << nm
                               << L" (PID " << pid << L"): "
                               << GetWin32ErrorMessage(err)
                               << L" (" << err << L")\n";
                        exitCode = 1;
                    }
                    continue;
                }

                if (!TerminateProcess(hProc, 1))
                {
                    const DWORD err      = GetLastError();
                    DWORD       procExit = 0;
                    if (GetExitCodeProcess(hProc, &procExit) && procExit == STILL_ACTIVE)
                    {
                        errors << L"[ERROR] Failed to terminate " << nm
                               << L" (PID " << pid << L"): "
                               << GetWin32ErrorMessage(err)
                               << L" (" << err << L")\n";
                        exitCode = 1;
                    }
                }
                else
                {
                    WaitForSingleObject(hProc, 1000);
                }

                CloseHandle(hProc);
            }
            while (Process32NextW(snap, &pe));
        }

        CloseHandle(snap);
        if (foundAny) Sleep(200);
    }

    if (foundAny && iterations >= MAX_IT)
    {
        errors << L"[ERROR] Max iterations reached. Some matching processes may still be running.\n"
               << L"        This usually means a selected process is respawning or cannot finish terminating.\n";
        exitCode = 1;
    }

    if (exitCode != 0)
        MessageBoxW(nullptr, errors.str().c_str(), L"AI Process-Terminate — Errors", MB_OK | MB_ICONERROR);

    return exitCode;
}
