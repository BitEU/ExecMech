#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <conio.h>
#include <direct.h>

#pragma comment(lib, "user32.lib")

// --- UI & Style Constants ---
// Using Unicode literals for modern box-drawing characters
#define BOX_V "║"
#define BOX_H "═"
#define BOX_TL "╔"
#define BOX_TR "╗"
#define BOX_BL "╚"
#define BOX_BR "╝"
#define BOX_TJ "╦"
#define BOX_BJ "╩"
#define BOX_LJ "╠"
#define BOX_RJ "╣"
#define SCROLL_TRACK '▒'
#define SCROLL_THUMB '█'

#define MAX_PROGRAMS 100
#define MAX_PATH_LEN 512
#define MAX_DESC_LEN 1024
#define MAX_COMMAND_LEN 256
#define MAX_NAME_LEN 64

// Window dimensions (doubled from 80x24 to 160x48)
#define WINDOW_WIDTH 160
#define WINDOW_HEIGHT 40

#define FOREGROUND_WHITE (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE)
#define FOREGROUND_GRAY (FOREGROUND_INTENSITY) // Brighter gray for better visibility
#define FOREGROUND_DARK_GRAY (0) // Black, but on a colored background appears gray
#define SELECTED_ITEM_COLOR (BACKGROUND_INTENSITY | FOREGROUND_BLUE)

typedef struct {
    char name[MAX_NAME_LEN];
    char description[MAX_DESC_LEN];
    char command[MAX_COMMAND_LEN];
    char directory[MAX_PATH_LEN];
} Program;

typedef struct {
    Program programs[MAX_PROGRAMS];
    int count;
    int selected;
    int scroll_offset;
} ProgramList;

// Structure to store original console settings
typedef struct {
    CONSOLE_SCREEN_BUFFER_INFO original_screen_info;
    CONSOLE_CURSOR_INFO original_cursor_info;
    COORD original_window_size;
    SMALL_RECT original_window_rect;
    WORD original_attributes;
} OriginalConsoleSettings;

void gotoxy(int x, int y) {
    COORD coord = { (SHORT)x, (SHORT)y };
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
}

void clear_screen() {
    system("cls");
}

void set_color(int color) {
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

// Store original console settings
void store_original_settings(OriginalConsoleSettings* settings) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    
    GetConsoleScreenBufferInfo(hConsole, &settings->original_screen_info);
    GetConsoleCursorInfo(hConsole, &settings->original_cursor_info);
    
    settings->original_window_size = settings->original_screen_info.dwSize;
    settings->original_window_rect = settings->original_screen_info.srWindow;
    settings->original_attributes = settings->original_screen_info.wAttributes;
}

// Restore original console settings
void restore_original_settings(OriginalConsoleSettings* settings) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    
    // Restore window size and buffer size
    SetConsoleScreenBufferSize(hConsole, settings->original_window_size);
    SetConsoleWindowInfo(hConsole, TRUE, &settings->original_window_rect);
    
    // Restore cursor visibility
    SetConsoleCursorInfo(hConsole, &settings->original_cursor_info);
    
    // Restore text attributes (colors)
    SetConsoleTextAttribute(hConsole, settings->original_attributes);
    
    // Clear screen with original settings
    system("cls");
}

// Setup console for doubled size
void setup_console_size() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    
    // Disable window resizing
    HWND hwnd = GetConsoleWindow();
    LONG style = GetWindowLong(hwnd, GWL_STYLE);
    style &= ~WS_SIZEBOX;
    style &= ~WS_MAXIMIZEBOX;
    SetWindowLong(hwnd, GWL_STYLE, style);
    
    // Set buffer size first (must be larger than or equal to window size)
    COORD bufferSize = { WINDOW_WIDTH, WINDOW_HEIGHT + 10 };
    SetConsoleScreenBufferSize(hConsole, bufferSize);
    
    // Set window size
    SMALL_RECT windowRect = { 0, 0, WINDOW_WIDTH - 1, WINDOW_HEIGHT - 1 };
    SetConsoleWindowInfo(hConsole, TRUE, &windowRect);
}

// --- NEW/IMPROVED UI DRAWING FUNCTIONS ---

void draw_title_bar(const char* title) {
    set_color(BACKGROUND_BLUE | FOREGROUND_WHITE);
    for (int i = 0; i < WINDOW_WIDTH; i++) {
        gotoxy(i, 0);
        printf(" ");
    }
    gotoxy((WINDOW_WIDTH - strlen(title)) / 2, 0);
    printf("%s", title);
}

void draw_status_bar(const char* status) {
    set_color(BACKGROUND_BLUE | FOREGROUND_WHITE);
    for (int i = 1; i < WINDOW_WIDTH - 1; i++) {
        gotoxy(i, WINDOW_HEIGHT - 2);
        printf(" ");
    }
    gotoxy(3, WINDOW_HEIGHT - 2);
    printf("%s", status);
}

