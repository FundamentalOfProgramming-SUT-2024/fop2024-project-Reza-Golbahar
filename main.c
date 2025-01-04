#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "game.h"

// Define constants
#define MAX_USERS 100
#define MAX_STRING_LEN 100


// Function prototypes
// Core system functions
bool init_ncurses(void);
void handle_file_error(const char* operation);

// UserManager related functions
struct UserManager* create_user_manager(void);
void free_user_manager(struct UserManager* manager);
void load_users_from_json(struct UserManager* manager);
void save_users_to_json(struct UserManager* manager);

// Menu functions
void first_menu(struct UserManager* manager);
void pre_game_menu(struct UserManager* manager);
bool entering_menu(struct UserManager* manager, int selected_index);
int users_menu(struct UserManager* manager);

// User management functions
void adding_new_user(struct UserManager* manager);
bool authenticate_user(struct UserManager* manager, int index, const char* password);

// Display functions
void print_users(struct UserManager* manager);
void print_scoreboard(struct UserManager* manager);
void print_profile(struct UserManager* manager);
void settings(struct UserManager* manager);


// Structure definitions
struct User {
    char username[MAX_STRING_LEN];
    char password[MAX_STRING_LEN];
    char email[MAX_STRING_LEN];
    int score;
};

struct UserManager {
    struct User users[MAX_USERS];
    char usernames[MAX_USERS][MAX_STRING_LEN];
    int user_count;
    struct User* current_user;  // Add pointer to current user
};

// Function to load all users from JSON file
void load_users_from_json(struct UserManager* manager) {
    FILE* file = fopen("users.json", "r");
    if (file == NULL) {
        manager->user_count = 0;
        return;
    }

    char line[256];
    manager->user_count = 0;

    while (fgets(line, sizeof(line), file)) {
        if (strstr(line, "\"username\":") != NULL) {
            struct User* user = &manager->users[manager->user_count];
            
            sscanf(line, " \"username\": \"%[^\"]\",", user->username);
            fgets(line, sizeof(line), file);
            sscanf(line, " \"password\": \"%[^\"]\",", user->password);
            fgets(line, sizeof(line), file);
            sscanf(line, " \"email\": \"%[^\"]\",", user->email);
            fgets(line, sizeof(line), file);
            char score_str[10];
            sscanf(line, " \"score\": \"%[^\"]\"", score_str);
            user->score = atoi(score_str);

            strncpy(manager->usernames[manager->user_count], user->username, MAX_STRING_LEN - 1);
            manager->usernames[manager->user_count][MAX_STRING_LEN - 1] = '\0';
            
            manager->user_count++;
        }
    }
    fclose(file);
}

// Function to save all users back to JSON file
void save_users_to_json(struct UserManager* manager) {
    FILE* file = fopen("users.json", "w");
    if (file == NULL) {
        handle_file_error("write");
        return;
    }

    fprintf(file, "[\n");
    for (int i = 0; i < manager->user_count; i++) {
        fprintf(file, "  {\n");
        fprintf(file, "    \"username\": \"%s\",\n", manager->users[i].username);
        fprintf(file, "    \"password\": \"%s\",\n", manager->users[i].password);
        fprintf(file, "    \"email\": \"%s\",\n", manager->users[i].email);
        fprintf(file, "    \"score\": \"%d\"%s\n", manager->users[i].score,
                i < manager->user_count - 1 ? "," : "");
        fprintf(file, "  }%s\n", i < manager->user_count - 1 ? "," : "");
    }
    fprintf(file, "]\n");
    fclose(file);
}


struct UserManager* create_user_manager(void) {
    struct UserManager* manager = malloc(sizeof(struct UserManager));
    if (manager != NULL) {
        manager->user_count = 0;
        manager->current_user = NULL;
        memset(manager->users, 0, sizeof(struct User) * MAX_USERS);
        load_users_from_json(manager);  // Load existing users
    }
    return manager;
}

// Free UserManager
void free_user_manager(struct UserManager* manager) {
    if (manager != NULL) {
        free(manager);
    }
}


void handle_file_error(const char* operation) {
    clear();
    printw("Error: Could not %s file.\n", operation);
    printw("Press any key to continue...");
    refresh();
    getch();
}

int hasupper(const char password[]) {
    size_t len = strlen(password);
    for (size_t i = 0; i < len; i++) {
        if (password[i] >= 65 && password[i] <= 90)
            return 1;
    }
    return 0;
}

int haslower(const char password[]) {
    size_t len = strlen(password);
    for (size_t i = 0; i < len; i++) {
        if (password[i] >= 97 && password[i] <= 122)
            return 1;
    }
    return 0;
}

