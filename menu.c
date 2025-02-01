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
    init_pair(2, COLOR_GREEN, COLOR_BLACK);   // Unlocked doors / Player // Do not change Player COlor
    init_pair(3, COLOR_YELLOW, COLOR_BLACK);  // Weapons
    init_pair(4, COLOR_RED, COLOR_BLACK);     // Traps
    init_pair(5, COLOR_YELLOW, COLOR_BLACK);  // Warning messages
    init_pair(6, COLOR_CYAN, COLOR_BLACK);    // Enchant Room
    init_pair(7, COLOR_RED, COLOR_BLACK);     // Final fail messages  // Do not change Player COlor
    init_pair(8, COLOR_MAGENTA, COLOR_BLACK); // Ancient Key
    init_pair(9, COLOR_BLUE, COLOR_BLACK);    // Secret Doors  // Do not change Player COlor

    init_pair(16, COLOR_RED, COLOR_BLACK);    // Default enemy
    init_pair(17, COLOR_MAGENTA, COLOR_BLACK); // Demon
    init_pair(18, COLOR_YELLOW, COLOR_BLACK);  // Giant
    init_pair(19, COLOR_GREEN, COLOR_BLACK);   // Snake
    //init_pair(21, COLOR_CYAN, COLOR_BLACK);    // Undead

    // Define new color pairs for items
    init_pair(21, COLOR_MAGENTA, COLOR_BLACK); // Food
    init_pair(22, COLOR_YELLOW, COLOR_BLACK);  // Normal Gold
    init_pair(23, COLOR_WHITE, COLOR_BLACK);   // Black Gold

    init_pair(20, COLOR_WHITE, COLOR_BLACK);  // Do not change Player COlor

    // Initialize color pairs for room themes
    init_pair(COLOR_PAIR_ROOM_NORMAL,    COLOR_WHITE,  COLOR_BLACK); // Normal Rooms
    init_pair(COLOR_PAIR_ROOM_ENCHANT,   COLOR_CYAN,   COLOR_BLACK); // Enchant Rooms
    init_pair(COLOR_PAIR_ROOM_TREASURE,  COLOR_YELLOW, COLOR_BLACK); // Treasure Rooms

    // Initialize color pairs for spells
    init_pair(COLOR_PAIR_HEALTH,  COLOR_MAGENTA, COLOR_BLACK); // Health Spell
    init_pair(COLOR_PAIR_SPEED,   COLOR_CYAN,     COLOR_BLACK); // Speed Spell
    init_pair(COLOR_PAIR_DAMAGE,  COLOR_RED,      COLOR_BLACK); // Damage Spell


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
            generate_password(password, sizeof(password));
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

        // Initialize other user details
        new_user->score = 0;
        new_user->games_completed = 0;
        new_user->gold = 0;
        new_user->first_game_time = time(NULL);
        new_user->last_game_time = time(NULL);
        new_user->difficulty = 1;
        new_user->song = 1;
        strcpy(new_user->character_color, "White");

        // Store in usernames array
        strncpy(manager->usernames[manager->user_count], username, MAX_STRING_LEN - 1);
        manager->usernames[manager->user_count][MAX_STRING_LEN - 1] = '\0';

        new_user->song = 1;  // default = 1
        play_music(new_user->song);

        // Write user data to JSON
        FILE* file = fopen("users.json", "a"); // Open in append mode
        if (file) {
            if (manager->user_count > 0) {
                fprintf(file, ",\n");
            }
            fprintf(file, "  {\n");
            fprintf(file, "    \"username\": \"%s\",\n", username);
            fprintf(file, "    \"password\": \"%s\",\n", password);
            fprintf(file, "    \"email\": \"%s\",\n", email);
            fprintf(file, "    \"score\": \"0\",\n");
            fprintf(file, "    \"gold_collected\": \"0\",\n");
            fprintf(file, "    \"difficulty\": 1,\n");
            fprintf(file, "    \"color\": \"White\",\n");
            fprintf(file, "    \"games_played\": 0,\n");
            fprintf(file, "    \"days_since_first_game\": 0\n");
            fprintf(file, "  }\n");
            fclose(file);
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

    // Check if there's a logged-in user
    if (!manager->current_user) {
        mvprintw(0, 0, "No user logged in!");
        refresh();
        getch();
        return;
    }

    // Display current settings
    mvprintw(0, 0, "Settings for user: %s", manager->current_user->username);
    mvprintw(2, 0, "Current values:");
    mvprintw(3, 0, "Difficulty = %d", manager->current_user->difficulty);
    mvprintw(4, 0, "Color = %s", manager->current_user->character_color);
    mvprintw(5, 0, "Song = %d", manager->current_user->song);

    mvprintw(7, 0, "Modify settings? (y/n)");
    refresh();
    
    if (getch() != 'y') return;

    // Variables for new settings
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

    // Update the current user's settings
    manager->current_user->difficulty = difficulty;
    strncpy(manager->current_user->character_color, color, sizeof(manager->current_user->character_color) - 1);
    manager->current_user->song = song;

    // Save updated settings to JSON
    save_users_to_json(manager);

    // Also optionally: play the new chosen song
    play_music(manager->current_user->song);

    mvprintw(13, 0, "Settings saved successfully!");
    refresh();
    getch();
}