void draw_borders() {
    set_color(FOREGROUND_GRAY);
    // Main window border
    gotoxy(0, 1); printf(BOX_TL);
    for (int i = 1; i < WINDOW_WIDTH - 1; i++) printf(BOX_H);
    printf(BOX_TR);

    for (int i = 2; i < WINDOW_HEIGHT - 1; i++) {
        gotoxy(0, i); printf(BOX_V);
        gotoxy(WINDOW_WIDTH - 1, i); printf(BOX_V);
    }

    gotoxy(0, WINDOW_HEIGHT - 1); printf(BOX_BL);
    for (int i = 1; i < WINDOW_WIDTH - 1; i++) printf(BOX_H);
    printf(BOX_BR);

    // Vertical separator (doubled from 30 to 60)
    gotoxy(60, 1); printf(BOX_TJ);
    for (int i = 2; i < WINDOW_HEIGHT - 1; i++) {
        gotoxy(60, i);
        printf(BOX_V);
    }
    gotoxy(60, WINDOW_HEIGHT - 1); printf(BOX_BJ);

    // Separator for the footer
    gotoxy(0, WINDOW_HEIGHT - 3); printf(BOX_LJ);
    for (int i = 1; i < 60; i++) printf(BOX_H);
    gotoxy(60, WINDOW_HEIGHT - 3); printf(BOX_RJ);
}

void draw_scrollbar(ProgramList* list) {
    int list_height = WINDOW_HEIGHT - 7; // Adjusted for doubled height
    if (list->count <= list_height) return; // No scrollbar needed

    set_color(FOREGROUND_DARK_GRAY | BACKGROUND_INTENSITY); // Dark gray on light gray
    for (int i = 0; i < list_height; i++) {
        gotoxy(59, 2 + i); // Adjusted for doubled width
        printf("%c", SCROLL_TRACK);
    }

    float thumb_pos_f = (float)list->selected / (list->count - 1);
    int thumb_pos = (int)(thumb_pos_f * (list_height - 1));

    set_color(FOREGROUND_WHITE | BACKGROUND_BLUE); // Bright thumb
    gotoxy(59, 2 + thumb_pos); // Adjusted for doubled width
    printf("%c", SCROLL_THUMB);
}

void draw_program_list(ProgramList* list) {
    int list_height = WINDOW_HEIGHT - 7; // Adjusted for doubled height
    int visible_items = list_height;

    for (int i = 0; i < visible_items; i++) {
        gotoxy(2, 2 + i);
        int program_index = i + list->scroll_offset;
        if (program_index < list->count) {
            if (program_index == list->selected) {
                set_color(FOREGROUND_WHITE | FOREGROUND_INTENSITY);
            } else {
                set_color(FOREGROUND_GRAY);
            }
            printf(" %-55s", list->programs[program_index].name); // Expanded width
        } else {
            // Clear unused lines
            printf("%-56s", " ");
        }
    }
    draw_scrollbar(list);
}

void draw_program_details(ProgramList* list) {
    if (list->count == 0) return;

    Program* current = &list->programs[list->selected];
    
    // Clear previous details (adjusted for doubled width)
    for (int y = 2; y < WINDOW_HEIGHT - 5; y++) {
        gotoxy(62, y);
        printf("%-96s", " "); // Expanded width
    }

    // --- Draw new details ---
    gotoxy(62, 3);
    set_color(FOREGROUND_WHITE | FOREGROUND_INTENSITY);
    printf("Program: %s", current->name);
    
    set_color(FOREGROUND_GRAY);
    gotoxy(62, 5); printf("Description:");
    
    // Word wrap logic for description (expanded area)
    char* desc = _strdup(current->description); // Use a mutable copy
    char* word = strtok(desc, " ");
    int current_x = 62;
    int current_y = 6;

    while(word != NULL) {
        if (current_x + strlen(word) > WINDOW_WIDTH - 2) {
            current_y++;
            current_x = 62;
        }
        if (current_y > WINDOW_HEIGHT - 15) break; // Stop if out of description area
        gotoxy(current_x, current_y);
        printf("%s ", word);
        current_x += strlen(word) + 1;
        word = strtok(NULL, " ");
    }
    free(desc);

    gotoxy(62, WINDOW_HEIGHT - 12);
    set_color(FOREGROUND_WHITE);
    printf("Command:");
    gotoxy(64, WINDOW_HEIGHT - 11);
    set_color(FOREGROUND_GREEN);
    printf("%s", current->command);

    gotoxy(62, WINDOW_HEIGHT - 9);
    set_color(FOREGROUND_WHITE);
    printf("Directory:");
    gotoxy(64, WINDOW_HEIGHT - 8);
    set_color(FOREGROUND_GREEN);
     printf("%s", current->directory);
}

