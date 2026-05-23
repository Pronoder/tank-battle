// 可视化随机点名程序 (1-53号)
// 使用 Win32 GDI 绘制界面，支持动画滚动点名
// 编译: g++ -o rollcall "hello world.c++" -lgdi32 -mwindows

#include <windows.h>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <string>
#include <cmath>

#define IDT_TIMER1 1
#define IDT_TIMER2 2
#define BTN_START  100
#define BTN_RESET 101

// 全局状态
HWND g_hWnd;
HWND g_btnStart, g_btnReset;
std::vector<int> g_called;       // 已点名列表
bool g_rolling = false;          // 是否正在滚动
int  g_currentNum = 1;           // 当前显示的数字
int  g_resultNum = 0;            // 最终结果
bool g_showResult = false;       // 是否显示结果
int  g_rollSpeed = 30;           // 滚动速度(ms)
int  g_rollCount = 0;            // 滚动计数
int  g_totalRolls = 0;           // 已滚动的总次数
int  g_targetRolls = 0;          // 目标滚动次数(随机30~60)
bool g_highlightCalled[54] = {}; // 已点名的标记
int  g_particles[20][4] = {};    // 粒子效果: x, y, vx, vy, life
bool g_particlesActive = false;

// 颜色定义
COLORREF g_bgColor     = RGB(18, 22, 36);
COLORREF g_cardColor   = RGB(30, 35, 55);
COLORREF g_accentColor = RGB(0, 200, 255);
COLORREF g_goldColor   = RGB(255, 200, 0);
COLORREF g_textColor   = RGB(220, 225, 240);
COLORREF g_dangerColor = RGB(255, 80, 80);

HFONT g_hFontLarge = NULL;
HFONT g_hFontMid   = NULL;
HFONT g_hFontSmall = NULL;

void InitFonts() {
    g_hFontLarge = CreateFontW(140, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                               ANTIALIASED_QUALITY, FF_MODERN, L"Consolas");
    g_hFontMid = CreateFontW(36, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             ANTIALIASED_QUALITY, FF_MODERN, L"Microsoft YaHei");
    g_hFontSmall = CreateFontW(22, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                               ANTIALIASED_QUALITY, FF_MODERN, L"Microsoft YaHei");
}

void SpawnParticles(int cx, int cy) {
    g_particlesActive = true;
    for (int i = 0; i < 20; i++) {
        g_particles[i][0] = cx;
        g_particles[i][1] = cy;
        double angle = (rand() % 360) * 3.14159 / 180.0;
        double speed = 2 + rand() % 6;
        g_particles[i][2] = (int)(cos(angle) * speed);
        g_particles[i][3] = (int)(sin(angle) * speed);
        g_particles[i][4] = 20 + rand() % 25;
    }
}

void UpdateParticles() {
    if (!g_particlesActive) return;
    bool anyAlive = false;
    for (int i = 0; i < 20; i++) {
        if (g_particles[i][4] > 0) {
            g_particles[i][0] += g_particles[i][2];
            g_particles[i][1] += g_particles[i][3];
            g_particles[i][3] += 1; // gravity
            g_particles[i][4]--;
            anyAlive = true;
        }
    }
    if (!anyAlive) g_particlesActive = false;
}

void DrawParticles(HDC hdc) {
    for (int i = 0; i < 20; i++) {
        if (g_particles[i][4] > 0) {
            int alpha = (int)(g_particles[i][4] * 255.0 / 25);
            int r = GetRValue(g_goldColor);
            int gv = GetGValue(g_goldColor);
            int b = GetBValue(g_goldColor);
            HPEN pen = CreatePen(PS_SOLID, 2, RGB(
                (r * alpha) / 255,
                (gv * alpha) / 255,
                (b * alpha) / 255));
            HPEN oldPen = (HPEN)SelectObject(hdc, pen);
            int x = g_particles[i][0];
            int y = g_particles[i][1];
            MoveToEx(hdc, x - 3, y, NULL);
            LineTo(hdc, x + 3, y);
            MoveToEx(hdc, x, y - 3, NULL);
            LineTo(hdc, x, y + 3);
            SelectObject(hdc, oldPen);
            DeleteObject(pen);
        }
    }
}

void DrawRoundedRect(HDC hdc, RECT rc, int radius, COLORREF fill, COLORREF border) {
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 2, border);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, brush);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, radius, radius);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

void DrawCenteredText(HDC hdc, RECT rc, const wchar_t* text, HFONT font, COLORREF color) {
    HFONT oldFont = (HFONT)SelectObject(hdc, font);
    SetTextColor(hdc, color);
    SetBkMode(hdc, TRANSPARENT);
    DrawTextW(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, oldFont);
}

