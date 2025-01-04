#ifndef SAVE_GAME_H
#define SAVE_GAME_H

#include "game.h"
#include "users.h"

// Structure for saved game
struct SavedGame {
    char name[MAX_STRING_LEN];
    struct Map game_map;
    struct Point character_location;
    int score;
    time_t save_time;
};

// Function declarations
void save_current_game(struct UserManager* manager, struct Map* game_map, 
                      struct Point* character_location, int score);
bool load_saved_game(struct UserManager* manager, struct SavedGame* saved_game);
void list_saved_games(struct UserManager* manager);

#endif