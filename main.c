#include <ncurses.h>
#include <stdlib.h>
#include <time.h>
#include "menu.h"
#include "game.h"
#include "users.h"

bool init_ncurses(void) {
    initscr();
    raw();
    keypad(stdscr, TRUE);
    noecho();
    curs_set(0);
    
    if (!has_colors()) {
        endwin();
        printf("Your terminal does not support color\n");
        return false;
    }
    
    start_color();
    return true;
}

int main() {
    // Initialize ncurses
    initscr();
    noecho();        // Don't echo keypresses
    cbreak();        // Disable line buffering
    keypad(stdscr, TRUE);  // Enable keypad
    curs_set(0);     // Hide cursor
    
    // Initialize random seed with current time
    srand(time(NULL));
    
    // Create user manager
    struct UserManager* manager = create_user_manager();
    if (!manager) {
        endwin();
        fprintf(stderr, "Failed to create user manager\n");
        return 1;
    }

    // Main menu loop
    bool running = true;
    while (running) {
        clear();
        mvprintw(0, 0, "Welcome to the Game!");
        mvprintw(2, 0, "1. Login");
        mvprintw(3, 0, "2. Register");
        mvprintw(4, 0, "3. Play as Guest");
        mvprintw(5, 0, "4. Quit");
        mvprintw(7, 0, "Current Date and Time (UTC): 2025-01-04 19:45:03");
        mvprintw(8, 0, "Please choose an option (1-4): ");
        refresh();

        int choice = getch();

        switch (choice) {
            case '1':
                login_menu(manager);
                break;
            case '2':
                register_menu(manager);
                break;
            case '3':
                // Play as guest
                manager->current_user = NULL;
                pre_game_menu(manager);
                break;
            case '4':
                running = false;
                break;
            default:
                mvprintw(10, 0, "Invalid option. Press any key to continue...");
                refresh();
                getch();
                break;
        }
    }

    // Cleanup
    free_user_manager(manager);
    endwin();
    return 0;
}

void place_windows(struct Map* map, struct Room* room) {
    // Place windows on room walls with a certain chance
    for (int y = room->top_wall + 1; y < room->bottom_wall; y++) {
        if (rand() % 100 < WINDOW_CHANCE) {
            map->grid[y][room->left_wall] = WINDOW;
        }
        if (rand() % 100 < WINDOW_CHANCE) {
            map->grid[y][room->right_wall] = WINDOW;
        }
    }
    
    for (int x = room->left_wall + 1; x < room->right_wall; x++) {
        if (rand() % 100 < WINDOW_CHANCE) {
            map->grid[room->top_wall][x] = WINDOW;
        }
        if (rand() % 100 < WINDOW_CHANCE) {
            map->grid[room->bottom_wall][x] = WINDOW;
        }
    }
}