void DrawCalledList(HDC hdc, RECT rc) {
    // 标题区域
    RECT titleRC = rc;
    titleRC.bottom = titleRC.top + 45;
    DrawCenteredText(hdc, titleRC, L"📋 已点名列表", g_hFontMid, g_accentColor);

    // 列表区域
    int margin = 8;
    int cols = 10;
    int cellW = ((rc.right - rc.left) - margin * (cols + 1)) / cols;
    int cellH = 32;
    int startY = titleRC.bottom + 10;

    HFONT oldFont = (HFONT)SelectObject(hdc, g_hFontSmall);
    SetBkMode(hdc, TRANSPARENT);

    for (int i = 1; i <= 53; i++) {
        int row = (i - 1) / cols;
        int col = (i - 1) % cols;
        int x = rc.left + margin + col * (cellW + margin);
        int y = startY + row * (cellH + margin);

        RECT cellRC = { x, y, x + cellW, y + cellH };

        if (g_highlightCalled[i]) {
            COLORREF bgColor = (i == g_resultNum && g_showResult) ? g_goldColor : RGB(0, 140, 100);
            COLORREF textColor = (i == g_resultNum && g_showResult) ? RGB(30, 30, 30) : RGB(255, 255, 255);
            COLORREF borderColor = (i == g_resultNum && g_showResult) ? RGB(255, 220, 60) : RGB(0, 200, 140);

            HBRUSH brush = CreateSolidBrush(bgColor);
            HPEN pen = CreatePen(PS_SOLID, 2, borderColor);
            HBRUSH oldBr = (HBRUSH)SelectObject(hdc, brush);
            HPEN oldP = (HPEN)SelectObject(hdc, pen);
            RoundRect(hdc, x, y, x + cellW, y + cellH, 6, 6);
            SelectObject(hdc, oldBr);
            SelectObject(hdc, oldP);
            DeleteObject(brush);
            DeleteObject(pen);

            SetTextColor(hdc, textColor);
            wchar_t buf[8];
            wsprintfW(buf, L"%d", i);
            DrawTextW(hdc, buf, -1, &cellRC, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        } else {
            // 未点名的灰色格子
            HBRUSH brush = CreateSolidBrush(RGB(40, 44, 60));
            HPEN pen = CreatePen(PS_SOLID, 1, RGB(55, 60, 75));
            HBRUSH oldBr = (HBRUSH)SelectObject(hdc, brush);
            HPEN oldP = (HPEN)SelectObject(hdc, pen);
            RoundRect(hdc, x, y, x + cellW, y + cellH, 6, 6);
            SelectObject(hdc, oldBr);
            SelectObject(hdc, oldP);
            DeleteObject(brush);
            DeleteObject(pen);

            SetTextColor(hdc, RGB(140, 145, 160));
            wchar_t buf[8];
            wsprintfW(buf, L"%d", i);
            DrawTextW(hdc, buf, -1, &cellRC, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
    }

    SelectObject(hdc, oldFont);
}

void OnPaint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT clientRC;
    GetClientRect(hwnd, &clientRC);

    // 双缓冲
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, clientRC.right, clientRC.bottom);
    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

    // 背景
    HBRUSH bgBrush = CreateSolidBrush(g_bgColor);
    FillRect(memDC, &clientRC, bgBrush);
    DeleteObject(bgBrush);

    int w = clientRC.right;
    int h = clientRC.bottom;

    // === 左侧大数字显示区 ===
    int leftW = (int)(w * 0.48);
    RECT mainCard = { 20, 20, leftW - 10, h - 20 };
    DrawRoundedRect(memDC, mainCard, 20, g_cardColor, RGB(50, 55, 75));

    // 标题
    RECT titleRC = { mainCard.left, mainCard.top + 20, mainCard.right, mainCard.top + 70 };
    DrawCenteredText(memDC, titleRC, L"🎯 随机点名", g_hFontMid, g_accentColor);

    // 数字显示
    RECT numRC = { mainCard.left, mainCard.top + 90, mainCard.right, mainCard.bottom - 160 };

    // 光晕效果
    if (g_showResult) {
        int cx = (numRC.left + numRC.right) / 2;
        int cy = (numRC.top + numRC.bottom) / 2;
        for (int r = 140; r > 30; r -= 15) {
            int alpha = 20 + (140 - r);
            COLORREF glow = RGB(
                (255 * alpha) / 160,
                (200 * alpha) / 160,
                (0 * alpha) / 160
            );
            if (glow > 0) {
                HBRUSH glowBrush = CreateSolidBrush(glow);
                HPEN glowPen = CreatePen(PS_SOLID, 1, glow);
                HBRUSH oldBr = (HBRUSH)SelectObject(memDC, glowBrush);
                HPEN oldPn = (HPEN)SelectObject(memDC, glowPen);
                Ellipse(memDC, cx - r, cy - r, cx + r, cy + r);
                SelectObject(memDC, oldBr);
                SelectObject(memDC, oldPn);
                DeleteObject(glowBrush);
                DeleteObject(glowPen);
            }
        }
    }

    // 数字
    wchar_t numStr[8];
    wsprintfW(numStr, L"%d", g_showResult ? g_resultNum : g_currentNum);
    COLORREF numColor = g_showResult ? g_goldColor : g_textColor;
    DrawCenteredText(memDC, numRC, numStr, g_hFontLarge, numColor);

    // 滚动时显示 "点选中..."
    if (g_rolling) {
        RECT rollingRC = { mainCard.left, mainCard.bottom - 150, mainCard.right, mainCard.bottom - 110 };
        COLORREF blink = (g_rollCount / 5) % 2 == 0 ? g_accentColor : RGB(255, 150, 0);
        DrawCenteredText(memDC, rollingRC, L"● 点选中...", g_hFontSmall, blink);
    }

    // 结果文字
    if (g_showResult && !g_rolling) {
        RECT resultRC = { mainCard.left, mainCard.bottom - 150, mainCard.right, mainCard.bottom - 110 };
        DrawCenteredText(memDC, resultRC, L"✨ 恭喜中选! ✨", g_hFontMid, g_goldColor);
    }

    // 已点名计数
    RECT countRC = { mainCard.left, mainCard.bottom - 95, mainCard.right, mainCard.bottom - 55 };
    wchar_t countStr[64];
    wsprintfW(countStr, L"已点名: %d / 53 人", (int)g_called.size());
    DrawCenteredText(memDC, countRC, countStr, g_hFontSmall, g_textColor);

    // 粒子
    DrawParticles(memDC);

    // === 右侧已点名列表 ===
    RECT rightPanel = { leftW + 10, 20, w - 20, h - 20 };
    DrawRoundedRect(memDC, rightPanel, 20, g_cardColor, RGB(50, 55, 75));
    RECT listRC = { rightPanel.left + 15, rightPanel.top + 10, rightPanel.right - 15, rightPanel.bottom - 10 };
    DrawCalledList(memDC, listRC);

    // 输出
    BitBlt(hdc, 0, 0, clientRC.right, clientRC.bottom, memDC, 0, 0, SRCCOPY);
    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);

    EndPaint(hwnd, &ps);
}

