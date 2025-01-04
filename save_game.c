#include <stdio.h>
#include <string.h>
#include <time.h>
#include "save_game.h"

void save_current_game(struct UserManager* manager, struct Map* game_map, 
                      struct Point* character_location, int score) {
    if (!manager->current_user) {
        mvprintw(0, 0, "Cannot save game as guest user.");
        refresh();
        getch();
        return;
    }

    char filename[MAX_STRING_LEN * 2];
    char save_name[MAX_STRING_LEN];

    // Get save name from user
    clear();
    echo();
    mvprintw(0, 0, "Enter name for this save: ");
    refresh();
    getnstr(save_name, MAX_STRING_LEN - 1);
    noecho();

    // Create filename using username and save name
    snprintf(filename, sizeof(filename), "saves/%s_%s.sav", 
             manager->current_user->username, save_name);

    FILE* file = fopen(filename, "wb");
    if (!file) {
        mvprintw(2, 0, "Error: Could not create save file.");
        refresh();
        getch();
        return;
    }

    // Create save game structure
    struct SavedGame save = {
        .game_map = *game_map,
        .character_location = *character_location,
        .score = score,
        .save_time = time(NULL)
    };
    strncpy(save.name, save_name, MAX_STRING_LEN - 1);

    // Write save data
    fwrite(&save, sizeof(struct SavedGame), 1, file);
    fclose(file);

    mvprintw(2, 0, "Game saved successfully!");
    refresh();
    getch();
}

bool load_saved_game(struct UserManager* manager, struct SavedGame* saved_game) {
    if (!manager->current_user) {
        mvprintw(0, 0, "Cannot load game as guest user.");
        refresh();
        getch();
        return false;
    }

    // List available saves
    clear();
    list_saved_games(manager);
    
    // Get user choice
    mvprintw(20, 0, "Enter save name to load (or 'q' to cancel): ");
    refresh();
    
    char save_name[MAX_STRING_LEN];
    echo();
    getnstr(save_name, MAX_STRING_LEN - 1);
    noecho();

    if (save_name[0] == 'q') return false;

    // Construct filename
    char filename[MAX_STRING_LEN * 2];
    snprintf(filename, sizeof(filename), "saves/%s_%s.sav", 
             manager->current_user->username, save_name);

    // Try to open save file
    FILE* file = fopen(filename, "rb");
    if (!file) {
        mvprintw(22, 0, "Error: Could not find save file.");
        refresh();
        getch();
        return false;
    }

    // Read save data
    fread(saved_game, sizeof(struct SavedGame), 1, file);
    fclose(file);
    return true;
}

void list_saved_games(struct UserManager* manager) {
    if (!manager->current_user) return;

    char cmd[MAX_STRING_LEN * 3];
    snprintf(cmd, sizeof(cmd), "ls saves/%s_*.sav 2>/dev/null", 
             manager->current_user->username);

    FILE* pipe = popen(cmd, "r");
    if (!pipe) return;

    mvprintw(0, 0, "Available saved games:");
    int line = 2;
    char buffer[256];
    
    while (fgets(buffer, sizeof(buffer), pipe)) {
        // Extract save name from filename
        char* start = strrchr(buffer, '_') + 1;
        char* end = strrchr(buffer, '.');
        if (start && end) {
            *end = '\0';
            mvprintw(line++, 0, "%s", start);
        }
    }
    
    pclose(pipe);
    refresh();
}