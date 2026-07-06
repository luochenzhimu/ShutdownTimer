/*
 * 定时关机工具 - 单实例版
 * 编译: Visual Studio (Unicode, 子系统 Windows)
 * 说明: 仅允许一个实例运行，再次启动时自动弹出已有窗口
 */

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <commctrl.h>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <ctime>

#pragma comment(lib, "comctl32.lib")
#pragma comment(linker, "\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

 // 常量
#define IDT_COUNTDOWN       1001
#define IDT_BLINK           1002
#define WM_TRAYICON         (WM_APP + 1)
#define ID_TRAY_EXIT        2001
#define ID_TRAY_SHOW        2002
#define ID_TRAY_RESTORE     2003
#define ID_TRAY_CANCEL      2004
#define IDM_FULLSCREEN      2005

// 菜单ID
#define IDM_PROGRAM_SHOW    4010
#define IDM_PROGRAM_CANCEL  4011
#define IDM_PROGRAM_EXIT    4012
#define IDM_ABOUT           4013

#define IDC_RADIO_1MIN      3001
#define IDC_RADIO_5MIN      3002
#define IDC_RADIO_10MIN     3003
#define IDC_RADIO_30MIN     3004
#define IDC_RADIO_1H        3005
#define IDC_RADIO_2H        3012
#define IDC_RADIO_3H        3006
#define IDC_RADIO_5H        3016
#define IDC_RADIO_CUSTOM    3007
#define IDC_RADIO_DATETIME  3013
#define IDC_DTPICKER        3008
#define IDC_DATE_PICKER     3014
#define IDC_TIME_PICKER     3015
#define IDC_BTN_START       3009
#define IDC_BTN_CANCEL      3010
#define IDC_LABEL_TITLE     3011
#define IDC_COMBO_ACTION    3017
#define IDC_LABEL_ACTION    3018

// 全局变量
HINSTANCE g_hInst;
HWND g_hMainWnd = NULL;
HWND g_hCountWnd = NULL;
NOTIFYICONDATA g_nid = {};
int g_remainingSec = 0;
int g_totalSec = 0;
bool g_countdownActive = false;
bool g_fullscreen = false;
RECT g_normalRect = {};
float g_dpiScale = 1.0f;
HICON g_hAppIcon = NULL;
bool g_blink = true;
int g_blinkPhase = 0;
int g_shutdownAction = 0;   // 0=关机, 1=重启

// 函数声明
LRESULT CALLBACK MainWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK CountdownWndProc(HWND, UINT, WPARAM, LPARAM);
BOOL InitInstance(HINSTANCE, int);
void AddTrayIcon(HWND);
void RemoveTrayIcon();
void DoShutdown();
BOOL SetShutdownPrivilege();
void StartCountdown(int seconds);
void StopCountdown();
void ShowCountdownWindow();
void UpdateCountdownWindow();
void InitDPIAwareness();
HFONT CreateArtFont(int pixelHeight);
HFONT CreateYaHeiFont(int pixelHeight, BOOL bold);
void ToggleFullscreen(HWND hwnd);
void LayoutCountdownControls(HWND hwnd, int width, int height);
HICON CreateAppIcon(int iconSize);
void LayoutMainControls(HWND hwnd, int clientWidth, int clientHeight);
std::wstring SecondsToText(int seconds);
void DrawBlingEffect(HDC hdc, RECT& textRect);
void UpdateCancelButtonText();
void UpdateMainCancelButtonText();
void UpdateProgramMenu(HWND hwnd);

