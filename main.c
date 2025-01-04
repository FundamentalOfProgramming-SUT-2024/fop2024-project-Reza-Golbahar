#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "game.h"
#include "users.h"
#include "menu.h"

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

    // Show first menu and handle user login
    first_menu(manager);

    if (manager->current_user != NULL || manager->current_user == NULL) {  
        srand(time(NULL));  // Initialize random seed
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
                case 'n': {
                    // Generate map and set up game
                    struct Map game_map = generate_map();

                    // Set initial character position to spawn point
                    struct Point character_location = game_map.initial_position;
                    int score = 0;
                    bool in_game = true;

                    // Game loop
                    while (in_game) {
                        clear();
                        
                        // Print score and controls
                        mvprintw(0, MAP_WIDTH + 2, "Score: %d", score);
                        mvprintw(2, MAP_WIDTH + 2, "Controls:");
                        mvprintw(3, MAP_WIDTH + 2, "h,j,k,l - Move");
                        mvprintw(4, MAP_WIDTH + 2, "y,u,b,n - Diagonal");
                        mvprintw(5, MAP_WIDTH + 2, "q - Quit");

                        // Print map with current character position
                        for (int y = 0; y < MAP_HEIGHT; y++) {
                            for (int x = 0; x < MAP_WIDTH; x++) {
                                if (y == character_location.y && x == character_location.x) {
                                    mvaddch(y, x, PLAYER);
                                } else {
                                    mvaddch(y, x, game_map.grid[y][x]);
                                }
                            }
                        }
                        refresh();

                        // Handle input
                        char key = getch();
                        if (key == 'q') {
                            in_game = false;
                        } else {
                            struct Point old_pos = character_location;
                            move_character(&character_location, key, &game_map);

                            // Check for food collection
                            if (game_map.grid[character_location.y][character_location.x] == FOOD) {
                                score += 10;
                                game_map.grid[character_location.y][character_location.x] = FLOOR;
                                if (manager->current_user) {
                                    manager->current_user->score += 10;
                                }
                            }
                        }
                    }
                    break;
                }
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
    }

    // Clean up and exit
    save_users_to_json(manager);
    free_user_manager(manager);
    endwin();
    return 0;
}