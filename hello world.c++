// ============================================================
//  可视化随机点名程序 (1-53号)
//  纯标准 C++，控制台版本，带 ANSI 颜色和动画效果
//
//  编译: g++ "hello world.c++" -o rollcall
//  运行: ./rollcall    (或双击 rollcall.exe)
//
//  操作: [回车] 开始/停止点名  [R] 重置  [Q] 退出
// ============================================================

#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <string>
#include <thread>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#endif

// ============ ANSI 颜色码 ============
namespace Color {
    const char* RESET   = "\033[0m";
    const char* BOLD    = "\033[1m";
    const char* RED     = "\033[91m";
    const char* GREEN   = "\033[92m";
    const char* YELLOW  = "\033[93m";
    const char* BLUE    = "\033[94m";
    const char* MAGENTA = "\033[95m";
    const char* CYAN    = "\033[96m";
    const char* WHITE   = "\033[97m";
    const char* GRAY    = "\033[90m";

    const char* BG_BLACK   = "\033[40m";
    const char* BG_RED     = "\033[101m";
    const char* BG_GREEN   = "\033[42m";
    const char* BG_YELLOW  = "\033[103m";
    const char* BG_BLUE    = "\033[44m";
    const char* BG_CYAN    = "\033[46m";
    const char* BG_WHITE   = "\033[47m";
    const char* BG_DEFAULT = "\033[49m";
}

// ============ 全局状态 ============
std::vector<int> called_list;
bool  called_map[54] = {};
int   current_num   = 1;
int   result_num    = 0;
bool  is_rolling    = false;
bool  show_result   = false;
int   roll_count    = 0;
int   total_rolls   = 0;
int   target_rolls  = 0;
int   roll_delay    = 30;   // 毫秒

// ============ 辅助函数 ============

void enable_ansi() {
#ifdef _WIN32
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(out, &mode);
    SetConsoleMode(out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif
}

void clear_screen() {
    std::cout << "\033[2J\033[H";
}

void set_cursor_visible(bool visible) {
#ifdef _WIN32
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO info;
    GetConsoleCursorInfo(out, &info);
    info.bVisible = visible;
    SetConsoleCursorInfo(out, &info);
#else
    std::cout << (visible ? "\033[?25h" : "\033[?25l");
#endif
}

int get_console_width() {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO info;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info);
    return info.srWindow.Right - info.srWindow.Left + 1;
#else
    return 80;
#endif
}

// 检测键盘输入（非阻塞）
bool kbhit() {
#ifdef _WIN32
    static bool init = false;
    static HANDLE hin;
    if (!init) {
        hin = GetStdHandle(STD_INPUT_HANDLE);
        SetConsoleMode(hin, ENABLE_PROCESSED_INPUT);
        init = true;
    }
    DWORD events;
    GetNumberOfConsoleInputEvents(hin, &events);
    if (events > 0) {
        INPUT_RECORD rec;
        DWORD read;
        PeekConsoleInput(hin, &rec, 1, &read);
        if (read > 0 && rec.EventType == KEY_EVENT && rec.Event.KeyEvent.bKeyDown)
            return true;
        // 消耗非按键事件
        ReadConsoleInput(hin, &rec, 1, &read);
    }
    return false;
#else
    return false; // Linux/Mac 使用不同的方式
#endif
}

char get_char() {
#ifdef _WIN32
    HANDLE hin = GetStdHandle(STD_INPUT_HANDLE);
    INPUT_RECORD rec;
    DWORD read;
    while (true) {
        ReadConsoleInput(hin, &rec, 1, &read);
        if (rec.EventType == KEY_EVENT && rec.Event.KeyEvent.bKeyDown)
            return rec.Event.KeyEvent.uChar.AsciiChar;
    }
#else
    return std::cin.get();
#endif
}

std::string center_text(const std::string& text, int width) {
    int vis_len = 0;
    bool in_escape = false;
    for (size_t i = 0; i < text.size(); i++) {
        if (text[i] == '\033') { in_escape = true; continue; }
        if (in_escape) {
            if (text[i] == 'm') in_escape = false;
            continue;
        }
        vis_len++;
    }
    int pad = (width - vis_len) / 2;
    if (pad < 0) pad = 0;
    return std::string(pad, ' ') + text;
}

// ============ 绘制 ============

void draw_border(int width, const std::string& color) {
    std::cout << color;
    std::cout << "+" << std::string(width - 2, '-') << "+" << std::endl;
    std::cout << Color::RESET;
}

