#include <ncurses.h>
#include <string.h>
#include <stdlib.h>
#include "menu.h"
#include "users.h"
#include "game.h"
#include "inventory.h"
#include "weapons.h"


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
    use_default_colors();

    init_pair(1, COLOR_RED,   -1); // قرمز: در قفل
    init_pair(2, COLOR_GREEN, -1); // سبز: در باز
    init_pair(3, COLOR_YELLOW, -1); // زرد: دکمه رمز
    init_pair(4, COLOR_RED,   -1); // For traps
    init_color(200, 1000, 647, 0); // This is just an example if the terminal supports init_color
    init_pair(6, 200, COLOR_BLACK);
    init_pair(7, COLOR_RED, COLOR_BLACK);
    init_pair(5, COLOR_YELLOW, COLOR_BLACK);
    init_pair(8, COLOR_YELLOW, COLOR_BLACK);  // Approx. gold on black //for ancient key
    init_pair(9, COLOR_BLUE, COLOR_BLACK);     // Secret doors as blue '?' symbol

    return true;
}

void adding_new_user(struct UserManager* manager) {
    if (manager->user_count >= MAX_USERS) {
        clear();
        printw("Maximum number of users reached.\n");
        printw("Press any key to continue...");
        refresh();
        getch();
        return;
    }

    while (1) {
        clear();
        echo(); // Enable character echo for input

        // Username input
        printw("Adding New User Menu:\n\n");
        printw("Username (3-20 characters): ");
        refresh();
        
        char username[MAX_STRING_LEN];
        char password[MAX_STRING_LEN];
        char email[MAX_STRING_LEN];

        // Get username
        if (getnstr(username, sizeof(username) - 1) != OK) {
            printw("\nError reading username. Press any key to try again...");
            refresh();
            getch();
            continue;
        }

        // Check username length
        if (strlen(username) < 3 || strlen(username) > 20) {
            printw("\nUsername must be between 3 and 20 characters.\n");
            printw("Press any key to try again...");
            refresh();
            getch();
            continue;
        }

        // Check username uniqueness
        bool unique = true;
        for (int i = 0; i < manager->user_count; i++) {
            if (strcmp(manager->users[i].username, username) == 0) {
                unique = false;
                break;
            }
        }

        if (!unique) {
            printw("\nUsername already taken. Please choose another one.\n");
            printw("Press any key to try again...");
            refresh();
            getch();
            continue;
        }

        // Password input
        printw("\nPassword (minimum 7 characters, must include uppercase, lowercase, and number): ");
        refresh();
        
        noecho(); // Disable echo for password
        if (getnstr(password, sizeof(password) - 1) != OK) {
            printw("\nError reading password. Press any key to try again...");
            refresh();
            getch();
            continue;
        }

        // Validate password
        if (!validate_password(password)) {
            printw("Press any key to try again...");
            refresh();
            getch();
            continue;
        }

        // Email input
        echo(); // Re-enable echo for email
        printw("\nEmail (format: xxx@yyy.zzz): ");
        refresh();
        
        if (getnstr(email, sizeof(email) - 1) != OK) {
            printw("\nError reading email. Press any key to try again...");
            refresh();
            getch();
            continue;
        }

        // Validate email format
        if (!validate_email(email)) {
            printw("Press any key to try again...");
            refresh();
            getch();
            continue;
        }

        // Add the new user to the manager
        struct User* new_user = &manager->users[manager->user_count];
        strncpy(new_user->username, username, MAX_STRING_LEN - 1);
        new_user->username[MAX_STRING_LEN - 1] = '\0'; // Ensure null termination
        strncpy(new_user->password, password, MAX_STRING_LEN - 1);
        new_user->password[MAX_STRING_LEN - 1] = '\0';
        strncpy(new_user->email, email, MAX_STRING_LEN - 1);
        new_user->email[MAX_STRING_LEN - 1] = '\0';
        new_user->score = 0;
        new_user->gold = 0;
        new_user->games_completed = 0;
        new_user->first_game_time = time(NULL);
        new_user->last_game_time = time(NULL);
        new_user->current_level = 1; // Initialize current_level


        // Store in usernames array
        strncpy(manager->usernames[manager->user_count], username, MAX_STRING_LEN - 1);
        manager->usernames[manager->user_count][MAX_STRING_LEN - 1] = '\0';
        
        // Update JSON file
        FILE* file = fopen("users.json", "r");
        char* content = NULL;
        long file_size = 0;

        if (file != NULL) {
            fseek(file, 0, SEEK_END);
            file_size = ftell(file);
            fseek(file, 0, SEEK_SET);
            content = malloc(file_size + 1);
            if (content) {
                fread(content, 1, file_size, file);
                content[file_size] = '\0';
            }
            fclose(file);
        }

        file = fopen("users.json", "w");
        if (file) {
            if (manager->user_count == 0) {
                fprintf(file, "[\n");
            } else {
                if (content) {
                    // Remove the closing bracket
                    content[strlen(content) - 2] = ',';  // Replace the closing bracket with a comma
                    fprintf(file, "%s", content);
                    fprintf(file, "\n");
                }
            }

            fprintf(file, "  {\n");
            fprintf(file, "    \"username\": \"%s\",\n", username);
            fprintf(file, "    \"password\": \"%s\",\n", password);
            fprintf(file, "    \"email\": \"%s\",\n", email);
            fprintf(file, "    \"score\": \"%d\",\n", new_user->score);
            fprintf(file, "    \"gold\": \"%d\",\n", new_user->gold);
            fprintf(file, "    \"games_completed\": \"%d\",\n", new_user->games_completed);
            fprintf(file, "    \"first_game_time\": \"%ld\",\n", new_user->first_game_time);
            fprintf(file, "    \"last_game_time\": \"%ld\"\n", new_user->last_game_time);
            fprintf(file, "  }%s\n]", (manager->user_count < MAX_USERS - 1) ? "," : "");
            fclose(file);
        }

        if (content) {
            free(content);
        }

        manager->user_count++;

        clear();
        printw("User successfully added!\n");
        printw("Press any key to continue...");
        refresh();
        getch();
        break;
    }
}