void StartRoll() {
    if (g_called.size() >= 53) {
        // 全部点完
        g_rolling = false;
        g_showResult = false;
        MessageBoxW(g_hWnd, L"所有53个号码已全部点完!\n请点击\"重置\"重新开始。",
                    L"提示", MB_OK | MB_ICONINFORMATION);
        return;
    }

    g_rolling = true;
    g_showResult = false;
    g_resultNum = 0;
    g_rollCount = 0;
    g_totalRolls = 0;
    g_particlesActive = false;

    // 随机目标滚动次数: 30~60次
    g_targetRolls = 30 + rand() % 31;

    // 初始速度较快
    g_rollSpeed = 20;

    SetTimer(g_hWnd, IDT_TIMER1, g_rollSpeed, NULL);
    SetWindowTextW(g_btnStart, L"停止点名");
}

void StopRoll() {
    KillTimer(g_hWnd, IDT_TIMER1);

    // 从未点名的号码中随机选一个
    std::vector<int> remaining;
    for (int i = 1; i <= 53; i++) {
        if (!g_highlightCalled[i]) {
            remaining.push_back(i);
        }
    }

    if (remaining.empty()) {
        g_rolling = false;
        return;
    }

    g_resultNum = remaining[rand() % remaining.size()];
    g_rolling = false;
    g_showResult = true;
    g_currentNum = g_resultNum;
    g_highlightCalled[g_resultNum] = true;
    g_called.push_back(g_resultNum);

    // 粒子效果
    RECT rc;
    GetClientRect(g_hWnd, &rc);
    int leftW = (int)(rc.right * 0.48);
    int cx = 20 + (leftW - 10 - 20) / 2;
    int cy = 90 + (rc.bottom - 20 - 90 - 160) / 2;
    SpawnParticles(cx, cy);
    SetTimer(g_hWnd, IDT_TIMER2, 30, NULL);

    SetWindowTextW(g_btnStart, L"开始点名");

    // 如果全部点完
    if (g_called.size() >= 53) {
        SetWindowTextW(g_btnStart, L"全部完成");
        EnableWindow(g_btnStart, FALSE);
    }

    InvalidateRect(g_hWnd, NULL, TRUE);
}