// 自绘高清图标 (256x256)
HICON CreateAppIcon(int iconSize)
{
    int cx = iconSize, cy = iconSize;
    HDC hdc = GetDC(NULL);
    HDC hdcColor = CreateCompatibleDC(hdc);
    HBITMAP hbmColor = CreateCompatibleBitmap(hdc, cx, cy);
    HBITMAP hbmOldColor = (HBITMAP)SelectObject(hdcColor, hbmColor);
    RECT rc = { 0, 0, cx, cy };
    FillRect(hdcColor, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
    int penWidth = max(2, cx / 16);
    HPEN hPenRed = CreatePen(PS_SOLID, penWidth, RGB(230, 50, 50));
    HPEN hOldPen = (HPEN)SelectObject(hdcColor, hPenRed);
    SelectObject(hdcColor, GetStockObject(NULL_BRUSH));
    int margin = penWidth + 1;
    Ellipse(hdcColor, margin, margin, cx - margin, cy - margin);
    int cxCenter = cx / 2, cyCenter = cy / 2;
    MoveToEx(hdcColor, cxCenter, margin, NULL);
    LineTo(hdcColor, cxCenter, cyCenter - margin / 2);
    SelectObject(hdcColor, hOldPen); DeleteObject(hPenRed);
    HDC hdcMask = CreateCompatibleDC(hdc);
    HBITMAP hbmMask = CreateBitmap(cx, cy, 1, 1, NULL);
    HBITMAP hbmOldMask = (HBITMAP)SelectObject(hdcMask, hbmMask);
    FillRect(hdcMask, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
    HPEN hPenWhite = CreatePen(PS_SOLID, penWidth, RGB(255, 255, 255));
    HPEN hOldPen2 = (HPEN)SelectObject(hdcMask, hPenWhite);
    SelectObject(hdcMask, GetStockObject(NULL_BRUSH));
    Ellipse(hdcMask, margin, margin, cx - margin, cy - margin);
    MoveToEx(hdcMask, cxCenter, margin, NULL);
    LineTo(hdcMask, cxCenter, cyCenter - margin / 2);
    SelectObject(hdcMask, hOldPen2); DeleteObject(hPenWhite);
    SelectObject(hdcColor, hbmOldColor); SelectObject(hdcMask, hbmOldMask);
    DeleteDC(hdcColor); DeleteDC(hdcMask); ReleaseDC(NULL, hdc);
    ICONINFO ii = {};
    ii.fIcon = TRUE; ii.hbmColor = hbmColor; ii.hbmMask = hbmMask;
    HICON hIcon = CreateIconIndirect(&ii);
    DeleteObject(hbmColor); DeleteObject(hbmMask);
    return hIcon;
}

void InitDPIAwareness()
{
    HMODULE hShcore = LoadLibrary(L"shcore.dll");
    if (hShcore) {
        typedef HRESULT(WINAPI* SetProcessDpiAwareness_t)(int);
        auto pFunc = (SetProcessDpiAwareness_t)GetProcAddress(hShcore, "SetProcessDpiAwareness");
        if (pFunc) { pFunc(2); FreeLibrary(hShcore); return; }
        FreeLibrary(hShcore);
    }
    SetProcessDPIAware();
}

HFONT CreateArtFont(int pixelHeight)
{
    int scaled = (int)(pixelHeight * g_dpiScale);
    return CreateFontW(scaled, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Microsoft YaHei");
}

HFONT CreateYaHeiFont(int pixelHeight, BOOL bold)
{
    int scaled = (int)(pixelHeight * g_dpiScale);
    return CreateFontW(scaled, 0, 0, 0,
        bold ? FW_BOLD : FW_NORMAL,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        VARIABLE_PITCH, L"Microsoft YaHei");
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
    // 单实例检测
    HANDLE hMutex = CreateMutexW(NULL, TRUE, L"Local\\ShutdownTimerTool_Mutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        // 已有实例在运行，找到其主窗口并恢复
        HWND hExist = FindWindowW(L"ShutdownTimerMain", NULL);
        if (hExist)
        {
            ShowWindow(hExist, SW_SHOW);
            if (IsIconic(hExist))
                ShowWindow(hExist, SW_RESTORE);
            SetForegroundWindow(hExist);
        }
        CloseHandle(hMutex);
        return 0;
    }

    InitDPIAwareness();
    HDC hdc = GetDC(NULL);
    int dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(NULL, hdc);
    g_dpiScale = dpiX / 96.0f;

    INITCOMMONCONTROLSEX icex = { sizeof(INITCOMMONCONTROLSEX), ICC_PROGRESS_CLASS | ICC_DATE_CLASSES };
    InitCommonControlsEx(&icex);
    SetShutdownPrivilege();

    g_hAppIcon = CreateAppIcon(256);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"ShutdownTimerMain";
    wc.hIcon = g_hAppIcon;
    wc.hIconSm = g_hAppIcon;
    RegisterClassExW(&wc);

    WNDCLASSEXW wcCount = {};
    wcCount.cbSize = sizeof(WNDCLASSEXW);
    wcCount.style = CS_HREDRAW | CS_VREDRAW;
    wcCount.lpfnWndProc = CountdownWndProc;
    wcCount.hInstance = hInstance;
    wcCount.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcCount.hbrBackground = NULL;
    wcCount.lpszClassName = L"CountdownWindow";
    wcCount.hIcon = g_hAppIcon;
    RegisterClassExW(&wcCount);

    g_hInst = hInstance;
    if (!InitInstance(hInstance, nCmdShow)) return FALSE;

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    CloseHandle(hMutex);
    if (g_hAppIcon) DestroyIcon(g_hAppIcon);
    return (int)msg.wParam;
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    int width = (int)(750 * g_dpiScale);
    int height = (int)(600 * g_dpiScale);
    int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
    g_hMainWnd = CreateWindowExW(0, L"ShutdownTimerMain", L"定时关机",
        WS_OVERLAPPEDWINDOW, x, y, width, height,
        NULL, NULL, hInstance, NULL);
    if (!g_hMainWnd) return FALSE;
    ShowWindow(g_hMainWnd, nCmdShow);
    UpdateWindow(g_hMainWnd);
    return TRUE;
}

BOOL SetShutdownPrivilege()
{
    HANDLE hToken; TOKEN_PRIVILEGES tkp;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
        return FALSE;
    LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, &tkp.Privileges[0].Luid);
    tkp.PrivilegeCount = 1;
    tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    BOOL ret = AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, NULL, 0);
    CloseHandle(hToken);
    return ret && GetLastError() == ERROR_SUCCESS;
}

void DoShutdown()
{
    SetShutdownPrivilege();
    UINT flags = (g_shutdownAction == 0) ? EWX_SHUTDOWN : EWX_REBOOT;
    ExitWindowsEx(flags, SHTDN_REASON_MAJOR_OTHER);
}

void AddTrayIcon(HWND hwnd)
{
    g_nid.cbSize = sizeof(NOTIFYICONDATA);
    g_nid.hWnd = hwnd; g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = g_hAppIcon;
    wcscpy_s(g_nid.szTip, L"定时关机");
    Shell_NotifyIcon(NIM_ADD, &g_nid);
}
void RemoveTrayIcon() { Shell_NotifyIcon(NIM_DELETE, &g_nid); }

void UpdateCancelButtonText()
{
    if (g_hCountWnd) {
        HWND hCancelBtn = GetDlgItem(g_hCountWnd, 103);
        if (hCancelBtn) {
            SetWindowTextW(hCancelBtn, (g_shutdownAction == 1) ? L"取消重启" : L"取消关机");
        }
    }
}

void UpdateMainCancelButtonText()
{
    if (g_hMainWnd) {
        HWND hBtnCancel = GetDlgItem(g_hMainWnd, IDC_BTN_CANCEL);
        if (hBtnCancel) {
            SetWindowTextW(hBtnCancel, (g_shutdownAction == 1) ? L"取消重启" : L"取消关机");
        }
    }
}

void UpdateProgramMenu(HWND hwnd)
{
    HMENU hMenu = GetMenu(hwnd);
    if (!hMenu) return;
    HMENU hProgMenu = GetSubMenu(hMenu, 0);
    UINT stateShow = g_countdownActive ? MF_ENABLED : MF_GRAYED;
    UINT stateCancel = g_countdownActive ? MF_ENABLED : MF_GRAYED;
    WCHAR cancelText[20];
    wcscpy_s(cancelText, (g_shutdownAction == 1) ? L"取消重启" : L"取消关机");
    EnableMenuItem(hProgMenu, IDM_PROGRAM_SHOW, MF_BYCOMMAND | stateShow);
    ModifyMenu(hProgMenu, IDM_PROGRAM_CANCEL, MF_BYCOMMAND | MF_STRING | stateCancel, IDM_PROGRAM_CANCEL, cancelText);
}

void StartCountdown(int seconds)
{
    if (seconds <= 0) return;
    g_remainingSec = seconds; g_totalSec = seconds;
    g_countdownActive = true; g_fullscreen = false;
    g_blink = true; g_blinkPhase = 0;

    if (g_hCountWnd == NULL) {
        int width = (int)(600 * g_dpiScale);
        int height = (int)(420 * g_dpiScale);
        int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
        int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
        DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
        g_hCountWnd = CreateWindowExW(
            WS_EX_LAYERED,
            L"CountdownWindow", L"关机倒计时", style,
            x, y, width, height, NULL, NULL, g_hInst, NULL);
        SetLayeredWindowAttributes(g_hCountWnd, 0, 210, LWA_ALPHA);
        LayoutCountdownControls(g_hCountWnd, width, height);
    }
    ShowWindow(g_hCountWnd, SW_SHOW);
    UpdateCancelButtonText();
    SetTimer(g_hMainWnd, IDT_COUNTDOWN, 1000, NULL);
    if (g_remainingSec <= 10) SetTimer(g_hMainWnd, IDT_BLINK, 500, NULL);
    UpdateCountdownWindow();
    UpdateProgramMenu(g_hMainWnd);
}

void StopCountdown()
{
    if (!g_countdownActive) return;
    g_countdownActive = false;
    KillTimer(g_hMainWnd, IDT_COUNTDOWN); KillTimer(g_hMainWnd, IDT_BLINK);
    if (g_hCountWnd) { DestroyWindow(g_hCountWnd); g_hCountWnd = NULL; }
    UpdateProgramMenu(g_hMainWnd);
}

void ShowCountdownWindow()
{
    if (g_countdownActive && g_hCountWnd) {
        // 如果最小化则恢复，否则直接显示并前置
        if (IsIconic(g_hCountWnd))
            ShowWindow(g_hCountWnd, SW_RESTORE);
        else
            ShowWindow(g_hCountWnd, SW_SHOW);
        SetForegroundWindow(g_hCountWnd);
        BringWindowToTop(g_hCountWnd);
        InvalidateRect(g_hCountWnd, NULL, TRUE);
    }
}

std::wstring SecondsToText(int seconds)
{
    int d = seconds / 86400; seconds %= 86400;
    int h = seconds / 3600; seconds %= 3600;
    int m = seconds / 60; int s = seconds % 60;
    std::wstring text;
    if (d > 0) text += std::to_wstring(d) + L"天";
    if (h > 0 || (d > 0 && (m > 0 || s > 0))) text += std::to_wstring(h) + L"小时";
    if (m > 0 || ((d > 0 || h > 0) && s > 0)) text += std::to_wstring(m) + L"分钟";
    text += std::to_wstring(s) + L"秒后";
    text += (g_shutdownAction == 1) ? L"重启" : L"关机";
    return text;
}

void UpdateCountdownWindow()
{
    if (!g_hCountWnd || !IsWindowVisible(g_hCountWnd)) return;
    HWND hProgress = GetDlgItem(g_hCountWnd, 102);
    if (hProgress) {
        SendMessage(hProgress, PBM_SETRANGE, 0, MAKELPARAM(0, g_totalSec));
        SendMessage(hProgress, PBM_SETPOS, g_totalSec - g_remainingSec, 0);
    }
    InvalidateRect(g_hCountWnd, NULL, TRUE);
}

void ToggleFullscreen(HWND hwnd)
{
    if (g_fullscreen) {
        SetWindowLong(hwnd, GWL_STYLE, GetWindowLong(hwnd, GWL_STYLE) | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
        SetWindowPos(hwnd, NULL, g_normalRect.left, g_normalRect.top,
            g_normalRect.right - g_normalRect.left, g_normalRect.bottom - g_normalRect.top,
            SWP_NOZORDER | SWP_FRAMECHANGED);
        g_fullscreen = false;
    }
    else {
        GetWindowRect(hwnd, &g_normalRect);
        SetWindowLong(hwnd, GWL_STYLE, GetWindowLong(hwnd, GWL_STYLE) & ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX));
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), SWP_FRAMECHANGED);
        g_fullscreen = true;
    }
    RECT rc; GetClientRect(hwnd, &rc);
    LayoutCountdownControls(hwnd, rc.right - rc.left, rc.bottom - rc.top);
    InvalidateRect(hwnd, NULL, TRUE);
}