void draw_line(int width, const std::string& text, const std::string& color) {
    std::cout << color << "|";
    std::cout << center_text(text, width - 2);
    std::string pad(width - 2 - text.size(), ' ');
    std::cout << "|" << Color::RESET << std::endl;
}

void draw_big_number(int num, int width, const std::string& color) {
    // ASCII 大数字 5x7
    const char* digits[10][7] = {
        {" ███ ", "█   █", "█   █", "█   █", "█   █", "█   █", " ███ "}, // 0
        {"  █  ", " ██  ", "  █  ", "  █  ", "  █  ", "  █  ", " ███ "}, // 1
        {" ███ ", "█   █", "    █", "   █ ", "  █  ", " █   ", "█████"}, // 2
        {" ███ ", "█   █", "    █", "  ██ ", "    █", "█   █", " ███ "}, // 3
        {"█   █", "█   █", "█   █", "█████", "    █", "    █", "    █"}, // 4
        {"█████", "█    ", "████ ", "    █", "    █", "█   █", " ███ "}, // 5
        {" ███ ", "█   █", "█    ", "████ ", "█   █", "█   █", " ███ "}, // 6
        {"█████", "    █", "   █ ", "  █  ", " █   ", "█    ", "█    "}, // 7
        {" ███ ", "█   █", "█   █", " ███ ", "█   █", "█   █", " ███ "}, // 8
        {" ███ ", "█   █", "█   █", " ████", "    █", "█   █", " ███ "}  // 9
    };

    std::string num_str = std::to_string(num);
    int total_w = (int)num_str.size() * 6 - 1;
    int pad_left = (width - 2 - total_w) / 2;
    if (pad_left < 2) pad_left = 2;

    for (int row = 0; row < 7; row++) {
        std::cout << color << "|" << std::string(pad_left, ' ');
        for (size_t d = 0; d < num_str.size(); d++) {
            if (d > 0) std::cout << " ";
            std::cout << digits[num_str[d] - '0'][row];
        }
        std::string pad_right(width - 2 - pad_left - total_w, ' ');
        std::cout << pad_right << "|" << Color::RESET << std::endl;
    }
}

void draw_grid(int width) {
    int cols = 10;
    int grid_w = width - 4;
    int cell_w = (grid_w - cols - 1) / cols;
    int total_w = cell_w * cols + (cols - 1);
    int pad_l = (grid_w - total_w) / 2;

    std::cout << Color::CYAN << Color::BOLD;
    std::string title = "=== 已点名列表 (" +
                        std::to_string(called_list.size()) + "/53) ===";
    std::cout << center_text(title, width) << Color::RESET << std::endl;
    std::cout << std::endl;

    // 6 行 (1-50) + 1 行 (51-53)
    for (int row = 0; row < 6; row++) {
        std::cout << "  " << std::string(pad_l, ' ');
        for (int col = 0; col < cols; col++) {
            int num = row * cols + col + 1;
            if (num > 53) break;

            if (called_map[num]) {
                if (num == result_num && show_result) {
                    std::cout << Color::BG_YELLOW << Color::RED << Color::BOLD;
                } else {
                    std::cout << Color::BG_GREEN << Color::WHITE;
                }
            } else {
                std::cout << Color::GRAY;
            }

            char buf[8];
            sprintf(buf, "%2d", num);
            std::cout << " " << buf << " ";
            std::cout << Color::RESET;
            if (col < cols - 1 && row * cols + col + 1 < 53)
                std::cout << " ";
        }
        std::cout << std::endl;
    }
}