void ResetAll() {
    KillTimer(g_hWnd, IDT_TIMER1);
    KillTimer(g_hWnd, IDT_TIMER2);
    g_called.clear();
    g_rolling = false;
    g_showResult = false;
    g_currentNum = 1;
    g_resultNum = 0;
    g_particlesActive = false;
    g_rollCount = 0;
    for (int i = 0; i < 54; i++) g_highlightCalled[i] = false;

    SetWindowTextW(g_btnStart, L"开始点名");
    EnableWindow(g_btnStart, TRUE);
    InvalidateRect(g_hWnd, NULL, TRUE);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            g_hWnd = hwnd;
            srand((unsigned)time(NULL));
            InitFonts();

            // 开始按钮
            g_btnStart = CreateWindowW(L"BUTTON", L"开始点名",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                20, 0, 180, 50,
                hwnd, (HMENU)BTN_START,
                ((LPCREATESTRUCTW)lParam)->hInstance, NULL);

            // 重置按钮
            g_btnReset = CreateWindowW(L"BUTTON", L"重置",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                220, 0, 100, 50,
                hwnd, (HMENU)BTN_RESET,
                ((LPCREATESTRUCTW)lParam)->hInstance, NULL);
            break;
        }

        case WM_SIZE: {
            RECT rc;
            GetClientRect(hwnd, &rc);
            int leftW = (int)(rc.right * 0.48);
            // 按钮放在左下角
            int btnY = rc.bottom - 50;
            SetWindowPos(g_btnStart, NULL, 30, btnY, 170, 42, SWP_NOZORDER);
            SetWindowPos(g_btnReset, NULL, 215, btnY, 90, 42, SWP_NOZORDER);
            InvalidateRect(hwnd, NULL, TRUE);
            break;
        }

        case WM_CTLCOLORBTN: {
            // 按钮样式
            HDC hdcBtn = (HDC)wParam;
            SetTextColor(hdcBtn, RGB(255, 255, 255));
            SetBkColor(hdcBtn, RGB(0, 160, 220));
            return (LRESULT)CreateSolidBrush(RGB(0, 160, 220));
        }

        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case BTN_START:
                    if (g_rolling) {
                        StopRoll();
                    } else {
                        StartRoll();
                    }
                    break;
                case BTN_RESET:
                    ResetAll();
                    break;
            }
            break;
        }

        case WM_TIMER: {
            if (wParam == IDT_TIMER1 && g_rolling) {
                g_totalRolls++;

                // 从未点名的号码中随机选
                std::vector<int> remaining;
                for (int i = 1; i <= 53; i++) {
                    if (!g_highlightCalled[i]) {
                        remaining.push_back(i);
                    }
                }
                if (!remaining.empty()) {
                    g_currentNum = remaining[rand() % remaining.size()];
                }

                g_rollCount++;

                // 逐渐减速
                if (g_totalRolls >= g_targetRolls * 0.6 && g_rollSpeed < 150) {
                    g_rollSpeed += 8;
                    KillTimer(hwnd, IDT_TIMER1);
                    SetTimer(hwnd, IDT_TIMER1, g_rollSpeed, NULL);
                }

                // 到达目标次数后停止
                if (g_totalRolls >= g_targetRolls) {
                    StopRoll();
                }

                InvalidateRect(hwnd, NULL, TRUE);
            } else if (wParam == IDT_TIMER2) {
                UpdateParticles();
                if (!g_particlesActive) {
                    KillTimer(hwnd, IDT_TIMER2);
                }
                InvalidateRect(hwnd, NULL, TRUE);
            }
            break;
        }

        case WM_PAINT:
            OnPaint(hwnd);
            break;

        case WM_DESTROY:
            KillTimer(hwnd, IDT_TIMER1);
            KillTimer(hwnd, IDT_TIMER2);
            if (g_hFontLarge) DeleteObject(g_hFontLarge);
            if (g_hFontMid) DeleteObject(g_hFontMid);
            if (g_hFontSmall) DeleteObject(g_hFontSmall);
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow) {
    const wchar_t CLASS_NAME[] = L"RandomRollCallWindow";

    WNDCLASSW wc = {};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(g_bgColor);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(
        WS_EX_COMPOSITED,           // 双缓冲减少闪烁
        CLASS_NAME,
        L"🎯 随机点名系统 (1-53)",
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME,
        CW_USEDEFAULT, CW_USEDEFAULT, 1200, 750,
        NULL, NULL, hInstance, NULL);

    if (!hwnd) return 1;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg = {};
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}