void DrawBlingEffect(HDC hdc, RECT& textRect)
{
    int cx = textRect.left + 20 + (g_blinkPhase * 13) % (textRect.right - textRect.left - 40);
    int cy = textRect.top + 20 + (g_blinkPhase * 7) % (textRect.bottom - textRect.top - 40);
    int size = 6 + (g_blinkPhase % 5);
    HBRUSH br = CreateSolidBrush(RGB(255, 255, 150));
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(255, 255, 200));
    SelectObject(hdc, br); SelectObject(hdc, pen);
    Ellipse(hdc, cx - size, cy - size, cx + size, cy + size);
    cx = textRect.right - 20 - (g_blinkPhase * 11) % (textRect.right - textRect.left - 40);
    cy = textRect.bottom - 20 - (g_blinkPhase * 9) % (textRect.bottom - textRect.top - 40);
    DeleteObject(br); DeleteObject(pen);
    br = CreateSolidBrush(RGB(255, 255, 200));
    SelectObject(hdc, br);
    Ellipse(hdc, cx - size, cy - size, cx + size, cy + size);
    DeleteObject(br);
}

void LayoutCountdownControls(HWND hwnd, int width, int height)
{
    HWND hProgress = GetDlgItem(hwnd, 102);
    HWND hCancelBtn = GetDlgItem(hwnd, 103);
    int titleBarHeight = GetSystemMetrics(SM_CYCAPTION);
    int usableH = height - titleBarHeight;
    int margin = width / 20;
    int textAreaHeight = usableH * 50 / 100;
    int progHeight = usableH / 12;
    int btnHeight = usableH / 8;
    int btnWidth = width / 3;
    MoveWindow(hProgress, margin, titleBarHeight + textAreaHeight + margin,
        width - 2 * margin, progHeight, TRUE);
    int btnY = titleBarHeight + textAreaHeight + margin + progHeight + margin;
    if (btnY + btnHeight > height) btnY = height - btnHeight - margin;
    MoveWindow(hCancelBtn, (width - btnWidth) / 2, btnY, btnWidth, btnHeight, TRUE);
}

