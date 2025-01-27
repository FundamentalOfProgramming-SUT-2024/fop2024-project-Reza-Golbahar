#ifndef MENU_H
#define MENU_H

#include "users.h"
#include <stdbool.h>
#include "game.h"

bool init_ncurses(void);
void first_menu(struct UserManager* manager);
void pre_game_menu(struct UserManager* manager, Inventory* inventory);
bool entering_menu(struct UserManager* manager, int selected_index);
int users_menu(struct UserManager* manager);
void settings(struct UserManager* manager);
void adding_new_user(struct UserManager* manager);
void login_menu(struct UserManager* manager, Inventory* inventory);
void register_menu(struct UserManager* manager, Inventory* inventory);
void game_menu(struct UserManager* manager);
// void open_inventory_menu(int* food_inventory, int* food_count, int* gold_count,
//                          int* score, int* hunger_rate,
//                          int* ancient_key_count, int* broken_key_count,
//                          int weapon_counts[WEAPON_COUNT]);



#endif