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
        printw("Welcome %s!\n", manager->current_user ? manager->current_user->username : "Guest");
        printw("1- Press [n] to create New Game.\n");
        printw("2- Press [l] to load the Last Game.\n");
        printw("3- Press [b] to show the Scoreboard.\n");
        printw("4- Press [s] to go to Settings.\n");
        printw("5- Press [p] to go to Profile Menu.\n");
        printw("6- Press [q] to quit.\n");
        refresh();

        char choice = getch();
        switch(choice) {
            case 'n':
                game_menu(manager);
                break;
            case 'l':
                if (manager->current_user) {
                    printw("\nLoad game functionality not implemented yet.");
                    refresh();
                    getch();
                } else {
                    printw("\nGuests cannot load games.");
                    refresh();
                    getch();
                }
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
                    printw("\nGuests cannot view profile.");
                    refresh();
                    getch();
                }
                break;
            case 'q':
                playing = false;
                break;
        }
    }

    // Clean up and exit
    save_users_to_json(manager);
    free_user_manager(manager);
    endwin();
    return 0;
}