void draw_ui(ProgramList* list) {
    // clear_screen(); // Flicker-free: only redraw what's needed
    draw_title_bar("AutoExec Menu - Compatible Programs");
    draw_borders();
    draw_program_list(list);
    draw_program_details(list);
    draw_status_bar("Use ↑/↓ arrows. ENTER to CD. ESC to Exit.");
}


// --- CORE LOGIC (Mostly Unchanged) ---

int parse_json_file(const char* filename, Program* program) {
    FILE* file = fopen(filename, "r");
    if (!file) return 0;
    char line[512];
    while (fgets(line, sizeof(line), file)) {
        char* trimmed = line;
        while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
        if (strncmp(trimmed, "\"name\"", 6) == 0) sscanf(trimmed, "\"name\": \"%[^\"]\"", program->name);
        else if (strncmp(trimmed, "\"description\"", 13) == 0) sscanf(trimmed, "\"description\": \"%[^\"]\"", program->description);
        else if (strncmp(trimmed, "\"command\"", 9) == 0) sscanf(trimmed, "\"command\": \"%[^\"]\"", program->command);
        else if (strncmp(trimmed, "\"directory\"", 11) == 0) sscanf(trimmed, "\"directory\": \"%[^\"]\"", program->directory);
    }
    fclose(file);
    return 1;
}

void load_programs(ProgramList* list) {
    WIN32_FIND_DATA findData;
    HANDLE hFind = FindFirstFile("*.json", &findData);
    list->count = 0; list->selected = 0; list->scroll_offset = 0;
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && list->count < MAX_PROGRAMS) {
                if (parse_json_file(findData.cFileName, &list->programs[list->count])) list->count++;
            }
        } while (FindNextFile(hFind, &findData));
        FindClose(hFind);
    }
}

int handle_input(ProgramList* list, OriginalConsoleSettings* originalSettings) {
    int key = _getch();
    int list_height = WINDOW_HEIGHT - 7; // Adjusted for doubled height

    if (key == 27) return 1; // ESC exits

    if (key == 224) { // Arrow keys
        key = _getch();
        switch (key) {
            case 72: // Up
                if (list->selected > 0) {
                    list->selected--;
                    if (list->selected < list->scroll_offset) list->scroll_offset = list->selected;
                }
                break;
            case 80: // Down
                if (list->selected < list->count - 1) {
                    list->selected++;
                    if (list->selected >= list->scroll_offset + list_height) list->scroll_offset = list->selected - list_height + 1;
                }
                break;
        }
    } else if (key == 13) { // Enter
        if (list->count > 0) {
            Program* current = &list->programs[list->selected];
            if (strlen(current->directory) > 0) {
                // Save current working directory
                char original_dir[MAX_PATH_LEN];
                _getcwd(original_dir, MAX_PATH_LEN);
                
                // Restore original console settings before launching shell
                restore_original_settings(originalSettings);
                
                _chdir(current->directory);
                printf("%s", current->directory);
                system("cmd.exe"); // Launch a shell in the new directory
                
                // Restore original working directory
                _chdir(original_dir);
                
                // Restore our program's settings after shell exits
                SetConsoleOutputCP(CP_UTF8);
                setup_console_size();
                CONSOLE_CURSOR_INFO cursorInfo;
                GetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &cursorInfo);
                cursorInfo.bVisible = FALSE;
                SetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &cursorInfo);
                
                // Reset console input mode to ensure proper key handling
                HANDLE hInput = GetStdHandle(STD_INPUT_HANDLE);
                SetConsoleMode(hInput, 0);
                
                clear_screen();
            }
        }
    }
    return 0; // Continue
}

int main() {
    // Store original console settings before making changes
    OriginalConsoleSettings originalSettings;
    store_original_settings(&originalSettings);

    // Set console to UTF-8 to properly display box characters
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleTitle("AutoExec Menu");

    // Setup doubled console size
    setup_console_size();

    ProgramList list;
    CONSOLE_CURSOR_INFO cursorInfo;
    GetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &cursorInfo);
    cursorInfo.bVisible = FALSE;
    SetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &cursorInfo);

    load_programs(&list);

    if (list.count == 0) {
        printf("No program definition files (*.json) found in current directory.\nPress any key to exit...");
        _getch();
        restore_original_settings(&originalSettings);
        return 1;
    }

    clear_screen();
    while (1) {
        draw_ui(&list);
        if (handle_input(&list, &originalSettings)) break;
    }
    
    // Restore original console settings before exiting
    restore_original_settings(&originalSettings);
    return 0;
}