// Modified users_menu to use UserManager
int users_menu(struct UserManager* manager) {
    int user_id = 0;
    while (!user_id) {
        clear();
        mvprintw(0, 0, "\nPress the number for the user you want to choose. Press [q] to quit.");
        print_users(manager);
        char input[10];
        mvscanw(manager->user_count + 5, 0, "%s", input);
        if (strcmp(input, "q") == 0)
            clear();
        else {
            int num_input = atoi(input);
            if (num_input > 0 && num_input <= manager->user_count) {
                user_id = num_input;
                return user_id;
            }
        }
    }
    return user_id;
}

bool entering_menu(struct UserManager* manager, int selected_index) {
    while (1) {
        clear();
        mvprintw(0, 0, "Enter the password for the chosen username");
        char password[MAX_STRING_LEN];
        noecho();
        mvscanw(2, 0, "%s", password);
        echo();

        if (authenticate_user(manager, selected_index - 1, password)) {
            mvprintw(4, 0, "Password is correct.\n");
            refresh();
            getch();
            return true;
        } else {
            mvprintw(4, 0, "Password is incorrect. Please try again.\n");
            refresh();
            getch();
        }
    }
}

void first_menu(struct UserManager* manager) {
    char input;
    
    while (1) {
        clear();
        refresh();
        
        printw("1- Press [s] to add new User(Sign In).\n");
        printw("2- Press [l] to Log In.\n");
        printw("3- Press [g] to enter as guest user.\n");
        printw("4- Press [q] to quit.\n");
        refresh();

        input = getch();
        
        switch(input) {
            case 's':
                adding_new_user(manager);
                manager->current_user = &manager->users[manager->user_count - 1];
                return;
            case 'l':
                {
                    int selected_index = users_menu(manager);
                    if (selected_index > 0) {
                        if (entering_menu(manager, selected_index)) {
                            return;
                        }
                    }
                }
                break;
            case 'g':
                manager->current_user = NULL;
                return;
            case 'q':
                manager->current_user = NULL;
                return;
            default:
                clear();
                printw("Invalid Input. You must enter l, g, s, or q.\n");
                printw("Press Any Key To Continue...");
                refresh();
                getch();
        }
    }
}


