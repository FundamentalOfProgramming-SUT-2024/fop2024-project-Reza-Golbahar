#include <ncurses.h>
#include <string.h>
#include <stdlib.h>
#include "menu.h"
#include "users.h"
#include "game.h"


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

    init_pair(1, COLOR_RED, COLOR_BLACK);     // Locked doors
    init_pair(2, COLOR_GREEN, COLOR_BLACK);   // Unlocked doors / Player
    init_pair(3, COLOR_YELLOW, COLOR_BLACK);  // Weapons
    init_pair(4, COLOR_RED, COLOR_BLACK);     // Traps
    init_pair(5, COLOR_YELLOW, COLOR_BLACK);  // Warning messages
    init_pair(6, COLOR_CYAN, COLOR_BLACK);    // Enchant Room
    init_pair(7, COLOR_RED, COLOR_BLACK);     // Final fail messages
    init_pair(8, COLOR_MAGENTA, COLOR_BLACK); // Ancient Key
    init_pair(9, COLOR_BLUE, COLOR_BLACK);    // Secret Doors

    // Initialize color pairs for room themes
    init_pair(COLOR_PAIR_ROOM_NORMAL,    COLOR_WHITE,  COLOR_BLACK); // Normal Rooms
    init_pair(COLOR_PAIR_ROOM_ENCHANT,   COLOR_CYAN,   COLOR_BLACK); // Enchant Rooms
    init_pair(COLOR_PAIR_ROOM_TREASURE,  COLOR_YELLOW, COLOR_BLACK); // Treasure Rooms

    // Initialize color pairs for spells
    init_pair(COLOR_PAIR_HEALTH,  COLOR_MAGENTA, COLOR_BLACK); // Health Spell
    init_pair(COLOR_PAIR_SPEED,   COLOR_CYAN,     COLOR_BLACK); // Speed Spell
    init_pair(COLOR_PAIR_DAMAGE,  COLOR_RED,      COLOR_BLACK); // Damage Spell

    init_pair(16, COLOR_RED, COLOR_BLACK);    // Enemies


    return true;
}

// Function to generate a random password
void generate_password(char *password, int length) {
    const char *uppercase = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    const char *lowercase = "abcdefghijklmnopqrstuvwxyz";
    const char *digits = "0123456789";
    const char *special = SPECIAL_CHARACTERS;
    
    int i;
    size_t upper_len = strlen(uppercase);
    size_t lower_len = strlen(lowercase);
    size_t digit_len = strlen(digits);
    size_t special_len = strlen(special);

    srand(time(NULL)); // Seed for randomness

    // Ensure at least one character from each category
    password[0] = uppercase[rand() % upper_len];
    password[1] = lowercase[rand() % lower_len];
    password[2] = digits[rand() % digit_len];
    password[3] = special[rand() % special_len];

    // Fill the rest randomly
    for (i = 4; i < length; i++) {
        int category = rand() % 4;
        if (category == 0)
            password[i] = uppercase[rand() % upper_len];
        else if (category == 1)
            password[i] = lowercase[rand() % lower_len];
        else if (category == 2)
            password[i] = digits[rand() % digit_len];
        else
            password[i] = special[rand() % special_len];
    }

    password[length] = '\0'; // Null-terminate the string
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
        printw("\nPassword (minimum 7 characters, must include uppercase, lowercase, and number):\n");
        printw("Press + to generate random password. Otherwise, press any key to continue: \n");

        refresh();

        noecho();
        char key = getch();
        if (key == '+'){
            // Generate password automatically
            generate_password(password, PASSWORD_LENGTH);
            printw("\nGenerated Password: %s\n", password);
            refresh();
        }        
        else{
            echo(); // Disable echo for password
            printw("\nEnter your password: ");
            if (getnstr(password, sizeof(password) - 1) != OK) {
                printw("\nError reading password. Press any key to try again...");
                refresh();
                getch();
                continue;
            }
        }

        // Validate password length
        if (strlen(password) < 7) {
            printw("\nPassword must be at least 7 characters long.\n");
            printw("Press any key to try again...");
            refresh();
            getch();
            continue;
        }

        // Using the proper password validation functions
        if (!hasupper(password) || !haslower(password) || !hasdigit(password)) {
            printw("\nPassword must contain at least one uppercase letter, one lowercase letter, and one number.\n");
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
        char* at = strchr(email, '@');
        char* dot = strrchr(email, '.');
        if (!at || !dot || at > dot || at == email || dot == at + 1 || !dot[1]) {
            printw("\nInvalid email format. Must be xxx@yyy.zzz\n");
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
                    content[strlen(content) - 2] = ',';  // Replace the closing bracket with a comma
                    fprintf(file, "%s", content);
                    fprintf(file, "\n");
                }
            }

            fprintf(file, "  {\n");
            fprintf(file, "    \"username\": \"%s\",\n", username);
            fprintf(file, "    \"password\": \"%s\",\n", password);
            fprintf(file, "    \"email\": \"%s\",\n", email);
            fprintf(file, "    \"score\": \"0\"\n");
            fprintf(file, "  }\n]");
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
    echo();
    while (!user_id) {
        clear();
        mvprintw(0, 0, "\nPress the number for the user you want to choose. Press [q] to quit.");        
        print_users(manager);
        char input[10];
        mvscanw(manager->user_count + 5, 0, "%s", input);
        if (strcmp(input, "q") == 0){
            clear();
            return 0;
        }
        else{   
            int num_input = atoi(input);
            if (num_input > 0 && num_input <= manager->user_count) {
                user_id = num_input;
                return user_id;
            }
        }
        
    }
    noecho();
    return user_id;
}