void login_menu(struct UserManager* manager) {
    clear();

    int selected_index = users_menu(manager);
    if (selected_index > 0) {
        if (entering_menu(manager, selected_index)) {
            play_music(manager->current_user->song);  // <-- Play the chosen song
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
        mvprintw(6, 0, "5. Player Profile");
        mvprintw(7, 0, "6. Back to Main Menu");
        
        if (manager->current_user) {
            mvprintw(9, 0, "Logged in as: %s", manager->current_user->username);
        } else {
            mvprintw(9, 0, "Playing as Guest");
        }
        
        mvprintw(10, 0, "Current Date and Time (UTC): 2025-01-04 20:00:58");
        mvprintw(12, 0, "Choose an option (1-6): ");
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
                print_map(&new_map, visible, start_pos, manager);

                // Start the game
                struct SavedGame saved;
                if (load_saved_game(manager, &saved)) {
                    // Overwrite current map and player with saved data
                    struct Map loaded_map = saved.game_map;
                    Player loaded_player = saved.player;
                    int loaded_level = saved.current_level;

                    // Now call play_game with these:
                    play_game(manager, &loaded_map, &loaded_player.location, loaded_player.score);
                    }
                break;
            }
            case '2':
                //list_saved_games(manager);
                struct SavedGame saved;
                if (load_saved_game(manager, &saved)) {
                    //play_game(manager, &saved.game_map, &saved.character_location, saved.score);
                }
                break;
            case '3':
                print_scoreboard(manager);
                break;
            case '4':
                settings(manager);
                break;
            case '5':
                if (manager->current_user) {
                    // Call the print_user_profile function to display the profile of the current user
                    int user_index = manager->current_user - manager->users; // Calculate the user index
                    print_user_profile(manager, user_index);
                } else {
                    mvprintw(13, 0, "No user logged in. Press any key to continue...");
                    refresh();
                    getch();
                }
                break;
            case '6':
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

void print_user_profile(struct UserManager *manager, int user_index) {
    clear(); // Clear the screen before displaying the profile

    struct User *user = &manager->users[user_index];

    printw("User Profile:\n\n");

    // Display username
    printw("Username: %s\n", user->username);
    
    // Display email
    printw("Email: %s\n", user->email);
    
    // Display score
    printw("Score: %d\n", user->score);
    
    // Display number of games completed
    printw("Games Completed: %d\n", user->games_completed);
    
    // Display gold collected
    printw("Gold Collected: %d\n", user->gold);
    
    // Display difficulty
    printw("Difficulty: %d\n", user->difficulty);
    
    // Display song
    printw("Song: %d\n", user->song);
    
    // Display character color
    printw("Character Color: %s\n", user->character_color);
    
    // Display days since first game
    time_t now = time(NULL);
    double seconds_since_first_game = difftime(now, user->first_game_time);
    int days_since_first_game = seconds_since_first_game / (60 * 60 * 24);
    printw("Days Since First Game: %d\n", days_since_first_game);

    // Display last game time
    struct tm *last_game_time = localtime(&user->last_game_time);
    printw("Last Game Time: %s", asctime(last_game_time));

    printw("\nPress any key to return...");
    refresh();
    getch(); // Wait for user to press any key
}

void play_music(int song_choice) {
    // Example: store your audio files in "audio/" directory
    // You must ensure these files exist and your system has a CLI player.
    // For example, on Linux you might have "mpg123" or "cvlc".
    // On Windows, you might use "start /min" or a different approach.

    switch (song_choice) {
        case 1:
            system("cvlc --play-and-exit audio/venom.mp3 &"); 
            // or "start /min wmplayer \"audio\\venom.mp3\""
            break;
        case 2:
            system("cvlc --play-and-exit audio/rap_god.mp3 &"); 
            break;
        case 3:
            system("cvlc --play-and-exit audio/hello.mp3 &"); 
            break;
        default:
            // If an invalid choice, do nothing or default
            break;
    }
}