void draw_ui() {
    clear_screen();
    int w = get_console_width();
    if (w < 60) w = 60;
    if (w > 100) w = 100;

    // 标题
    std::cout << std::endl;
    std::cout << Color::CYAN << Color::BOLD;
    std::cout << center_text("╔══════════════════════════╗", w) << std::endl;
    std::cout << center_text("║    🎯 随机点名系统 1-53   ║", w) << std::endl;
    std::cout << center_text("╚══════════════════════════╝", w) << std::endl;
    std::cout << Color::RESET;
    std::cout << std::endl;

    // 主体显示区
    int card_w = 50;
    std::string card_color = Color::BLUE;
    draw_border(card_w, card_color);

    // 空白行
    for (int i = 0; i < 2; i++)
        draw_line(card_w, "", card_color);

    // 大数字
    int n = show_result ? result_num : current_num;
    std::string num_color;
    if (show_result)
        num_color = std::string(Color::BOLD) + Color::YELLOW;
    else if (is_rolling)
        num_color = Color::WHITE;
    else
        num_color = Color::WHITE;

    draw_big_number(n, card_w, num_color);

    // 状态行
    for (int i = 0; i < 1; i++)
        draw_line(card_w, "", card_color);

    if (is_rolling) {
        std::string flicker = (roll_count / 3) % 2 ? "▐" : "▌";
        draw_line(card_w, Color::MAGENTA + std::string("● ") + flicker +
                  " 点选中... " + flicker, card_color);
    } else if (show_result) {
        draw_line(card_w, std::string(Color::BOLD) + Color::YELLOW +
                  "⭐ 恭喜中选! ⭐", card_color);
    } else {
        draw_line(card_w, "按 [回车] 开始点名", card_color);
    }

    for (int i = 0; i < 1; i++)
        draw_line(card_w, "", card_color);

    draw_border(card_w, card_color);

    std::cout << std::endl;

    // 已点名网格
    draw_grid(w);

    // 底部提示
    std::cout << std::endl;
    std::cout << Color::GRAY;
    std::cout << center_text("[回车] 开始/停止点名  |  [R] 重置  |  [Q] 退出", w);
    std::cout << Color::RESET << std::endl;
}

// ============ 点名逻辑 ============

void start_roll() {
    if (called_list.size() >= 53) {
        is_rolling = false;
        show_result = false;
        draw_ui();
        std::cout << std::endl;
        std::cout << Color::RED << Color::BOLD;
        std::cout << center_text("所有53个号码已全部点完! 按 R 重置", get_console_width());
        std::cout << Color::RESET << std::endl;
        return;
    }

    is_rolling  = true;
    show_result = false;
    result_num  = 0;
    roll_count  = 0;
    total_rolls = 0;
    roll_delay  = 30;
    target_rolls = 30 + rand() % 31;
}

void stop_roll() {
    // 从未点名的号码中随机选一个
    std::vector<int> remaining;
    for (int i = 1; i <= 53; i++) {
        if (!called_map[i])
            remaining.push_back(i);
    }
    if (remaining.empty()) {
        is_rolling = false;
        return;
    }

    result_num  = remaining[rand() % remaining.size()];
    is_rolling  = false;
    show_result = true;
    current_num = result_num;
    called_map[result_num] = true;
    called_list.push_back(result_num);
}

void reset_all() {
    called_list.clear();
    for (int i = 0; i < 54; i++) called_map[i] = false;
    is_rolling  = false;
    show_result = false;
    current_num = 1;
    result_num  = 0;
    roll_count  = 0;
}

// ============ 主循环 ============

int main() {
    srand((unsigned)time(nullptr));
    enable_ansi();
    set_cursor_visible(false);

    std::cout << Color::RESET;
    draw_ui();

    // 主循环
    bool running = true;
    while (running) {
        // 如果正在滚动
        if (is_rolling) {
            std::this_thread::sleep_for(std::chrono::milliseconds(roll_delay));

            // 随机选一个未点名的号码
            std::vector<int> remaining;
            for (int i = 1; i <= 53; i++)
                if (!called_map[i]) remaining.push_back(i);

            if (!remaining.empty())
                current_num = remaining[rand() % remaining.size()];

            roll_count++;
            total_rolls++;

            // 逐渐减速
            if (total_rolls >= target_rolls * 0.6 && roll_delay < 200) {
                roll_delay += 10;
            }

            // 到达目标次数
            if (total_rolls >= target_rolls) {
                stop_roll();
            }

            draw_ui();
        }

        // 检测按键
        if (kbhit()) {
            char ch = get_char();
            if (ch == '\r' || ch == '\n' || ch == ' ') {  // 回车或空格
                if (is_rolling) {
                    stop_roll();
                    draw_ui();
                } else {
                    start_roll();
                    draw_ui();
                }
            } else if (ch == 'r' || ch == 'R') {
                reset_all();
                draw_ui();
            } else if (ch == 'q' || ch == 'Q') {
                running = false;
            }
        }

        // 不滚动时短暂休眠，减少 CPU 占用
        if (!is_rolling) {
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        }
    }

    set_cursor_visible(true);
    clear_screen();
    std::cout << Color::RESET;
    std::cout << "已退出随机点名系统。再见!" << std::endl;
    return 0;
}