bool entering_menu(struct UserManager* manager, int selected_index) {
    while (1) {
        clear();
        printw("Enter the password for the chosen username: ");
        printw("\nPress + if you have forgotten your password.");
        int key = getch();
        if (key=='+'){
            printw("\nEnter your Email: ");
            char email[100];
            scanw("%s", email);
            echo();

            if (strcmp(manager->users[selected_index - 1].email, email) == 0) {
                manager->current_user = &manager->users[selected_index - 1];
                printw("\nEmail is correct.\n");
                refresh();
                getch();
                return true;
            }

            else{
                printw("\nEmail is incorrect. Please try again.\n");
                refresh();
                getch();
            }

        }

        else{
            printw("\nEnter you password: ");
            char password[MAX_STRING_LEN];
            scanw("%s", password);
            echo();

            if (authenticate_user(manager, selected_index - 1, password)) {
                printw("\nPassword is correct.\n");
                refresh();
                getch();
                return true;
            } else {
                printw("\nPassword is incorrect. Please try again.\n");
                refresh();
                getch();
            }
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


void login_menu(struct UserManager* manager) {
    clear();

    int selected_index = users_menu(manager);
    if (selected_index > 0) {
        if (entering_menu(manager, selected_index)) {
            pre_game_menu(manager);
        }
    }

}

void register_menu(struct UserManager* manager) {
    clear();
    adding_new_user(manager);
    if (manager->user_count > 0) {
        manager->current_user = &manager->users[manager->user_count - 1];
        pre_game_menu(manager);
    }
}

void pre_game_menu(struct UserManager* manager) {
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
        
        mvprintw(9, 0, "Current Date and Time (UTC): 2025-01-04 20:00:58");
        mvprintw(11, 0, "Choose an option (1-5): ");
        refresh();

        int choice = getch();
        
        switch (choice) {
            case '1': {
                // Generate the map
                struct Map new_map = generate_map(NULL,1 ,4);
                struct Point start_pos = new_map.initial_position;

                // Create a visibility array for the map (all tiles are initially unexplored)
                bool visible[MAP_HEIGHT][MAP_WIDTH] = {0}; // All tiles hidden initially
                visible[start_pos.y][start_pos.x] = 1; // Make the starting position visible

                // Print the map with the starting location and visibility
                print_map(&new_map, visible, start_pos);

                // Start the game
                play_game(manager, &new_map, &start_pos, 0);
                break;
            }
            case '2':
                //list_saved_games(manager);
                struct SavedGame saved;
                if (load_saved_game(manager, &saved)) {
                    play_game(manager, &saved.game_map, &saved.character_location, saved.score);
                }
                break;
            case '3':
                print_scoreboard(manager);
                break;
            case '4':
                settings(manager);
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