void settings(struct UserManager* manager) {
    clear();
    refresh();

    mvprintw(0, 0, "Settings");
    mvprintw(2, 0, "Current values:");
    mvprintw(3, 0, "Difficulty = 1");
    mvprintw(4, 0, "Song = Venom");
    mvprintw(5, 0, "Color = White");
    
    mvprintw(7, 0, "Modify settings? (y/n)");
    refresh();
    
    if (getch() != 'y') return;

    int difficulty;
    char color[20];
    int song;

    echo();
    mvprintw(9, 0, "Difficulty [1-10]: ");
    scanw("%d", &difficulty);

    mvprintw(10, 0, "Character color [White/Red/Blue/Green]: ");
    scanw("%s", color);

    mvprintw(11, 0, "Song [1-Venom, 2-Rap God, 3-Hello]: ");
    scanw("%d", &song);
    noecho();

    // Save settings for current user
    if (manager->current_user) {
        FILE* file = fopen("settings.json", "w");
        if (file) {
            fprintf(file, "{\n");
            fprintf(file, "  \"username\": \"%s\",\n", manager->current_user->username);
            fprintf(file, "  \"difficulty\": %d,\n", difficulty);
            fprintf(file, "  \"color\": \"%s\",\n", color);
            fprintf(file, "  \"song\": %d\n", song);
            fprintf(file, "}\n");
            fclose(file);
            
            mvprintw(13, 0, "Settings saved successfully!");
        } else {
            mvprintw(13, 0, "Error saving settings!");
        }
    } else {
        mvprintw(13, 0, "Settings won't be saved for guest users.");
    }
    
    refresh();
    getch();
}


void login_menu(struct UserManager* manager, Inventory* inventory) {
    clear();
    int selected_index = users_menu(manager); // Assuming users_menu is implemented
    if (selected_index > 0) {
        if (entering_menu(manager, selected_index)) {
            pre_game_menu(manager, inventory); // Pass Inventory pointer
        }
    }
}

void register_menu(struct UserManager* manager, Inventory* inventory) {
    clear();
    adding_new_user(manager);
    if (manager->user_count > 0) {
        manager->current_user = &manager->users[manager->user_count - 1];
        pre_game_menu(manager, inventory); // Pass Inventory pointer
    }
}


void pre_game_menu(struct UserManager* manager, Inventory* inventory) {
    bool running = true;

    while (running) {
        clear();
        mvprintw(0, 0, "Game Menu");
        mvprintw(2, 0, "1. New Game");
        mvprintw(3, 0, "2. Load Game");
        mvprintw(4, 0, "3. Scoreboard");
        mvprintw(5, 0, "4. Settings");
        mvprintw(6, 0, "5. Back to Main Menu");
        
        if (manager->current_user) {
            mvprintw(8, 0, "Logged in as: %s", manager->current_user->username);
        } else {
            mvprintw(8, 0, "Playing as Guest");
        }
        
        mvprintw(9, 0, "Current Date and Time (UTC): %s", "2025-01-04 20:00:58"); // Replace with actual time if needed
        mvprintw(11, 0, "Choose an option (1-5): ");
        refresh();

        int choice = getch();

        switch (choice) {
            case '1': {
                // Generate the map
                struct Map new_map = generate_map(NULL);
                struct Point start_pos = new_map.initial_position;

                // Add weapons to the map
                add_weapons(&new_map);

                // Start the game
                if (manager->current_user) {
                    play_game(manager, &new_map, &start_pos, manager->current_user->score, manager->current_user->current_level, &manager->current_user->inventory);
                } else {
                    // For guest users, use the passed inventory
                    play_game(manager, &new_map, &start_pos, 0, 1, inventory);
                }
                break;
            }
            case '2': {
                struct SavedGame saved;
                if (load_saved_game(manager, &saved)) {
                    // Start the game from saved state
                    play_game(manager, &saved.game_map, &saved.character_location, saved.score, saved.current_level, &saved.inventory);
                }
                break;
            }
            case '3':
                print_scoreboard(manager); // Assuming print_scoreboard is implemented
                break;
            case '4':
                settings(manager); // Assuming settings is implemented
                break;
            case '5':
                running = false;
                break;
            default:
                mvprintw(13, 0, "Invalid option. Press any key to continue...");
                refresh();
                getch();
                break;
        }
    }
}