void LayoutMainControls(HWND hwnd, int clientWidth, int clientHeight)
{
    HWND hTitle = GetDlgItem(hwnd, IDC_LABEL_TITLE);
    HWND hLabelAction = GetDlgItem(hwnd, IDC_LABEL_ACTION);
    HWND hCombo = GetDlgItem(hwnd, IDC_COMBO_ACTION);
    HWND hRadio1 = GetDlgItem(hwnd, IDC_RADIO_1MIN);
    HWND hRadio5 = GetDlgItem(hwnd, IDC_RADIO_5MIN);
    HWND hRadio10 = GetDlgItem(hwnd, IDC_RADIO_10MIN);
    HWND hRadio30 = GetDlgItem(hwnd, IDC_RADIO_30MIN);
    HWND hRadio1h = GetDlgItem(hwnd, IDC_RADIO_1H);
    HWND hRadio2h = GetDlgItem(hwnd, IDC_RADIO_2H);
    HWND hRadio3h = GetDlgItem(hwnd, IDC_RADIO_3H);
    HWND hRadio5h = GetDlgItem(hwnd, IDC_RADIO_5H);
    HWND hRadioCustom = GetDlgItem(hwnd, IDC_RADIO_CUSTOM);
    HWND hRadioDateTime = GetDlgItem(hwnd, IDC_RADIO_DATETIME);
    HWND hDTPicker = GetDlgItem(hwnd, IDC_DTPICKER);
    HWND hDatePicker = GetDlgItem(hwnd, IDC_DATE_PICKER);
    HWND hTimePicker = GetDlgItem(hwnd, IDC_TIME_PICKER);
    HWND hBtnStart = GetDlgItem(hwnd, IDC_BTN_START);
    HWND hBtnCancel = GetDlgItem(hwnd, IDC_BTN_CANCEL);
    if (!hRadio1) return;

    int radioW = 250, radioH = 60;
    int btnW = 260, btnH = 70;
    int hGap = 77;   // 左右列间距
    int vGap = 24;
    int top = 12;
    int titleW = clientWidth - 40, titleH = 90;
    int labelActionW = 200, labelActionH = 56;
    int comboW = 160, comboH = 200;
    int comboVisibleH = 28;
    int actionGap = 10;
    int totalActionW = labelActionW + actionGap + comboW;
    int col1 = clientWidth / 2 - radioW - hGap / 2;
    int col2 = clientWidth / 2 + hGap / 2;

    MoveWindow(hTitle, (clientWidth - titleW) / 2, top, titleW, titleH, TRUE);
    int y = top + titleH + 15;

    MoveWindow(hRadio1, col1, y, radioW, radioH, TRUE);
    MoveWindow(hRadio5, col2, y, radioW, radioH, TRUE);
    y += radioH + vGap;
    MoveWindow(hRadio10, col1, y, radioW, radioH, TRUE);
    MoveWindow(hRadio30, col2, y, radioW, radioH, TRUE);
    y += radioH + vGap;
    MoveWindow(hRadio1h, col1, y, radioW, radioH, TRUE);
    MoveWindow(hRadio2h, col2, y, radioW, radioH, TRUE);
    y += radioH + vGap;
    MoveWindow(hRadio3h, col1, y, radioW, radioH, TRUE);
    MoveWindow(hRadio5h, col2, y, radioW, radioH, TRUE);
    y += radioH + vGap;

    MoveWindow(hRadioDateTime, col1, y, radioW, radioH, TRUE);
    MoveWindow(hRadioCustom, col2, y, radioW, radioH, TRUE);
    y += radioH + 12;

    int expandY = y;
    int expandHeight = 48;
    int expandLeft = col1;
    int expandRight = col2 + radioW;
    int expandWidth = expandRight - expandLeft;

    int dateWidth = expandWidth * 55 / 100;
    int timeWidth = expandWidth - dateWidth - 10;
    MoveWindow(hDatePicker, expandLeft, expandY, dateWidth, expandHeight, TRUE);
    MoveWindow(hTimePicker, expandLeft + dateWidth + 10, expandY, timeWidth, expandHeight, TRUE);
    int customTimeWidth = expandRight - col2;
    MoveWindow(hDTPicker, col2, expandY, customTimeWidth, expandHeight, TRUE);

    y = expandY + expandHeight + 20;
    int actionX = (clientWidth - totalActionW) / 2;
    MoveWindow(hLabelAction, actionX, y + 3, labelActionW, labelActionH, TRUE);
    MoveWindow(hCombo, actionX + labelActionW + actionGap, y, comboW, comboH, TRUE);
    y += comboVisibleH + 90;
    int totalBtnW = btnW * 2 + 50;
    int btnX = (clientWidth - totalBtnW) / 2;
    MoveWindow(hBtnStart, btnX, y, btnW, btnH, TRUE);
    MoveWindow(hBtnCancel, btnX + btnW + 50, y, btnW, btnH, TRUE);
}

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static HWND hRadio1, hRadio5, hRadio10, hRadio30, hRadio1h, hRadio2h, hRadio3h, hRadio5h,
        hRadioCustom, hRadioDateTime;
    static HWND hDTPicker, hDatePicker, hTimePicker, hBtnStart, hBtnCancel, hTitle, hCombo, hLabelAction;
    static HFONT hFont = NULL, hTitleFont = NULL, hDateFont = NULL;

    switch (msg) {
    case WM_CREATE:
    {
        hFont = CreateYaHeiFont(26, FALSE);
        hTitleFont = CreateYaHeiFont(34, TRUE);
        hDateFont = CreateYaHeiFont(23, FALSE);

        // 创建菜单栏
        HMENU hMenuBar = CreateMenu();
        HMENU hProgMenu = CreatePopupMenu();
        AppendMenu(hProgMenu, MF_STRING, IDM_PROGRAM_SHOW, L"倒计时页面");
        AppendMenu(hProgMenu, MF_STRING, IDM_PROGRAM_CANCEL, L"取消关机");
        AppendMenu(hProgMenu, MF_SEPARATOR, 0, NULL);
        AppendMenu(hProgMenu, MF_STRING, IDM_PROGRAM_EXIT, L"退出");
        AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hProgMenu, L"程序");
        HMENU hAboutMenu = CreatePopupMenu();
        AppendMenu(hAboutMenu, MF_STRING, IDM_ABOUT, L"关于定时关机");
        AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hAboutMenu, L"关于");
        SetMenu(hwnd, hMenuBar);

        hTitle = CreateWindowW(L"STATIC", L"请选择关机时间：",
            WS_CHILD | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE,
            0, 0, 100, 30, hwnd, (HMENU)IDC_LABEL_TITLE, g_hInst, NULL);
        SendMessage(hTitle, WM_SETFONT, (WPARAM)hTitleFont, TRUE);

        hLabelAction = CreateWindowW(L"STATIC", L"操作动作：",
            WS_CHILD | WS_VISIBLE | SS_RIGHT,
            0, 0, 130, 25, hwnd, (HMENU)IDC_LABEL_ACTION, g_hInst, NULL);
        SendMessage(hLabelAction, WM_SETFONT, (WPARAM)hFont, TRUE);

        hCombo = CreateWindowW(L"COMBOBOX", NULL,
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
            0, 0, 100, 100, hwnd, (HMENU)IDC_COMBO_ACTION, g_hInst, NULL);
        SendMessage(hCombo, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)L"关机");
        SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)L"重启");
        SendMessage(hCombo, CB_SETCURSEL, 0, 0);

        hRadio1 = CreateWindowW(L"BUTTON", L"一分钟后", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
            0, 0, 100, 30, hwnd, (HMENU)IDC_RADIO_1MIN, g_hInst, NULL);
        SendMessage(hRadio1, WM_SETFONT, (WPARAM)hFont, TRUE);
        hRadio5 = CreateWindowW(L"BUTTON", L"五分钟后", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
            0, 0, 100, 30, hwnd, (HMENU)IDC_RADIO_5MIN, g_hInst, NULL);
        SendMessage(hRadio5, WM_SETFONT, (WPARAM)hFont, TRUE);
        hRadio10 = CreateWindowW(L"BUTTON", L"十分钟后", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
            0, 0, 100, 30, hwnd, (HMENU)IDC_RADIO_10MIN, g_hInst, NULL);
        SendMessage(hRadio10, WM_SETFONT, (WPARAM)hFont, TRUE);
        hRadio30 = CreateWindowW(L"BUTTON", L"三十分钟后", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
            0, 0, 100, 30, hwnd, (HMENU)IDC_RADIO_30MIN, g_hInst, NULL);
        SendMessage(hRadio30, WM_SETFONT, (WPARAM)hFont, TRUE);
        hRadio1h = CreateWindowW(L"BUTTON", L"一小时后", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
            0, 0, 100, 30, hwnd, (HMENU)IDC_RADIO_1H, g_hInst, NULL);
        SendMessage(hRadio1h, WM_SETFONT, (WPARAM)hFont, TRUE);
        hRadio2h = CreateWindowW(L"BUTTON", L"二小时后", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
            0, 0, 100, 30, hwnd, (HMENU)IDC_RADIO_2H, g_hInst, NULL);
        SendMessage(hRadio2h, WM_SETFONT, (WPARAM)hFont, TRUE);
        hRadio3h = CreateWindowW(L"BUTTON", L"三小时后", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
            0, 0, 100, 30, hwnd, (HMENU)IDC_RADIO_3H, g_hInst, NULL);
        SendMessage(hRadio3h, WM_SETFONT, (WPARAM)hFont, TRUE);
        hRadio5h = CreateWindowW(L"BUTTON", L"五小时后", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
            0, 0, 100, 30, hwnd, (HMENU)IDC_RADIO_5H, g_hInst, NULL);
        SendMessage(hRadio5h, WM_SETFONT, (WPARAM)hFont, TRUE);

        hRadioCustom = CreateWindowW(L"BUTTON", L"指定时间", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
            0, 0, 130, 30, hwnd, (HMENU)IDC_RADIO_CUSTOM, g_hInst, NULL);
        SendMessage(hRadioCustom, WM_SETFONT, (WPARAM)hFont, TRUE);
        hRadioDateTime = CreateWindowW(L"BUTTON", L"指定日期", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
            0, 0, 130, 30, hwnd, (HMENU)IDC_RADIO_DATETIME, g_hInst, NULL);
        SendMessage(hRadioDateTime, WM_SETFONT, (WPARAM)hFont, TRUE);

        hDTPicker = CreateWindowW(DATETIMEPICK_CLASSW, NULL,
            WS_CHILD | WS_BORDER | DTS_TIMEFORMAT | DTS_UPDOWN,
            0, 0, 200, 34, hwnd, (HMENU)IDC_DTPICKER, g_hInst, NULL);
        SendMessage(hDTPicker, WM_SETFONT, (WPARAM)hDateFont, TRUE);
        SetWindowText(hDTPicker, L"HH':'mm':'ss");
        ShowWindow(hDTPicker, SW_HIDE);

        hDatePicker = CreateWindowW(DATETIMEPICK_CLASSW, NULL,
            WS_CHILD | WS_BORDER | DTS_SHORTDATEFORMAT,
            0, 0, 200, 34, hwnd, (HMENU)IDC_DATE_PICKER, g_hInst, NULL);
        SendMessage(hDatePicker, WM_SETFONT, (WPARAM)hDateFont, TRUE);
        SendMessage(hDatePicker, DTM_SETFORMAT, 0, (LPARAM)L"yyyy'年'MM'月'dd'日'");
        ShowWindow(hDatePicker, SW_HIDE);

        hTimePicker = CreateWindowW(DATETIMEPICK_CLASSW, NULL,
            WS_CHILD | WS_BORDER | DTS_TIMEFORMAT | DTS_UPDOWN,
            0, 0, 200, 34, hwnd, (HMENU)IDC_TIME_PICKER, g_hInst, NULL);
        SendMessage(hTimePicker, WM_SETFONT, (WPARAM)hDateFont, TRUE);
        SetWindowText(hTimePicker, L"HH':'mm':'ss");
        ShowWindow(hTimePicker, SW_HIDE);

        hBtnStart = CreateWindowW(L"BUTTON", L"开始倒计时",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 260, 70, hwnd, (HMENU)IDC_BTN_START, g_hInst, NULL);
        SendMessage(hBtnStart, WM_SETFONT, (WPARAM)hFont, TRUE);

        hBtnCancel = CreateWindowW(L"BUTTON", L"取消关机",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_DISABLED,
            0, 0, 260, 70, hwnd, (HMENU)IDC_BTN_CANCEL, g_hInst, NULL);
        SendMessage(hBtnCancel, WM_SETFONT, (WPARAM)hFont, TRUE);

        SendMessage(hRadio1, BM_SETCHECK, BST_CHECKED, 0);
        AddTrayIcon(hwnd);
        UpdateProgramMenu(hwnd);
    }
    break;

    case WM_INITMENUPOPUP:
        if ((HMENU)wParam == GetSubMenu(GetMenu(hwnd), 0))
            UpdateProgramMenu(hwnd);
        break;

    case WM_SIZE:
    {
        RECT rc; GetClientRect(hwnd, &rc);
        LayoutMainControls(hwnd, rc.right - rc.left, rc.bottom - rc.top);
    }
    break;

    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        switch (wmId) {
        case IDM_PROGRAM_SHOW:
            ShowCountdownWindow();
            break;
        case IDM_PROGRAM_CANCEL:
            StopCountdown();
            EnableWindow(hBtnStart, TRUE);
            EnableWindow(hBtnCancel, FALSE);
            ShowWindow(hwnd, SW_SHOW);
            break;
        case IDM_PROGRAM_EXIT:
            DestroyWindow(hwnd);
            break;
        case IDM_ABOUT:
            MessageBox(hwnd, L"软件版本：Version 1.0.0.0\r\n落尘之木：https://www.luochenzhimu.com", L"关于定时关机工具", MB_OK | MB_ICONINFORMATION);
            break;
        }

        BOOL showCustom = (SendMessage(hRadioCustom, BM_GETCHECK, 0, 0) == BST_CHECKED);
        BOOL showDateTime = (SendMessage(hRadioDateTime, BM_GETCHECK, 0, 0) == BST_CHECKED);

        if (showCustom && !IsWindowVisible(hDTPicker)) {
            SYSTEMTIME stNow; GetLocalTime(&stNow);
            SendMessage(hDTPicker, DTM_SETSYSTEMTIME, GDT_VALID, (LPARAM)&stNow);
        }
        if (showDateTime) {
            if (!IsWindowVisible(hDatePicker)) {
                SYSTEMTIME stNow; GetLocalTime(&stNow);
                SendMessage(hDatePicker, DTM_SETSYSTEMTIME, GDT_VALID, (LPARAM)&stNow);
            }
            if (!IsWindowVisible(hTimePicker)) {
                SYSTEMTIME stNow; GetLocalTime(&stNow);
                SendMessage(hTimePicker, DTM_SETSYSTEMTIME, GDT_VALID, (LPARAM)&stNow);
            }
        }

        ShowWindow(hDTPicker, showCustom ? SW_SHOW : SW_HIDE);
        ShowWindow(hDatePicker, showDateTime ? SW_SHOW : SW_HIDE);
        ShowWindow(hTimePicker, showDateTime ? SW_SHOW : SW_HIDE);

        if (HIWORD(wParam) == CBN_SELCHANGE && (HWND)lParam == hCombo) {
            g_shutdownAction = SendMessage(hCombo, CB_GETCURSEL, 0, 0);
            UpdateCancelButtonText();
            UpdateMainCancelButtonText();
            UpdateProgramMenu(hwnd);
            if (g_hCountWnd && IsWindowVisible(g_hCountWnd))
                InvalidateRect(g_hCountWnd, NULL, TRUE);
        }

        if (wmId == IDC_BTN_START) {
            g_shutdownAction = SendMessage(hCombo, CB_GETCURSEL, 0, 0);
            UpdateMainCancelButtonText();
            int seconds = 0;
            if (SendMessage(hRadio1, BM_GETCHECK, 0, 0)) seconds = 60;
            else if (SendMessage(hRadio5, BM_GETCHECK, 0, 0)) seconds = 300;
            else if (SendMessage(hRadio10, BM_GETCHECK, 0, 0)) seconds = 600;
            else if (SendMessage(hRadio30, BM_GETCHECK, 0, 0)) seconds = 1800;
            else if (SendMessage(hRadio1h, BM_GETCHECK, 0, 0)) seconds = 3600;
            else if (SendMessage(hRadio2h, BM_GETCHECK, 0, 0)) seconds = 7200;
            else if (SendMessage(hRadio3h, BM_GETCHECK, 0, 0)) seconds = 10800;
            else if (SendMessage(hRadio5h, BM_GETCHECK, 0, 0)) seconds = 18000;
            else if (showCustom) {
                SYSTEMTIME st = {};
                SendMessage(hDTPicker, DTM_GETSYSTEMTIME, 0, (LPARAM)&st);
                time_t now = time(nullptr); struct tm nowTm; localtime_s(&nowTm, &now);
                int nowSecs = nowTm.tm_hour * 3600 + nowTm.tm_min * 60 + nowTm.tm_sec;
                int targetSecs = st.wHour * 3600 + st.wMinute * 60 + st.wSecond;
                int diff = targetSecs - nowSecs;
                if (diff <= 0) diff += 24 * 3600;
                seconds = diff;
            }
            else if (showDateTime) {
                SYSTEMTIME stDate = {}, stTime = {};
                SendMessage(hDatePicker, DTM_GETSYSTEMTIME, 0, (LPARAM)&stDate);
                SendMessage(hTimePicker, DTM_GETSYSTEMTIME, 0, (LPARAM)&stTime);
                struct tm targetTm = {};
                targetTm.tm_year = stDate.wYear - 1900;
                targetTm.tm_mon = stDate.wMonth - 1;
                targetTm.tm_mday = stDate.wDay;
                targetTm.tm_hour = stTime.wHour;
                targetTm.tm_min = stTime.wMinute;
                targetTm.tm_sec = stTime.wSecond;
                targetTm.tm_isdst = -1;
                time_t targetTime = mktime(&targetTm);
                if (targetTime == -1) {
                    MessageBox(hwnd, L"日期/时间无效，请重新选择。", L"错误", MB_OK | MB_ICONERROR);
                    return 0;
                }
                time_t now = time(nullptr);
                double diffSec = difftime(targetTime, now);
                if (diffSec <= 0) {
                    MessageBox(hwnd, L"关机时间应迟于当前时间！", L"错误", MB_OK | MB_ICONWARNING);
                    return 0;
                }
                if (diffSec > 365.0 * 24 * 3600) {
                    MessageBox(hwnd, L"设定时间不能超过一年。", L"提示", MB_OK | MB_ICONINFORMATION);
                    return 0;
                }
                seconds = (int)diffSec;
            }

            if (seconds > 0) {
                StartCountdown(seconds);
                EnableWindow(hBtnStart, FALSE);
                EnableWindow(hBtnCancel, TRUE);
                ShowWindow(hwnd, SW_HIDE);
                if (g_hCountWnd) { SetForegroundWindow(g_hCountWnd); ShowWindow(g_hCountWnd, SW_SHOW); }
            }
        }
        else if (wmId == IDC_BTN_CANCEL || wmId == ID_TRAY_CANCEL) {
            StopCountdown();
            EnableWindow(hBtnStart, TRUE); EnableWindow(hBtnCancel, FALSE);
            ShowWindow(hwnd, SW_SHOW);
        }
        else if (wmId == ID_TRAY_SHOW) { ShowWindow(hwnd, SW_SHOW); SetForegroundWindow(hwnd); }
        else if (wmId == ID_TRAY_RESTORE) { ShowCountdownWindow(); }
        else if (wmId == ID_TRAY_EXIT) { DestroyWindow(hwnd); }
    }
    break;

    case WM_TIMER:
        if (wParam == IDT_COUNTDOWN) {
            if (g_remainingSec > 0) {
                g_remainingSec--;
                UpdateCountdownWindow();
                if (g_remainingSec <= 10 && g_remainingSec > 0) {
                    if (!g_blink) g_blink = true;
                    SetTimer(hwnd, IDT_BLINK, 500, NULL);
                }
                else if (g_remainingSec > 10) { KillTimer(hwnd, IDT_BLINK); g_blink = true; }
                if (g_remainingSec == 0) { StopCountdown(); DoShutdown(); }
            }
            else { StopCountdown(); DoShutdown(); }
        }
        else if (wParam == IDT_BLINK) {
            g_blink = !g_blink; g_blinkPhase++;
            InvalidateRect(g_hCountWnd, NULL, TRUE);
        }
        break;

    case WM_CLOSE: ShowWindow(hwnd, SW_HIDE); return 0;
    case WM_DESTROY:
        RemoveTrayIcon(); StopCountdown();
        if (hFont) DeleteObject(hFont);
        if (hTitleFont) DeleteObject(hTitleFont);
        if (hDateFont) DeleteObject(hDateFont);
        PostQuitMessage(0);
        break;

    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP) {
            POINT pt; GetCursorPos(&pt);
            HMENU hMenu = CreatePopupMenu();
            AppendMenu(hMenu, MF_STRING, ID_TRAY_SHOW, L"显示主窗口");
            if (g_countdownActive && g_hCountWnd && !IsWindowVisible(g_hCountWnd))
                AppendMenu(hMenu, MF_STRING, ID_TRAY_RESTORE, L"显示倒计时");
            if (g_countdownActive)
                AppendMenu(hMenu, MF_STRING, ID_TRAY_CANCEL, (g_shutdownAction == 1) ? L"取消重启" : L"取消关机");
            AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
            AppendMenu(hMenu, MF_STRING, ID_TRAY_EXIT, L"退出");
            SetForegroundWindow(hwnd);
            TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
            DestroyMenu(hMenu);
        }
        else if (lParam == WM_LBUTTONDBLCLK) {
            if (g_countdownActive && g_hCountWnd) ShowCountdownWindow();
            else { ShowWindow(hwnd, SW_SHOW); SetForegroundWindow(hwnd); }
        }
        break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK CountdownWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static HFONT hArtFont = NULL, hBtnFont = NULL;
    static HWND hProgress, hCancelBtn;

    switch (msg) {
    case WM_CREATE:
    {
        hArtFont = CreateArtFont(48);
        hBtnFont = CreateYaHeiFont(20, FALSE);
        HMENU hSysMenu = GetSystemMenu(hwnd, FALSE);
        AppendMenu(hSysMenu, MF_SEPARATOR, 0, NULL);
        AppendMenu(hSysMenu, MF_STRING, IDM_FULLSCREEN, L"全屏(&F)");
        RECT rc; GetClientRect(hwnd, &rc);
        int w = rc.right - rc.left, h = rc.bottom - rc.top;
        int titleBarHeight = GetSystemMetrics(SM_CYCAPTION);
        int usableH = h - titleBarHeight;
        int margin = w / 20;
        int progHeight = usableH / 12, btnHeight = usableH / 8, btnWidth = w / 3;
        hProgress = CreateWindowW(PROGRESS_CLASSW, NULL, WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
            0, 0, 100, 20, hwnd, (HMENU)102, g_hInst, NULL);
        SendMessage(hProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
        hCancelBtn = CreateWindowW(L"BUTTON", L"取消关机", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 100, 30, hwnd, (HMENU)103, g_hInst, NULL);
        SendMessage(hCancelBtn, WM_SETFONT, (WPARAM)hBtnFont, TRUE);
        LayoutCountdownControls(hwnd, w, h);
    }
    break;
    case WM_SIZE: LayoutCountdownControls(hwnd, LOWORD(lParam), HIWORD(lParam)); break;
    case WM_ERASEBKGND: return TRUE;
    case WM_PAINT:
    {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        int titleBarHeight = GetSystemMetrics(SM_CYCAPTION);
        int textAreaHeight = (rc.bottom - rc.top - titleBarHeight) * 50 / 100;
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBmp = CreateCompatibleBitmap(hdc, rc.right - rc.left, rc.bottom - rc.top);
        HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);
        HBRUSH hBr = CreateSolidBrush(RGB(30, 30, 30));
        FillRect(memDC, &rc, hBr); DeleteObject(hBr);
        SelectObject(memDC, hArtFont); SetBkMode(memDC, TRANSPARENT);
        std::wstring text = SecondsToText(g_remainingSec);
        SIZE sz;
        GetTextExtentPoint32W(memDC, text.c_str(), (int)text.length(), &sz);
        int x = (rc.right - rc.left - sz.cx) / 2, y = titleBarHeight + (textAreaHeight - sz.cy) / 2;
        if (g_remainingSec <= 10) {
            if (g_blink) {
                SetTextColor(memDC, RGB(50, 50, 50));
                TextOutW(memDC, x + 3, y + 3, text.c_str(), (int)text.length());
                SetTextColor(memDC, RGB(255, 50, 50));
                TextOutW(memDC, x, y, text.c_str(), (int)text.length());
                RECT textRect = { x, y, x + sz.cx, y + sz.cy };
                DrawBlingEffect(memDC, textRect);
            }
        }
        else {
            SetTextColor(memDC, RGB(50, 50, 50));
            TextOutW(memDC, x + 3, y + 3, text.c_str(), (int)text.length());
            SetTextColor(memDC, RGB(255, 255, 255));
            TextOutW(memDC, x, y, text.c_str(), (int)text.length());
        }
        BitBlt(hdc, 0, 0, rc.right - rc.left, rc.bottom - rc.top, memDC, 0, 0, SRCCOPY);
        SelectObject(memDC, oldBmp); DeleteObject(memBmp); DeleteDC(memDC);
        EndPaint(hwnd, &ps);
    }
    break;
    case WM_SYSCOMMAND:
        if (wParam == IDM_FULLSCREEN) { ToggleFullscreen(hwnd); return 0; }
        break;
    case WM_COMMAND:
        if (LOWORD(wParam) == 103) {
            if (g_hMainWnd) {
                EnableWindow(GetDlgItem(g_hMainWnd, IDC_BTN_START), TRUE);
                EnableWindow(GetDlgItem(g_hMainWnd, IDC_BTN_CANCEL), FALSE);
                ShowWindow(g_hMainWnd, SW_SHOW);
            }
            StopCountdown();
        }
        break;
    case WM_CLOSE: ShowWindow(hwnd, SW_HIDE); return 0;
    case WM_DESTROY:
        if (hArtFont) DeleteObject(hArtFont);
        if (hBtnFont) DeleteObject(hBtnFont);
        break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}