int hasdigit(const char password[]) {
    size_t len = strlen(password);
    for (size_t i = 0; i < len; i++) {
        if (password[i] >= 48 && password[i] <= 57)
            return 1;
    }
    return 0;
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


bool validate_password(char *password) {
    if (strlen(password) < 7) {
        printw("Password must be at least 7 characters long\n");
        return false;
    }
    if (!hasupper(password)) {
        printw("Password must contain at least one uppercase letter\n");
        return false;
    }
    if (!haslower(password)) {
        printw("Password must contain at least one lowercase letter\n");
        return false;
    }
    if (!hasdigit(password)) {
        printw("Password must contain at least one number\n");
        return false;
    }
    return true;
}

bool validate_email(const char *email) {
    const char *at = strchr(email, '@');
    if (!at) return false;
    
    const char *dot = strchr(at, '.');
    if (!dot || dot == at + 1 || *(dot + 1) == '\0') {
        printw("Invalid email format. Must be xxxx@yyy.zzz\n");
        return false;
    }
    return true;
}


void print_users(struct UserManager* manager) {
    if (manager->user_count == 0) {
        printw("No users found.\n");
        return;
    }

    mvprintw(2, 0, "Usernames");
    mvprintw(2, 30, "Emails");
    mvprintw(2, 60, "Score");

    for (int i = 0; i < manager->user_count; i++) {
        mvprintw(i + 3, 0, "%d-%s", i + 1, manager->users[i].username);
        mvprintw(i + 3, 30, "%s", manager->users[i].email);
        mvprintw(i + 3, 60, "%d", manager->users[i].score);
    }
}

// Modified users_menu to use UserManager
int users_menu(struct UserManager* manager) {
    int user_id = 0;
    while (!user_id) {
        clear();
        mvprintw(0, 0, "\nPress [+] to add new user. Or not, Press the number for each. Press [q] to quit.");
        print_users(manager);
        char input[10];
        mvscanw(manager->user_count + 5, 0, "%s", input);
        if (strcmp(input, "+") == 0)
            adding_new_user(manager);
        else if (strcmp(input, "q") == 0)
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



bool authenticate_user(struct UserManager* manager, int index, const char* password) {
    if (index < 0 || index >= manager->user_count) {
        return false;
    }
    
    if (strcmp(manager->users[index].password, password) == 0) {
        manager->current_user = &manager->users[index];
        return true;
    }
    return false;
}


bool entering_menu(struct UserManager* manager, int selected_index) {
    while (1) {
        clear();
        printw("Enter the password for the chosen username, or type 'G' if you want to play as a guest user: ");
        char password[MAX_STRING_LEN];
        noecho();
        scanw("%s", password);
        echo();

        if (strcmp(password, "G") == 0) {
            manager->current_user = NULL;  // Guest user
            return true;
        }

        if (authenticate_user(manager, selected_index - 1, password)) {
            printw("Password is correct.\n");
            refresh();
            getch();
            return true;
        } else {
            printw("Password is incorrect. Please try again.\n");
            refresh();
            getch();
        }
    }
}


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

    printw("Settings\n");
    printw("You want to modify the settings? Press [q] to quit and Press Any other key to continue");
    printw("Default Values: Difficulty = 1; Song = Venom; Color: White");
    char command;
    command = getchar();
    if (command == 'q')
        return;

    int difficulty, song;
    char color[20];
    printw("Difficulty [1-10]: ");
    scanw("%d", &difficulty);

    printw("\nCharacter_color: [White - Red - Blue - Green]: ");
    scanw("%s", color);

    printw("\nSong: [1-Venom, 2-Rap God, 3-Hello]:(enter a number 1-3) ");
    scanf("%d", &song);

    // Save settings for current user if logged in
    if (manager->current_user != NULL) {
        FILE* fptr = fopen("load.json", "w");
        if (fptr == NULL) {
            handle_file_error("open settings");
            return;
        }
        
        fprintf(fptr, "[\n");
        fprintf(fptr, "  \"Difficulty\": %d,\n", difficulty);
        fprintf(fptr, "  \"Song\": %d,\n", song);
        fprintf(fptr, "  \"Color\": \"%s\"\n", color);
        fprintf(fptr, "]");
        
        fclose(fptr);
    } else {
        printw("\nSettings won't be saved for guest users.");
        refresh();
        getch();
    }
}



void print_scoreboard(struct UserManager* manager) {
    if (manager->user_count == 0) {
        printw("No users found.\n");
        return;
    }

    // Create temporary array for sorting
    struct User sorted_users[MAX_USERS];
    memcpy(sorted_users, manager->users, sizeof(struct User) * manager->user_count);

    // Sort users by score in descending order
    for (int i = 0; i < manager->user_count - 1; i++) {
        for (int j = i + 1; j < manager->user_count; j++) {
            if (sorted_users[j].score > sorted_users[i].score) {
                struct User temp = sorted_users[i];
                sorted_users[i] = sorted_users[j];
                sorted_users[j] = temp;
            }
        }
    }

    // Print scoreboard
    clear();
    mvprintw(0, 0, "SCOREBOARD");
    mvprintw(2, 0, "Rank");
    mvprintw(2, 10, "Username");
    mvprintw(2, 35, "Score");
    
    // Print horizontal line
    mvprintw(3, 0, "----------------------------------------");

    for (int i = 0; i < manager->user_count; i++) {
        mvprintw(i + 4, 0, "%d", i + 1);
        mvprintw(i + 4, 10, "%s", sorted_users[i].username);
        mvprintw(i + 4, 35, "%d", sorted_users[i].score);
    }

    // Print bottom line
    mvprintw(manager->user_count + 4, 0, "----------------------------------------");
    mvprintw(manager->user_count + 6, 0, "Press any key to continue...");
    refresh();
    getch();
}

void print_profile(struct UserManager* manager) {
    if (manager->current_user == NULL) {
        printw("No user is currently logged in.\n");
        refresh();
        getch();
        return;
    }

    clear();
    int line_num = 0;  // Add this line
    // Print user profile
    mvprintw(0, 0, "User Profile");
    mvprintw(2, 0, "----------------------------------------");
    mvprintw(3, 0, "Username: %s", manager->current_user->username);
    mvprintw(4, 0, "Email: %s", manager->current_user->email);
    mvprintw(5, 0, "Current Score: %d", manager->current_user->score);
    mvprintw(6, 0, "----------------------------------------");
    
    line_num = 7;  // Start settings from line 7
    // Load game settings if they exist
    FILE *settings_file = fopen("load.json", "r");
    if (settings_file != NULL) {
        mvprintw(line_num++, 0, "Current Settings:");
        char settings_line[256];
        while (fgets(settings_line, sizeof(settings_line), settings_file)) {
            mvprintw(line_num++, 0, "%s", settings_line);
        }
        fclose(settings_file);
    } else {
        handle_file_error("open settings");
    }

    mvprintw(line_num + 1, 0, "Press any key to return...");
    refresh();
    getch();
}


void pre_game_menu(struct UserManager* manager) {
    char command;
    while (1) {
        clear();
        refresh();
        printw("1- Press [n] to create New Game.\n");
        printw("2- Press [l] to load the Last Game.\n");
        printw("3- Press [b] to show the Scoreboard.\n");
        printw("4- Press [s] to go to Settings.\n");
        printw("5- Press [p] to go to Profile Menu.\n");
        if (manager->current_user == NULL) {
            printw("Attention: Guest Users can't load game or go to Profile Menu.\n");
        }
        refresh();

        command = getch();

        switch(command) {
            case 'n':
                //run_game();
                break;
            case 'l':
                if (manager->current_user) {
                    //load_game();
                } else {
                    printw("Attention: Guest Users can't load game or go to Profile Menu.\n");
                    printw("\nPress Any Key to Continue.");
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
                    printw("Attention: Guest Users can't load game or go to Profile Menu.\n");
                    printw("\nPress Any Key to Continue.");
                    getch();
                }
                break;
            case 'q':
                return;
            default:
                printw("\nInvalid input: You must enter one of the options above.\n");
                printw("Press Any Key to Continue.\n");
                refresh();
                getch();
                break;
        }    
    }
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
                    connect_rooms(&game_map);  // Connect rooms with corridors and doors

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

                        // Print map
                        for (int y = 0; y < MAP_HEIGHT; y++) {
                            for (int x = 0; x < MAP_WIDTH; x++) {
                                mvaddch(y, x, game_map.grid[y][x]);
                            }
                        }

                        // Print player
                        mvaddch(character_location.y, character_location.x, PLAYER);
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

                            // Only allow movement through corridors, floors, and doors
                            if (game_map.grid[character_location.y][character_location.x] != FLOOR && 
                                game_map.grid[character_location.y][character_location.x] != CORRIDOR && 
                                game_map.grid[character_location.y][character_location.x] != DOOR) {
                                character_location = old_pos;
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