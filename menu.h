#ifndef MENU_H
#define MENU_H

#include "users.h"
#include <stdbool.h>
#include "game.h"


bool init_ncurses(void);
void first_menu(struct UserManager* manager);
void pre_game_menu(struct UserManager* manager);
bool entering_menu(struct UserManager* manager, int selected_index);
int users_menu(struct UserManager* manager);
void settings(struct UserManager* manager);
void adding_new_user(struct UserManager* manager);
void login_menu(struct UserManager* manager);
void register_menu(struct UserManager* manager);
void game_menu(struct UserManager* manager);



#endif