#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "game.h"
#include "users.h"
#include "menu.h"

// Initialize ncurses
bool init_ncurses(void) {
    initscr();
    if (!has_colors()) {
        endwin();
        printf("Your terminal does not support color\n");
        return false;
    }
    start_color();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);  // Hide cursor
    return true;
}

int main() {
    // Initialize ncurses and the user manager
    if (!init_ncurses()) {
        fprintf(stderr, "Failed to initialize ncurses\n");
        return 1;
    }

    struct UserManager* manager = create_user_manager();
    if (manager == NULL) {
        endwin();
        fprintf(stderr, "Failed to create user manager\n");
        return 1;
    }

    // Initialize random seed
    srand(time(NULL));

    // Show first menu and handle user login
    first_menu(manager);

    // Main game loop
    bool playing = true;
    while (playing) {
        clear();
        refresh();
        
        // Print header
        mvprintw(0, 0, "Welcome %s!", manager->current_user ? manager->current_user->username : "Guest");
        
        // Print menu options
        mvprintw(2, 0, "Main Menu:");
        mvprintw(4, 0, "1- Press [n] to create New Game.");
        mvprintw(5, 0, "2- Press [l] to load the Last Game.");
        mvprintw(6, 0, "3- Press [b] to show the Scoreboard.");
        mvprintw(7, 0, "4- Press [s] to go to Settings.");
        mvprintw(8, 0, "5- Press [p] to go to Profile Menu.");
        mvprintw(9, 0, "6- Press [q] to quit.");
        
        if (manager->current_user == NULL) {
            mvprintw(11, 0, "Note: Some features are not available for guests.");
        }
        
        refresh();

        char choice = getch();
        clear();  // Clear screen before handling choice

        switch(choice) {
            case 'n':
                // Start new game
                clear();
                refresh();
                game_menu(manager);
                break;

            case 'l':
                if (manager->current_user) {
                    mvprintw(0, 0, "Load game functionality not implemented yet.");
                } else {
                    mvprintw(0, 0, "Guests cannot load games.");
                }
                refresh();
                getch();
                break;

            case 'b':
                print_scoreboard(manager);
                break;

            case 's':
                settings(manager);
                break;

            case 'p':
                if (manager->current_user) {
                    print_profile(manager);
                } else {
                    mvprintw(0, 0, "Guests cannot view profile.");
                    refresh();
                    getch();
                }
                break;

            case 'q':
                mvprintw(0, 0, "Are you sure you want to quit? (y/n)");
                refresh();
                if (getch() == 'y') {
                    playing = false;
                }
                break;

            default:
                mvprintw(0, 0, "Invalid choice. Press any key to continue...");
                refresh();
                getch();
                break;
        }
    }

    // Clean up and exit
    save_users_to_json(manager);
    free_user_manager(manager);
    endwin();
    return 0;
}