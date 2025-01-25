#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <dirent.h>
#include "game.h"
#include "users.h"
#include "inventory.h"
#include "weapons.h"

bool hasPassword = false;  // The single definition

#define DOOR '+'
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))

#define MIN_ROOM_COUNT 6
#define MIN_WIDTH_OR_LENGTH 6
#define MAX_POINTS 100
#define MAX_ROOMS 10
#define MAP_WIDTH 80
#define MAP_HEIGHT 24


Weapon weapons_list[WEAPON_COUNT];


void play_game(struct UserManager* manager, struct Map* game_map, 
              struct Point* character_location, int score, int current_level, Inventory* inventory) {
    //int current_level = 1;  // Start at level 1
    bool game_running = true;
    //int score = initial_score;

    bool show_map = false;  // Flag to track map visibility


    bool show_map = false;  // Flag to track map visibility


    bool show_map = false;  // Flag to track map visibility


    // Initialize player attributes
    int hitpoints = 100;            // Initial hitpoints
    int hunger_rate = 0;            // Initial hunger rate
    const int MAX_HUNGER = 100;     // Threshold for hunger affecting hitpoints
    const int HUNGER_INCREASE_INTERVAL = 50; // Frames for hunger to increase
    const int HUNGER_DAMAGE_INTERVAL = 20;   // Frames for hitpoint loss due to hunger
    const int HITPOINT_DECREASE_RATE = 5;    // Hitpoints lost due to hunger

    // Timers for hunger and hunger damage
    int frame_count = 0;
    int hunger_damage_timer = 0;

    // Initialize food and gold inventories
    int food_inventory[5] = {0};
    int food_count = 0;
    int gold_count = 0;

    // Initialize visibility array (all tiles hidden initially)
    bool visible[MAP_HEIGHT][MAP_WIDTH] = {0}; 
    visible[character_location->y][character_location->x] = 1; // Start position is visible

    while (game_running) {
        clear();

        if (show_map) {
            // Display the entire map
            print_full_map(game_map, character_location);
        } else {
            // Update visibility based on player's field of view
            update_visibility(game_map, character_location, visible);
            // Display only visible parts of the map
            print_map(game_map, visible, *character_location);
        }

        // Show game info
        mvprintw(MAP_HEIGHT + 1, 0, "Level: %d", current_level);
        mvprintw(MAP_HEIGHT + 2, 0, "Score: %d", score);
        mvprintw(MAP_HEIGHT + 3, 0, "Hitpoints: %d", hitpoints);
        mvprintw(MAP_HEIGHT + 4, 0, "Hunger: %d/%d", hunger_rate, MAX_HUNGER);
        if (manager->current_user) {
            mvprintw(MAP_HEIGHT + 5, 0, "Player: %s", manager->current_user->username);
        } else {
            mvprintw(MAP_HEIGHT + 5, 0, "Player: Guest");
        }
        mvprintw(MAP_HEIGHT + 6, 0, "Use arrow keys to move, 'q' to quit, 'e' for inventory menu, press 'm' for full vision");
        refresh();

        // Increase hunger rate over time
        if (frame_count % HUNGER_INCREASE_INTERVAL == 0) {
            hunger_rate++;
        }

        // Decrease hitpoints if hunger exceeds threshold
        if (hunger_rate >= MAX_HUNGER) {
            if (hunger_damage_timer % HUNGER_DAMAGE_INTERVAL == 0) {
                hitpoints -= HITPOINT_DECREASE_RATE;
            }
            hunger_damage_timer++;
        } else {
            hunger_damage_timer = 0; // Reset damage timer if hunger is below threshold
        }

        // Check if hitpoints are zero
        if (hitpoints <= 0) {
            mvprintw(MAP_HEIGHT + 6, 0, "Game Over! You ran out of hitpoints.");
            refresh();
            getch();
            game_running = false;
            break;
        }

        update_password_display();
        refresh();

        // Handle input
        int key = getch();
        if (key == 'm' || key == 'M') {
            show_map = !show_map;  // Toggle the map visibility flag
            continue;  // Skip the rest of the loop to refresh the display
        }
        switch (key) {
            case KEY_UP:
            case KEY_DOWN:
            case KEY_LEFT:
            case KEY_RIGHT:
                // Move the character
                move_character(character_location, key, game_map, &hitpoints, inventory);

                // Check the tile the player moves onto
                char tile = game_map->grid[character_location->y][character_location->x];

                if (tile == STAIRS) {
                    // Handle level up logic
                    if (current_level < 4) {
                        current_level++;
                        struct Room* current_room = NULL;

                        // Find the current room
                        for (int i = 0; i < game_map->room_count; i++) {
                            if (isPointInRoom(character_location, &game_map->rooms[i])) {
                                current_room = &game_map->rooms[i];
                                break;
                            }
                        }

                        // Generate the next map
                        struct Map new_map = generate_map(current_room);
                        *game_map = new_map;
                        *character_location = new_map.initial_position;

                        mvprintw(MAP_HEIGHT + 5, 0, "Level up! Welcome to Level %d.", current_level);
                        refresh();
                        getch();
                    } else {
                        mvprintw(MAP_HEIGHT + 3, 0, "Congratulations! You've completed all levels.");
                        refresh();
                        getch();
                        game_running = false;
                    }
                } else if (tile == FOOD) {
                    // Collect food
                    if (food_count < 5) {
                        game_map->grid[character_location->y][character_location->x] = FLOOR;
                        food_inventory[food_count++] = 1;
                    }
                } else if (tile == GOLD) {
                    // Collect gold
                    game_map->grid[character_location->y][character_location->x] = FLOOR;
                    gold_count++;
                    score += 10;
                } else if (tile == FLOOR) {
                    // Check for traps
                    for (int i = 0; i < game_map->trap_count; i++) {
                        Trap* trap = &game_map->traps[i];
                        if (trap->location.x == character_location->x && 
                            trap->location.y == character_location->y) {
                            if (!trap->triggered) {
                                trap->triggered = true;
                                game_map->grid[character_location->y][character_location->x] = TRAP_SYMBOL;
                                mvprintw(MAP_HEIGHT + 6, 0, "You triggered a trap! Hitpoints decreased.");
                                hitpoints -= 10;
                                refresh();
                                getch();
                            }
                            break;
                        }
                    }
                }
                break;

            case 'e':
            case 'E':
                // Open inventory menu
                open_inventory_menu(inventory->food_inventory, &inventory->food_count, &inventory->gold_count, 
                                    &score, &hunger_rate, &inventory->ancient_key_count, 
                                    &inventory->broken_key_count, inventory->weapon_counts);
                break;

            case 'i':
            case 'I':
                // Display weapons inventory
                display_weapons(inventory);
                break;

            case 'q':
                game_running = false;
                break;
        }

        update_password_display();

        // Increment frame count
        frame_count++;
    }
}

<<<<<<< HEAD
<<<<<<< HEAD
=======
// game.c

>>>>>>> 0294fab (m vision okay)
=======
// game.c

>>>>>>> 0294fab (m vision okay)
void print_full_map(struct Map* game_map, struct Point* character_location) {
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            char tile = game_map->grid[y][x];

            // Draw the player
            if (character_location->x == x && character_location->y == y) {
                mvaddch(y, x, PLAYER_CHAR);
                continue;
            }

            // Apply color and print based on tile type
            if (tile == DOOR_PASSWORD) {
                Room* door_room = find_room_by_position(game_map, x, y);
                if (door_room && door_room->password_unlocked) {
                    // Green for unlocked door
                    attron(COLOR_PAIR(2));
                    mvaddch(y, x, '@');
                    attroff(COLOR_PAIR(2));
                } else {
                    // Red for locked door
                    attron(COLOR_PAIR(1));
                    mvaddch(y, x, '@');
                    attroff(COLOR_PAIR(1));
                }
            }
            else if (tile == TRAP_SYMBOL) {
                // Red for triggered traps
                attron(COLOR_PAIR(4));
                mvaddch(y, x, tile);
                attroff(COLOR_PAIR(4));
            }
            else if (tile == ANCIENT_KEY) {
                // Golden yellow for Ancient Key
                attron(COLOR_PAIR(8));
                mvaddstr(y, x, "▲");  // Unicode symbol
                attroff(COLOR_PAIR(8));
            }
            else if (tile == SECRET_DOOR_CLOSED) {
                // Print as horizontal wall
                mvaddch(y, x, WALL_HORIZONTAL);
            }
            else if (tile == SECRET_DOOR_REVEALED) {
                // Print as '?', with distinct color
                attron(COLOR_PAIR(9));
                mvaddch(y, x, '?');
                attroff(COLOR_PAIR(9));
            }
            else {
                // Normal tile printing with default color
                mvaddch(y, x, tile);
            }
        }
    }

    // Optionally, display a message indicating that the full map is being shown
    mvprintw(MAP_HEIGHT + 7, 0, "Full map displayed. Press 'm' to hide.");
}
<<<<<<< HEAD
<<<<<<< HEAD
=======


>>>>>>> 0294fab (m vision okay)
=======


>>>>>>> 0294fab (m vision okay)

struct Map generate_map(struct Room* previous_room) {
    struct Map map;
    init_map(&map);

    // Check if this is the first map or a new level
    if (previous_room != NULL) {
        // Copy the previous room dimensions and location
        struct Room new_room = {
            .left_wall = previous_room->left_wall,
            .right_wall = previous_room->right_wall,
            .top_wall = previous_room->top_wall,
            .bottom_wall = previous_room->bottom_wall,
            .width = previous_room->width,
            .height = previous_room->height,
            .has_password_door = false,
            .password_unlocked   = false,
            .password_active     = false,
            .password_gen_time   = 0,
            .door_code[0]        = '\0', // empty string initially
            .door_count = 0,
            .has_stairs = true,
            .visited = false
        };

        // Place the preserved room in the new map
        place_room(&map, &new_room);

        if (rand() % 100 < 30) { // 30% chance each room has a generator
            for (int i = 0; i < map.room_count; i++) {
                place_password_generator_in_corner(&map, &map.rooms[i]);
            }
        }

        // Set stairs in the same relative position
        int stairs_x = new_room.left_wall + (previous_room->width / 2);
        int stairs_y = new_room.top_wall + (previous_room->height / 2);
        map.grid[stairs_y][stairs_x] = STAIRS;

        // Save stairs location and initial player position
        map.stairs_location = (struct Point){stairs_x, stairs_y};
        map.initial_position = map.stairs_location;

        // Add the preserved room to the map's room list
        map.rooms[map.room_count++] = new_room;
    }

    // Generate additional rooms
    int num_rooms = MIN_ROOMS + rand() % (MAX_ROOMS - MIN_ROOMS + 1);
    for (int i = (previous_room ? 1 : 0); i < num_rooms; i++) {  // Skip first room if previous_room exists
        struct Room room;
        int attempts = 0;
        bool placed = false;

        while (!placed && attempts < 100) {
            room.width = MIN_ROOM_SIZE + rand() % (MAX_ROOM_SIZE - MIN_ROOM_SIZE + 1);
            room.height = MIN_ROOM_SIZE + rand() % (MAX_ROOM_SIZE - MIN_ROOM_SIZE + 1);
            room.left_wall = 1 + rand() % (MAP_WIDTH - room.width - 2);
            room.top_wall = 1 + rand() % (MAP_HEIGHT - room.height - 2);
            room.right_wall = room.left_wall + room.width;
            room.bottom_wall = room.top_wall + room.height;
            room.door_count = 0;
            room.has_stairs = false;
            room.visited = false;

            if (is_valid_room_placement(&map, &room)) {
                map.rooms[map.room_count++] = room;
                place_room(&map, &room);
                placed = true;
            }
            attempts++;
        }
    }

    // Connect rooms with corridors
    connect_rooms_with_corridors(&map);

    place_secret_doors(&map);

    // Spawn food, gold, and traps
    add_food(&map);
    add_gold(&map);
    add_traps(&map);

    add_ancient_key(&map);

    place_stairs(&map);

    // Set initial player position for the first map
    if (previous_room == NULL && map.room_count > 0) {
        map.initial_position.x = map.rooms[0].left_wall + map.rooms[0].width / 2;
        map.initial_position.y = map.rooms[0].top_wall + map.rooms[0].height / 2;
    }

    return map;
}

void place_secret_doors(struct Map* map) {
    for (int i = 0; i < map->room_count; i++) {
        struct Room* room = &map->rooms[i];
        
        // Check if this room is a 'dead-end' (exactly 1 door).
        if (room->door_count == 1) {
            // Try placing one secret door on a random wall tile
            // (similar to how you place normal doors).
            bool placed_secret = false;
            int attempts = 0;

            while (!placed_secret && attempts < 50) {
                attempts++;
                int wall = rand() % 4;  // 0: top, 1: bottom, 2: left, 3: right
                int x = 0, y = 0;

                switch (wall) {
                    case 0: // top
                        x = room->left_wall + 1 + rand() % (room->width - 2);
                        y = room->top_wall;
                        break;
                    case 1: // bottom
                        x = room->left_wall + 1 + rand() % (room->width - 2);
                        y = room->bottom_wall;
                        break;
                    case 2: // left
                        x = room->left_wall;
                        y = room->top_wall + 1 + rand() % (room->height - 2);
                        break;
                    case 3: // right
                        x = room->right_wall;
                        y = room->top_wall + 1 + rand() % (room->height - 2);
                        break;
                }

                // Only place if currently a wall
                if (map->grid[y][x] == WALL_HORIZONTAL || 
                    map->grid[y][x] == WALL_VERTICAL) {

                    // Set the tile to a hidden secret door
                    map->grid[y][x] = SECRET_DOOR_CLOSED;
                    placed_secret = true;
                }
            }
        }
    }
}

void open_inventory_menu(int* food_inventory, int* food_count, int* gold_count,
                         int* score, int* hunger_rate,
                         int* ancient_key_count, int* broken_key_count,
                         int weapon_counts[WEAPON_COUNT]){
    bool menu_open = true;

    while (menu_open) {
        clear();
        mvprintw(0, 0, "Inventory Menu");

        // 1) Show food inventory
        mvprintw(2, 0, "Food Inventory:");
        for (int i = 0; i < 5; i++) {
            if (food_inventory[i] == 1) {
                mvprintw(4 + i, 0, "Slot %d: Food", i + 1);
            } else {
                mvprintw(4 + i, 0, "Slot %d: Empty", i + 1);
            }
        }

        // 2) Show standard info
        mvprintw(10, 0, "Gold Collected: %d", *gold_count);
        mvprintw(11, 0, "Hunger: %d", *hunger_rate);

        // 3) Show Ancient Key info
        mvprintw(13, 0, "Ancient Keys (working): %d", *ancient_key_count);
        mvprintw(14, 0, "Broken Key Pieces: %d", *broken_key_count);

        // 4) Show Weapons Inventory
        mvprintw(16, 0, "Weapons Inventory:");
        for (int i = WEAPON_MACE; i < WEAPON_COUNT; i++) {
            Weapon weapon = get_weapon_by_type((WeaponType)i);
            mvprintw(18 + (i - WEAPON_MACE), 0, "%lc %s: %d", weapon.symbol,
                     (i == WEAPON_MACE) ? "Mace" :
                     (i == WEAPON_DAGGER) ? "Dagger" :
                     (i == WEAPON_MAGIC_WAND) ? "Magic Wand" :
                     (i == WEAPON_NORMAL_ARROW) ? "Normal Arrow" :
                     (i == WEAPON_SWORD) ? "Sword" : "Unknown",
                     weapon_counts[i]);
        }

        // 5) Instructions
        mvprintw(24, 0, "Press 'u' + slot number (1-5) to use food, 'c' to combine 2 broken keys, 'q' to quit.");
        refresh();

        // Handle input
        int key = getch();
        switch (key) {
            case 'q':
                // Exit menu
                menu_open = false;
                break;

            case 'u': {
                // Using food from a slot
                mvprintw(26, 0, "Enter slot number (1-5): ");
                refresh();
                int slot = getch() - '0'; // Convert char to integer

                if (slot >= 1 && slot <= 5 && food_inventory[slot - 1] == 1) {
                    // Use food
                    food_inventory[slot - 1] = 0;
                    (*food_count)--;
                    *hunger_rate = (*hunger_rate > 20) ? (*hunger_rate - 20) : 0;

                    mvprintw(28, 0, "You used food from slot %d! Hunger decreased.", slot);
                } else {
                    mvprintw(28, 0, "Invalid slot or no food in slot!");
                }
                refresh();
                getch(); // Wait for player to acknowledge
            } break;

            case 'c': {
                // Combine two broken key pieces into one working Ancient Key
                if (*broken_key_count >= 2) {
                    *broken_key_count -= 2;
                    (*ancient_key_count)++;
                    mvprintw(26, 0, "You combined two broken key pieces into one working Ancient Key!");
                } else {
                    mvprintw(26, 0, "Not enough broken key pieces to combine!");
                }
                refresh();
                getch(); // Wait for player
            } break;

            default:
                // Ignore other keys
                break;
        }
    }
}

void init_weapons() {
    weapons_list[WEAPON_NONE].type = WEAPON_NONE;
    weapons_list[WEAPON_NONE].symbol = L' '; // No weapon
    
    weapons_list[WEAPON_MACE].type = WEAPON_MACE;
    weapons_list[WEAPON_MACE].symbol = L'\u2692'; // ⚒️

    weapons_list[WEAPON_DAGGER].type = WEAPON_DAGGER;
    weapons_list[WEAPON_DAGGER].symbol = L'\U0001F5E1'; // 🗡️

    weapons_list[WEAPON_MAGIC_WAND].type = WEAPON_MAGIC_WAND;
    weapons_list[WEAPON_MAGIC_WAND].symbol = L'\U0001FA84'; // 🪄

    weapons_list[WEAPON_NORMAL_ARROW].type = WEAPON_NORMAL_ARROW;
    weapons_list[WEAPON_NORMAL_ARROW].symbol = L'\u27B3'; // ➳

    weapons_list[WEAPON_SWORD].type = WEAPON_SWORD;
    weapons_list[WEAPON_SWORD].symbol = L'\u2694'; // ⚔️
}

// Retrieve weapon by type
Weapon get_weapon_by_type(WeaponType type) {
    if (type >= WEAPON_NONE && type < WEAPON_COUNT) {
        return weapons_list[type];
    }
    // Return a default weapon if type is invalid
    Weapon default_weapon;
    default_weapon.type = WEAPON_NONE;
    default_weapon.symbol = L' ';
    return default_weapon;
}

// Add a weapon to the inventory
void add_weapon(Inventory* inventory, WeaponType type) {
    if (type <= WEAPON_NONE || type >= WEAPON_COUNT) return;
    inventory->weapon_counts[type]++;
}

// Display weapons in the inventory
void display_weapons(Inventory* inventory) {
    clear();
    mvprintw(0, 0, "Weapons Inventory:");
    int line = 2;
    
    for (int i = WEAPON_MACE; i < WEAPON_COUNT; i++) {
        Weapon weapon = get_weapon_by_type((WeaponType)i);
        mvprintw(line++, 0, "%lc %s: %d", weapon.symbol,
                 (i == WEAPON_MACE) ? "Mace" :
                 (i == WEAPON_DAGGER) ? "Dagger" :
                 (i == WEAPON_MAGIC_WAND) ? "Magic Wand" :
                 (i == WEAPON_NORMAL_ARROW) ? "Normal Arrow" :
                 (i == WEAPON_SWORD) ? "Sword" : "Unknown",
                 inventory->weapon_counts[i]);
    }
    
    mvprintw(line + 1, 0, "Press any key to return.");
    refresh();
    getch();
}

// Function to add weapons randomly on the map
void add_weapons(struct Map* game_map) {
    // Define the number of each weapon type to place
    int weapons_to_place[WEAPON_COUNT] = {0, 2, 3, 1, 4, 2}; // Example counts

    for (int i = WEAPON_MACE; i < WEAPON_COUNT; i++) {
        for (int j = 0; j < weapons_to_place[i]; j++) {
            while (1) {
                int x = rand() % MAP_WIDTH;
                int y = rand() % MAP_HEIGHT;
                
                // Place weapons only on floor tiles
                if (game_map->grid[y][x] == FLOOR) {
                    Weapon weapon = get_weapon_by_type((WeaponType)i);
                    game_map->grid[y][x] = weapon.symbol; // Assign wchar_t directly
                    break;
                }
            }
        }
    }
}

void add_traps(struct Map* game_map) {
    const int TRAP_COUNT = 10; // Number of traps to add
    int traps_placed = 0;

    while (traps_placed < TRAP_COUNT) {
        int x = rand() % MAP_WIDTH;
        int y = rand() % MAP_HEIGHT;

        // Place traps only on floor tiles within rooms
        bool inside_room = false;
        for (int i = 0; i < game_map->room_count; i++) {
            if (isPointInRoom(&(struct Point){x, y}, &game_map->rooms[i])) {
                inside_room = true;
                break;
            }
        }

        if (inside_room && game_map->grid[y][x] == FLOOR) {
            game_map->grid[y][x] = FLOOR; // Initially display as a normal floor
            game_map->traps[traps_placed].location = (struct Point){x, y};
            game_map->traps[traps_placed].triggered = false;
            traps_placed++;
            game_map->trap_count = traps_placed;
        }
    }
}

// void open_inventory_menu(int* food_inventory, int* food_count, int* gold_count,
//                          int* score, int* hunger_rate,
//                          int* ancient_key_count, int* broken_key_count){
//     bool menu_open = true;

//     while (menu_open) {
//         clear();
//         mvprintw(0, 0, "Inventory Menu");

//         // 1) Show food inventory
//         mvprintw(2, 0, "Food Inventory:");
//         for (int i = 0; i < 5; i++) {
//             if (food_inventory[i] == 1) {
//                 mvprintw(4 + i, 0, "Slot %d: Food", i + 1);
//             } else {
//                 mvprintw(4 + i, 0, "Slot %d: Empty", i + 1);
//             }
//         }

//         // 2) Show standard info
//         mvprintw(10, 0, "Gold Collected: %d", *gold_count);
//         mvprintw(11, 0, "Hunger: %d", *hunger_rate);

//         // 3) Show Ancient Key info
//         //    The working keys and broken key pieces
//         mvprintw(13, 0, "Ancient Keys (working): %d", *ancient_key_count);
//         mvprintw(14, 0, "Broken Key Pieces: %d", *broken_key_count);

//         // 4) Instructions
//         mvprintw(16, 0, "Press 'u' + slot number (1-5) to use food, 'c' to combine 2 broken keys, 'q' to quit.");
//         refresh();

//         // Handle input
//         int key = getch();
//         switch (key) {
//             case 'q':
//                 // Exit menu
//                 menu_open = false;
//                 break;

//             case 'u': {
//                 // Using food from a slot
//                 mvprintw(18, 0, "Enter slot number (1-5): ");
//                 refresh();
//                 int slot = getch() - '0'; // Convert char to integer

//                 if (slot >= 1 && slot <= 5 && food_inventory[slot - 1] == 1) {
//                     // Use food
//                     food_inventory[slot - 1] = 0;
//                     (*food_count)--;
//                     *hunger_rate = MAX(*hunger_rate - 20, 0);

//                     mvprintw(20, 0, "You used food from slot %d! Hunger decreased.", slot);
//                 } else {
//                     mvprintw(20, 0, "Invalid slot or no food in slot!");
//                 }
//                 refresh();
//                 getch(); // Wait for player to acknowledge
//             } break;

//             case 'c': {
//                 // Combine two broken key pieces into one working Ancient Key
//                 if (*broken_key_count >= 2) {
//                     *broken_key_count -= 2;
//                     (*ancient_key_count)++;
//                     mvprintw(18, 0, "You combined two broken key pieces into one working Ancient Key!");
//                 } else {
//                     mvprintw(18, 0, "Not enough broken key pieces to combine!");
//                 }
//                 refresh();
//                 getch(); // Wait for player
//             } break;

//             default:
//                 // Ignore other keys
//                 break;
//         }
//     }
// }

void print_point(struct Point p, const char* type) {
    if (p.y < 0 || p.x < 0 || p.y >= MAP_HEIGHT || p.x >= MAP_WIDTH) return;

    char symbol;
    if (strcmp(type, "wall") == 0) symbol = '#';
    else if (strcmp(type, "floor") == 0) symbol = '.';
    else if (strcmp(type, "trap") == 0) symbol = '!';
    else symbol = ' ';

    mvaddch(p.y, p.x, symbol);
}

void sight_range(struct Map* game_map, struct Point* character_location) {
    // Arrays to track visited and currently visible areas
    static char visited[MAP_HEIGHT][MAP_WIDTH] = {0};
    static char visible[MAP_HEIGHT][MAP_WIDTH] = {0};

    // Reset visibility for the new frame
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            visible[y][x] = 0;
        }
    }

    // Mark the current position as visited
    visited[character_location->y][character_location->x] = 1;

    // Find if the player is in a room
    struct Room* current_room = NULL;
    for (int i = 0; i < game_map->room_count; i++) {
        if (isPointInRoom(character_location, &game_map->rooms[i])) {
            current_room = &game_map->rooms[i];
            break;
        }
    }

    if (current_room != NULL) {
        // Player is in a room - reveal the entire current room
        showRoom(game_map, current_room, visible);

        // Reveal corridors and adjacent rooms that are connected to this room
        for (int y = current_room->top_wall; y <= current_room->bottom_wall; y++) {
            for (int x = current_room->left_wall; x <= current_room->right_wall; x++) {
                if (game_map->grid[y][x] == CORRIDOR) {
                    // Mark corridor as visible
                    visible[y][x] = 1;

                    // Check for adjacent rooms connected to this corridor
                    for (int i = 0; i < game_map->room_count; i++) {
                        struct Room* adjacent_room = &game_map->rooms[i];
                        if (adjacent_room != current_room && isPointInRoom(&(struct Point){x, y}, adjacent_room)) {
                            showRoom(game_map, adjacent_room, visible);
                        }
                    }
                }
            }
        }
    } else {
        // Player is in a corridor or open area - show within sight range
        for (int dy = -SIGHT_RANGE; dy <= SIGHT_RANGE; dy++) {
            for (int dx = -SIGHT_RANGE; dx <= SIGHT_RANGE; dx++) {
                int new_y = character_location->y + dy;
                int new_x = character_location->x + dx;

                // Check bounds
                if (new_y >= 0 && new_y < MAP_HEIGHT && new_x >= 0 && new_x < MAP_WIDTH) {
                    // Show if within range or previously visited
                    if (visited[new_y][new_x] ||
                        manhattanDistance(character_location->x, character_location->y, new_x, new_y) <= SIGHT_RANGE) {
                        visible[new_y][new_x] = 1;
                    }
                }
            }
        }
    }

    // Display the map with visibility
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            if (visible[y][x]) {
                // Show actual map content
                mvaddch(y, x, game_map->grid[y][x]);
            } else {
                // Show fog of war for unexplored areas
                mvaddch(y, x, ' ');
            }
        }
    }
}

void add_food(struct Map* game_map) {
    const int FOOD_COUNT = 10;  // Adjust as needed
    int food_placed = 0;
    
    while (food_placed < FOOD_COUNT) {
        int x = rand() % MAP_WIDTH;
        int y = rand() % MAP_HEIGHT;
        
        // Place food only on floor tiles
        if (game_map->grid[y][x] == FLOOR) {
            game_map->grid[y][x] = FOOD;
            food_placed++;
        }
    }
}

void showCorridorVisibility(struct Map* game_map, int x, int y) {
    // Use SIGHT_RANGE directly from game.h
    for (int dy = -SIGHT_RANGE; dy <= SIGHT_RANGE; dy++) {
        for (int dx = -SIGHT_RANGE; dx <= SIGHT_RANGE; dx++) {
            int new_y = y + dy;
            int new_x = x + dx;
            
            if (new_y >= 0 && new_y < MAP_HEIGHT && new_x >= 0 && new_x < MAP_WIDTH) {
                if (abs(dx) + abs(dy) <= SIGHT_RANGE) {
                    game_map->visibility[new_y][new_x] = true;
                }
            }
        }
    }
}

void print_map(struct Map* game_map,
              bool visible[MAP_HEIGHT][MAP_WIDTH],
              struct Point character_location) {
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {

            // Determine the tile to print
            char tile = game_map->grid[y][x];

            // Draw the player
            if (character_location.x == x && character_location.y == y) {
                mvaddch(y, x, PLAYER_CHAR);
                continue;
            }

            // Determine visibility
            bool should_draw = false;
            if (visible[y][x]) {
                should_draw = true;
            } else {
                // Check if it's in a visited room or discovered
                bool is_in_visited_room = false;
                for (int i = 0; i < game_map->room_count; i++) {
                    if (game_map->rooms[i].visited &&
                        isPointInRoom(&(struct Point){x, y}, &game_map->rooms[i])) {
                        is_in_visited_room = true;
                        break;
                    }
                }
                if (is_in_visited_room || game_map->discovered[y][x]) {
                    should_draw = true;
                }
            }

            if (!should_draw) {
                // Not visible and not discovered => print blank/fog
                mvaddch(y, x, ' ');
                continue;
            }

            // Apply color and print based on tile type
            if (tile == DOOR_PASSWORD) {
                Room* door_room = find_room_by_position(game_map, x, y);
                if (door_room && door_room->password_unlocked) {
                    // Green for unlocked door
                    attron(COLOR_PAIR(2));
                    mvaddch(y, x, '@');
                    attroff(COLOR_PAIR(2));
                } else {
                    // Red for locked door
                    attron(COLOR_PAIR(1));
                    mvaddch(y, x, '@');
                    attroff(COLOR_PAIR(1));
                }
            }
            else if (tile == TRAP_SYMBOL) {
                // Red for triggered traps
                attron(COLOR_PAIR(4));
                mvaddch(y, x, tile);
                attroff(COLOR_PAIR(4));
            }
            else if (tile == ANCIENT_KEY) {
                // Golden yellow for Ancient Key
                attron(COLOR_PAIR(8));               // Use color pair 8 for golden yellow
                mvaddstr(y, x, "▲");                  // Print the Unicode symbol
                attroff(COLOR_PAIR(8));
            }
            else if (tile == SECRET_DOOR_CLOSED) {
                // Print as wall (e.g., '-')
                mvaddch(y, x, WALL_HORIZONTAL);        // Or WALL_VERTICAL based on your wall type
            }
            else if (tile == SECRET_DOOR_REVEALED) {
                // Print as '?', with distinct color
                attron(COLOR_PAIR(9));                // Use color pair 9 for secret doors
                mvaddch(y, x, '?');
                attroff(COLOR_PAIR(9));
            }
            else {
                // Normal tile printing
                mvaddch(y, x, tile);
            }
        }
    }
}

Room* find_room_by_position(struct Map* map, int x, int y) {
    for (int i = 0; i < map->room_count; i++) {
        Room* rm = &map->rooms[i];
        if (x >= rm->left_wall && x <= rm->right_wall &&
            y >= rm->top_wall && y <= rm->bottom_wall)
        {
            return rm;
        }
    }
    return NULL;  // not in any room
}

void connect_rooms_with_corridors(struct Map* map) {
    bool connected[MAX_ROOMS] = { false };
    connected[0] = true;

    // For each unconnected room, connect it to the nearest connected room
    for (int count = 1; count < map->room_count; count++) {
        int best_src = -1, best_dst = -1;
        int min_distance = INT_MAX;

        // Find the closest pair of (connected) and (unconnected) rooms
        for (int i = 0; i < map->room_count; i++) {
            if (!connected[i]) continue;
            
            // Grab a pointer to the "source" room
            struct Room* room1 = &map->rooms[i];  

            for (int j = 0; j < map->room_count; j++) {
                if (connected[j]) continue;
                
                // Grab a pointer to the "destination" (unconnected) room
                struct Room* room2 = &map->rooms[j];

                // Now you can safely do:
                int rx1 = (room1->left_wall + room1->right_wall) / 2;
                int ry1 = (room1->top_wall + room1->bottom_wall) / 2;
                int rx2 = (room2->left_wall + room2->right_wall) / 2;
                int ry2 = (room2->top_wall + room2->bottom_wall) / 2;

                int distance = abs(rx1 - rx2) + abs(ry1 - ry2);
                if (distance < min_distance) {
                    min_distance = distance;
                    best_src = i;
                    best_dst = j;
                }
            }
        }

        if (best_src != -1 && best_dst != -1) {
            connected[best_dst] = true;

            // Identify the room centers
            struct Room* src_room = &map->rooms[best_src];
            struct Room* dst_room = &map->rooms[best_dst];

            struct Point src_center = {
                (src_room->left_wall + src_room->right_wall) / 2,
                (src_room->top_wall  + src_room->bottom_wall) / 2
            };
            struct Point dst_center = {
                (dst_room->left_wall + dst_room->right_wall) / 2,
                (dst_room->top_wall  + dst_room->bottom_wall) / 2
            };

            // Create the corridor and place doors at the boundary
            create_corridor_and_place_doors(map, src_center, dst_center);
        }
    }
}

void create_corridor_and_place_doors(struct Map* map, struct Point start, struct Point end) {
    struct Point current = start;

    // Move horizontally
    while (current.x != end.x) {
        int step = (current.x < end.x) ? 1 : -1;
        int next_x = current.x + step;

        // 1) If FOG => carve corridor
        if (map->grid[current.y][next_x] == FOG) {
            map->grid[current.y][next_x] = CORRIDOR;
        }
        // 2) If wall => place door
        else if (map->grid[current.y][next_x] == WALL_HORIZONTAL ||
                 map->grid[current.y][next_x] == WALL_VERTICAL ||
                 map->grid[current.y][next_x] == WINDOW)
        {
            // Place a normal door by default
            map->grid[current.y][next_x] = DOOR;

            // Find the room boundary we just hit
            for (int r = 0; r < map->room_count; r++) {
                Room* rm = &map->rooms[r];
                if (   next_x >= rm->left_wall && next_x <= rm->right_wall
                    && current.y >= rm->top_wall && current.y <= rm->bottom_wall)
                {
                    // If room->door_count >= 1 => not guaranteed dead end
                    if (rm->door_count >= 1 && !rm->has_password_door) {
                        // 20% chance to become a password door
                        if (rand() % 100 < 20) {
                            map->grid[current.y][next_x] = DOOR_PASSWORD;
                            // Immediately place a password generator tile in that room
                            rm->has_password_door = true;
                            rm->password_unlocked = false;
                            rm->password_active   = false;
                            rm->door_code[0] = '\0';
                            place_password_generator_in_corner(map, rm);
                        }
                    }

                    // Record the door in rm->doors[]
                    if (rm->door_count < MAX_DOORS) {
                        rm->doors[rm->door_count].x = next_x;
                        rm->doors[rm->door_count].y = current.y;
                        rm->door_count++;
                    }
                    break;
                }
            }
        }
        current.x = next_x;
    }

    // Move vertically
    while (current.y != end.y) {
        int step = (current.y < end.y) ? 1 : -1;
        int next_y = current.y + step;

        if (map->grid[next_y][current.x] == FOG) {
            map->grid[next_y][current.x] = CORRIDOR;
        }
        else if (map->grid[next_y][current.x] == WALL_HORIZONTAL ||
                 map->grid[next_y][current.x] == WALL_VERTICAL ||
                 map->grid[next_y][current.x] == WINDOW)
        {
            map->grid[next_y][current.x] = DOOR;

            for (int r = 0; r < map->room_count; r++) {
                Room* rm = &map->rooms[r];
                if (   current.x >= rm->left_wall && current.x <= rm->right_wall
                    && next_y >= rm->top_wall && next_y <= rm->bottom_wall)
                {
                    if (rm->door_count >= 1) {
                        if (rand() % 100 < 20) {
                            map->grid[next_y][current.x] = DOOR_PASSWORD;
                            place_password_generator_in_corner(map, rm);
                        }
                    }

                    if (rm->door_count < MAX_DOORS) {
                        rm->doors[rm->door_count].x = current.x;
                        rm->doors[rm->door_count].y = next_y;
                        rm->door_count++;
                    }
                    break;
                }
            }
        }
        current.y = next_y;
    }
}

void place_password_generator_in_corner(struct Map* map, struct Room* room) {
    // Define the four "inner" corners (one tile away from walls)
    int corners[4][2] = {
        { room->left_wall + 1,  room->top_wall + 1 },      // Top-left
        { room->right_wall - 1, room->top_wall + 1 },      // Top-right
        { room->left_wall + 1,  room->bottom_wall - 1 },   // Bottom-left
        { room->right_wall - 1, room->bottom_wall - 1 }    // Bottom-right
    };

    // Randomly start from one corner and check the next if it's not suitable
    // This way, it's less predictable. If you want a single corner always, just pick one.
    int corner_index = rand() % 4;
    for (int i = 0; i < 4; i++) {
        int cx = corners[corner_index][0];
        int cy = corners[corner_index][1];

        // Only place the generator if it's currently a floor tile
        if (map->grid[cy][cx] == FLOOR) {
            map->grid[cy][cx] = PASSWORD_GEN;  // '&'
            return;
        }

        // Move to the next corner in the array
        corner_index = (corner_index + 1) % 4;
    }

    // If none of the corners were valid (all blocked), do nothing
    // (This is unlikely if the room is large enough).
}

<<<<<<< HEAD
<<<<<<< HEAD
=======
=======
>>>>>>> 0294fab (m vision okay)
// Update movement validation to handle doors
>>>>>>> 0294fab (m vision okay)
void move_character(struct Point* character_location, int key,
                    struct Map* game_map, int* hitpoints, Inventory* inventory){
    struct Point new_location = *character_location;
    switch (key) {
        case KEY_UP:    new_location.y--; break;
        case KEY_DOWN:  new_location.y++; break;
        case KEY_LEFT:  new_location.x--; break;
        case KEY_RIGHT: new_location.x++; break;
        default: return;
    }

    // Bounds check
    if (new_location.x < 0 || new_location.x >= MAP_WIDTH ||
        new_location.y < 0 || new_location.y >= MAP_HEIGHT) {
        return; // Out of map bounds, ignore
    }

    wchar_t target_tile = (wchar_t)game_map->grid[new_location.y][new_location.x];

    // -----------------------------------------------------------
    // 1) If it's a locked password door and we have NO password,
    //    disallow movement
    // -----------------------------------------------------------
    if (target_tile == PASSWORD_GEN) {
        // Identify the room
        Room* current_room = find_room_by_position(game_map, new_location.x, new_location.y);

        if (current_room && current_room->has_password_door) {
            // If code not yet generated, do it now!
            if (current_room->door_code[0] == '\0') {
                // Generate a 4-digit code
                int code_num = rand() % 10000; // 0..9999
                snprintf(current_room->door_code, sizeof(current_room->door_code), "%04d", code_num);
            }

            // Store the code in our global variable
            strncpy(current_code, current_room->door_code, sizeof(current_code) - 1);
            current_code[sizeof(current_code) - 1] = '\0';

            // Mark it visible and reset the timer
            code_visible = true;
            code_start_time = time(NULL);

            // Call our display function right away so it appears immediately
            update_password_display();
        }
    }


    else if (target_tile == DOOR_PASSWORD) {
        Room* door_room = find_room_by_position(game_map, new_location.x, new_location.y);
        if (door_room->password_unlocked) {
            // already unlocked => just move
            *character_location = new_location;
        } else {
            // if the user has at least 1 key, let them choose
            bool has_key = (inventory->ancient_key_count > 0);
            bool used_key = false;

            if (has_key) {
                // Ask user if they'd like to use the Ancient Key or enter password
                clear();
                mvprintw(2, 2, "Door is locked! You have an Ancient Key. Use it? (y/n)");
                refresh();
                int c = getch();
                if (c == 'y' || c == 'Y') {
                    used_key = true;
                }
                // else they might attempt normal password input below
            }

            if (used_key) {
                // 10% break chance
                if ((rand() % 100) < 10) {
                    // Key breaks => lose 1 key, gain 1 broken piece
                    inventory->ancient_key_count--;
                    inventory->broken_key_count++;
                    mvprintw(4, 2, "The Ancient Key broke!");
                } else {
                    // Key successfully used => remove 1 key
                    inventory->ancient_key_count--;
                    door_room->password_unlocked = true; 
                    mvprintw(4, 2, "Door unlocked with the Ancient Key!");
                    // Player can pass through now
                    *character_location = new_location;
                }
                refresh();
                getch();
                return; 
            } else {
                // Normal prompt for password (as before)...
                bool success = prompt_for_password_door(door_room);
                if (success) {
                    door_room->password_unlocked = true;
                    *character_location = new_location;
                }
                return;
            }
        }
    }

    else if (target_tile == ANCIENT_KEY) {
        // Pick up the Ancient Key
        game_map->grid[new_location.y][new_location.x] = FLOOR; // Remove the key from the map
        inventory->ancient_key_count++;
        mvprintw(12, 80, "You picked up an");
        mvprintw(13, 80, "Ancient Key!");
        refresh();
        getch();
    }

    
        // Handle secret doors
    else if (target_tile == SECRET_DOOR_CLOSED) {
        // Reveal the secret door
        game_map->grid[new_location.y][new_location.x] = SECRET_DOOR_REVEALED;
        mvprintw(MAP_HEIGHT + 7, 0, "You discovered a secret door!");
        refresh();
        getch();
        // Move the player through the revealed secret door
        *character_location = new_location;
        return;
    }
    else if (target_tile == SECRET_DOOR_REVEALED) {
        // Allow movement through the revealed secret door
        *character_location = new_location;
        return;
    }

    // Handle weapon pickups
    else {
        WeaponType picked_weapon = WEAPON_NONE;
        for (int i = WEAPON_MACE; i < WEAPON_COUNT; i++) {
            Weapon weapon = get_weapon_by_type((WeaponType)i);
            if (weapon.symbol == target_tile) {
                picked_weapon = (WeaponType)i;
                break;
            }
        }
        
        if (picked_weapon != WEAPON_NONE) {
            add_weapon(inventory, picked_weapon);
            game_map->grid[new_location.y][new_location.x] = FLOOR; // Remove weapon from map
            mvprintw(MAP_HEIGHT + 6, 0, "You picked up a %s!", 
                     (picked_weapon == WEAPON_MACE) ? "Mace" :
                     (picked_weapon == WEAPON_DAGGER) ? "Dagger" :
                     (picked_weapon == WEAPON_MAGIC_WAND) ? "Magic Wand" :
                     (picked_weapon == WEAPON_NORMAL_ARROW) ? "Normal Arrow" :
                     (picked_weapon == WEAPON_SWORD) ? "Sword" : "Unknown");
            refresh();
            getch();
        }
    }


    // -----------------------------------------------------------
    // 2) If it's one of these, it's impassable
    // -----------------------------------------------------------
    if (target_tile == WALL_HORIZONTAL ||
        target_tile == WALL_VERTICAL   ||
        target_tile == WINDOW          ||
        target_tile == PILLAR          ||
        target_tile == FOG) 
    {
        // Not passable
        return;
    }

    // Otherwise, it's considered passable:
    //   (Floor, Corridor, Food, Gold, 
    //    OR password door with hasPassword==true)
    *character_location = new_location;

    // -----------------------------------------------------------
    // 3) Trap logic: If we just moved onto a trap location, trigger damage
    // -----------------------------------------------------------
    for (int i = 0; i < game_map->trap_count; i++) {
        Trap* trap = &game_map->traps[i];
        if (trap->location.x == new_location.x &&
            trap->location.y == new_location.y)
        {
            if (!trap->triggered) {
                trap->triggered = true;
                game_map->grid[new_location.y][new_location.x] = TRAP_SYMBOL;
                *hitpoints -= 10;  // Reduce HP
                mvprintw(MAP_HEIGHT + 6, 0, "You triggered a trap! HP -10.");
                refresh();
            }
            break;
        }
    }
}

bool prompt_for_password_door(Room* door_room) {
    // door_room->door_code contains the correct 4-digit code
    // The player has up to 3 attempts
    // Return true if door is unlocked, false otherwise

    for (int attempt = 1; attempt <= 3; attempt++) {
        // Clear the screen so we're on a "special page"
        clear();

        // Prompt text
        mvprintw(2, 2, "You have encountered a locked door.");
        mvprintw(3, 2, "Attempts remaining: %d", 4 - attempt);

        // If this is not the first attempt, show a color-coded warning
        if (attempt > 1) {
            int color_pair_id = 0;
            if      (attempt == 2) color_pair_id = 5; // Yellow for 1st wrong attempt
            else if (attempt == 3) color_pair_id = 6; // Orange or your chosen color
            // If you want the 3rd attempt to be RED, switch the logic:
            // else if (attempt == 3) color_pair_id = 7;

            attron(COLOR_PAIR(color_pair_id));
            mvprintw(5, 2, 
                (attempt == 2) ? "Warning: 1 wrong attempt so far!" :
                                 "Warning: 2 wrong attempts so far!");
            attroff(COLOR_PAIR(color_pair_id));
        }

        // Ask user for the code
        mvprintw(7, 2, "Enter 4-digit password (attempt %d of 3): ", attempt);
        refresh();

        // Read the input
        echo();
        char entered[5];
        getnstr(entered, 4);
        noecho();

        // Check if user typed nothing
        if (entered[0] == '\0') {
            // We can let it count as a fail or skip attempts logic. 
            // Let’s treat it as a wrong attempt:
            continue;
        }

        // Compare with door_room->door_code
        if (strcmp(entered, door_room->door_code) == 0) {
            // Correct => unlock
            clear();
            attron(COLOR_PAIR(2)); // or some "success" color if you want
            mvprintw(2, 2, "Door unlocked successfully!");
            attroff(COLOR_PAIR(2));
            refresh();
            getch();
            return true;  // success
        }
        else {
            // Wrong => let the loop continue
            // (If attempt < 3, the player will get another chance.)
        }
    }

    // If we get here, the user has failed 3 attempts
    clear();
    attron(COLOR_PAIR(7)); // Red for final fail
    mvprintw(2, 2, "Too many wrong attempts! The door remains locked.");
    attroff(COLOR_PAIR(7));
    refresh();
    getch();
    return false; // remain locked
}

void add_ancient_key(struct Map* game_map) {
    // Guarantee exactly 1 Ancient Key per level
    // Find a random floor tile and place '▲'
    while (true) {
        int x = rand() % MAP_WIDTH;
        int y = rand() % MAP_HEIGHT;
        if (game_map->grid[y][x] == FLOOR) {
            game_map->grid[y][x] = ANCIENT_KEY; 
            break; 
        }
    }
}

void print_password_messages(const char* message, int line_offset) {
    // We want to place the message near the right edge, say 25 columns from it.
    // 'line_offset' determines which row (vertical position) we print on.
    int right_margin = 25; // or 30, etc.
    int x = COLS - right_margin; 

    // If x is less than 0 for very small terminals, clamp to 0
    if (x < 0) x = 0;

    // Print the message at row = line_offset, column = x
    mvprintw(line_offset, x, "%s", message);
    refresh();
}

void update_password_display() {
    if (!code_visible) return;

    double elapsed = difftime(time(NULL), code_start_time);
    if (elapsed > 30.0) {
        code_visible = false;
        print_password_messages("                              ", MAP_HEIGHT + 5);
        print_password_messages("                              ", MAP_HEIGHT + 6);
    } else {
        char msg[128], msg2[128];
        snprintf(msg, sizeof(msg),
                 "Room code: %s",
                 current_code);
        snprintf(msg2, sizeof(msg2),
                 "(%.0f seconds left)",
                 30.0 - elapsed);

        print_password_messages(msg, MAP_HEIGHT + 5);
        print_password_messages(msg2, MAP_HEIGHT + 6);
    }
}

void init_map(struct Map* map) {
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            map->grid[y][x] = FOG;
            map->visibility[y][x] = false;
            map->discovered[y][x] = false;
        }
    }
    map->room_count = 0;
    map->trap_count = 0; // Initialize trap count
}

bool rooms_overlap(const struct Room* r1, const struct Room* r2) {
    return !(r1->right_wall + MIN_ROOM_DISTANCE < r2->left_wall ||
             r2->right_wall + MIN_ROOM_DISTANCE < r1->left_wall ||
             r1->top_wall + MIN_ROOM_DISTANCE < r2->bottom_wall ||
             r2->top_wall + MIN_ROOM_DISTANCE < r1->bottom_wall);
}

void add_gold(struct Map* game_map) {
    const int GOLD_COUNT = 10;  // Adjust as needed
    int gold_placed = 0;
    
    while (gold_placed < GOLD_COUNT) {
        int x = rand() % MAP_WIDTH;
        int y = rand() % MAP_HEIGHT;
        
        // Place gold only on floor tiles
        if (game_map->grid[y][x] == FLOOR) {
            game_map->grid[y][x] = GOLD;  // Assume GOLD is defined, e.g., `#define GOLD '$'`
            gold_placed++;
        }
    }
}

void message(const char* text) {
    mvprintw(MAP_HEIGHT, 0, "%s", text);
    refresh();
}

bool isPointInRoom(struct Point* point, struct Room* room) {
    return (point->x >= room->left_wall && point->x <= room->right_wall &&
            point->y >= room->top_wall && point->y <= room->bottom_wall);
}

void showRoom(struct Map* game_map, struct Room* room, char visible[MAP_HEIGHT][MAP_WIDTH]) {
    for (int y = room->bottom_wall; y <= room->top_wall; y++) {
        for (int x = room->left_wall; x <= room->right_wall; x++) {
            if (y >= 0 && y < MAP_HEIGHT && x >= 0 && x < MAP_WIDTH) {
                visible[y][x] = 1;
            }
        }
    }
}

bool canSeeRoomThroughWindow(struct Map* game_map, struct Room* room1, struct Room* room2) {
    bool share_horizontal = (room1->top_wall == room2->bottom_wall - 1) ||
                          (room1->bottom_wall == room2->top_wall + 1);

    bool share_vertical = (room1->right_wall == room2->left_wall - 1) ||
                         (room1->left_wall == room2->right_wall + 1);
    
    // Check for windows on shared walls
    if (share_horizontal) {
        for (int x = MAX(room1->left_wall, room2->left_wall); 
             x <= MIN(room1->right_wall, room2->right_wall); x++) {
            if (game_map->grid[room1->top_wall][x] == WINDOW ||
                game_map->grid[room1->bottom_wall][x] == WINDOW) {
                return true;
            }
        }
    }
    
    if (share_vertical) {
        for (int y = MAX(room1->bottom_wall, room2->bottom_wall);
             y <= MIN(room1->top_wall, room2->top_wall); y++) {
            if (game_map->grid[y][room1->right_wall] == WINDOW ||
                game_map->grid[y][room1->left_wall] == WINDOW) {
                return true;
            }
        }
    }
    
    return false;
}

int manhattanDistance(int x1, int y1, int x2, int y2) {
    return abs(x1 - x2) + abs(y1 - y2);
}

void add_game_message(struct MessageQueue* queue, const char* text, int color_pair) {
    if (queue->count >= MAX_MESSAGES) {
        // Shift messages up
        for (int i = 0; i < MAX_MESSAGES - 1; i++) {
            queue->messages[i] = queue->messages[i + 1];
        }
        queue->count--;
    }
    
    struct GameMessage* msg = &queue->messages[queue->count];
    strncpy(msg->text, text, MESSAGE_LENGTH - 1);
    msg->text[MESSAGE_LENGTH - 1] = '\0';
    msg->time_to_live = 60;  // Show for ~2 seconds at 30 fps
    msg->color_pair = color_pair;
    queue->count++;
}

void update_messages(struct MessageQueue* queue) {
    for (int i = 0; i < queue->count; i++) {
        queue->messages[i].time_to_live--;
        if (queue->messages[i].time_to_live <= 0) {
            // Remove this message
            for (int j = i; j < queue->count - 1; j++) {
                queue->messages[j] = queue->messages[j + 1];
            }
            queue->count--;
            i--;
        }
    }
}

void draw_messages(struct MessageQueue* queue, int start_y, int start_x) {
    for (int i = 0; i < queue->count; i++) {
        attron(COLOR_PAIR(queue->messages[i].color_pair));
        mvprintw(start_y + i, start_x, "%s", queue->messages[i].text);
        attroff(COLOR_PAIR(queue->messages[i].color_pair));
    }
}


bool is_valid_room_placement(struct Map* map, struct Room* room) {
    // Check map boundaries
    if (room->left_wall < 1 || room->right_wall >= MAP_WIDTH-1 ||
        room->top_wall < 1 || room->bottom_wall >= MAP_HEIGHT-1) {
        return false;
    }
    
    // Check overlap with existing rooms
    for (int i = 0; i < map->room_count; i++) {
        struct Room* other = &map->rooms[i];
        if (room->left_wall - MIN_ROOM_DISTANCE <= other->right_wall &&
            room->right_wall + MIN_ROOM_DISTANCE >= other->left_wall &&
            room->top_wall - MIN_ROOM_DISTANCE <= other->bottom_wall &&
            room->bottom_wall + MIN_ROOM_DISTANCE >= other->top_wall) {
            return false;
        }
    }
    
    return true;
}

void place_room(struct Map* map, struct Room* room) {
    // Place floor
    for (int y = room->top_wall + 1; y < room->bottom_wall; y++) {
        for (int x = room->left_wall + 1; x < room->right_wall; x++) {
            map->grid[y][x] = FLOOR;
        }
    }
    
    // Place walls
    for (int x = room->left_wall; x <= room->right_wall; x++) {
        map->grid[room->top_wall][x] = WALL_HORIZONTAL;
        map->grid[room->bottom_wall][x] = WALL_HORIZONTAL;
    }
    for (int y = room->top_wall; y <= room->bottom_wall; y++) {
        map->grid[y][room->left_wall] = WALL_VERTICAL;
        map->grid[y][room->right_wall] = WALL_VERTICAL;
    }
    
    // Place pillars and windows
    place_pillars(map, room);
    place_windows(map, room);
}

void update_visibility(struct Map* game_map, struct Point* player_pos, bool visible[MAP_HEIGHT][MAP_WIDTH]) {
    const int CORRIDOR_SIGHT = 5; // Sight range in corridors

    // Reset visibility
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            visible[y][x] = false;
        }
    }

    // Check if the player is in a room
    struct Room* current_room = NULL;
    for (int i = 0; i < game_map->room_count; i++) {
        if (isPointInRoom(player_pos, &game_map->rooms[i])) {
            current_room = &game_map->rooms[i];
            break;
        }
    }

    if (current_room) {
        // Mark the room as visited
        current_room->visited = true;

        // Reveal the entire room
        for (int y = current_room->top_wall; y <= current_room->bottom_wall; y++) {
            for (int x = current_room->left_wall; x <= current_room->right_wall; x++) {
                visible[y][x] = true;
                game_map->discovered[y][x] = true; // Mark as discovered
            }
        }
    } else {
        // Player is in a corridor: reveal up to 5 tiles ahead
        for (int dy = -CORRIDOR_SIGHT; dy <= CORRIDOR_SIGHT; dy++) {
            for (int dx = -CORRIDOR_SIGHT; dx <= CORRIDOR_SIGHT; dx++) {
                int tx = player_pos->x + dx;
                int ty = player_pos->y + dy;

                if (tx >= 0 && tx < MAP_WIDTH && ty >= 0 && ty < MAP_HEIGHT) {
                    int distance = abs(dx) + abs(dy);

                    if (distance <= CORRIDOR_SIGHT && game_map->grid[ty][tx] == CORRIDOR) {
                        visible[ty][tx] = true;
                        game_map->discovered[ty][tx] = true; // Mark as discovered
                    }
                }
            }
        }
    }
}

void place_stairs(struct Map* map) {
    // Place stairs in a random room that doesn't already have stairs
    while (1) {
        int room_index = rand() % map->room_count;
        struct Room* room = &map->rooms[room_index];
        
        if (!room->has_stairs) {
            // Place stairs randomly within the room
            int x = room->left_wall + 1 + rand() % (room->width - 2);
            int y = room->top_wall + 1 + rand() % (room->height - 2);
            
            map->grid[y][x] = STAIRS;
            room->has_stairs = true;
            map->stairs_location.x = x;
            map->stairs_location.y = y;
            break;
        }
    }
}

void place_pillars(struct Map* map, struct Room* room) {
    // Place pillars in corners if room is large enough
    if (room->width >= 6 && room->height >= 6) {
        // Top-left pillar
        map->grid[room->top_wall + 2][room->left_wall + 2] = PILLAR;
        
        // Top-right pillar
        map->grid[room->top_wall + 2][room->right_wall - 2] = PILLAR;
        
        // Bottom-left pillar
        map->grid[room->bottom_wall - 2][room->left_wall + 2] = PILLAR;
        
        // Bottom-right pillar
        map->grid[room->bottom_wall - 2][room->right_wall - 2] = PILLAR;
    }
}

bool can_see_room(struct Map* map, struct Room* room1, struct Room* room2) {
    // Check if rooms are connected through a window
    if (canSeeRoomThroughWindow(map, room1, room2)) {
        return true;
    }

    // Check if rooms are close enough and have line of sight
    int room1_center_x = (room1->left_wall + room1->right_wall) / 2;
    int room1_center_y = (room1->top_wall + room1->bottom_wall) / 2;
    int room2_center_x = (room2->left_wall + room2->right_wall) / 2;
    int room2_center_y = (room2->top_wall + room2->bottom_wall) / 2;

    // Check if rooms are within sight range
    if (abs(room1_center_x - room2_center_x) + abs(room1_center_y - room2_center_y) > SIGHT_RANGE * 2) {
        return false;
    }

    // Simple line of sight check
    int dx = room2_center_x - room1_center_x;
    int dy = room2_center_y - room1_center_y;
    int steps = MAX(abs(dx), abs(dy));

    if (steps == 0) return true;

    float x_inc = (float)dx / steps;
    float y_inc = (float)dy / steps;

    float x = room1_center_x;
    float y = room1_center_y;

    for (int i = 0; i < steps; i++) {
        x += x_inc;
        y += y_inc;

        int check_x = (int)x;
        int check_y = (int)y;

        // Check if line of sight is blocked by any wall or obstruction
        if (map->grid[check_y][check_x] == WALL_VERTICAL || 
            map->grid[check_y][check_x] == WALL_HORIZONTAL) {
            return false;
        }
    }

    return true;
}

bool validate_stair_placement(struct Map* map) {
    int stair_count = 0;
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            if (map->grid[y][x] == STAIRS) stair_count++;
        }
    }
    return stair_count == 1;
}

/**
 * Creates a safe filename for save games.
 * Maximum combined length of username and savename must be less than
 * MAX_FILENAME_LEN - 12 characters (accounting for "saves/", "_", and ".sav")
 * 
 * @return false if the resulting filename would be too long
 */

bool create_safe_filename(char* dest, size_t dest_size, const char* username, const char* savename) {
    // Calculate required space
    size_t required = strlen("saves/") + strlen(username) + 1 + strlen(savename) + 
                     strlen(".sav") + 1;
    
    if (required > dest_size) {
        return false;
    }
    
    snprintf(dest, dest_size, "saves/%s_%s.sav", username, savename);
    return true;
}

// Saving the game
void save_current_game(struct UserManager* manager, struct Map* game_map, 
                      struct Point* character_location, int score, int current_level, Inventory* inventory) {
    if (!manager->current_user) {
        mvprintw(0, 0, "Cannot save game as guest user.");
        refresh();
        getch();
        return;
    }

    // Ensure 'saves/' directory exists
    struct stat st = {0};
    if (stat("saves", &st) == -1) {
        mkdir("saves", 0700); // Permissions can be adjusted as needed
    }

    char filename[256];
    char save_name[100];

    clear();
    echo();
    mvprintw(0, 0, "Enter name for this save: ");
    refresh();
    getnstr(save_name, sizeof(save_name) - 1); // Limit input to fit within the buffer
    noecho();

    // Truncate username and save_name if they exceed a safe length
    char safe_username[94];
    char safe_save_name[94];
    strncpy(safe_username, manager->current_user->username, 93);
    safe_username[93] = '\0';
    strncpy(safe_save_name, save_name, 93);
    safe_save_name[93] = '\0';

    // Create filename using truncated strings
    int written = snprintf(filename, sizeof(filename), "saves/%s_%s.sav", safe_username, safe_save_name);
    if (written < 0 || written >= sizeof(filename)) {
        mvprintw(2, 0, "Error: Filename too long.");
        refresh();
        getch();
        return;
    }

    FILE* file = fopen(filename, "wb");
    if (!file) {
        mvprintw(2, 0, "Error: Could not create save file.");
        refresh();
        getch();
        return;
    }

    struct SavedGame save = {
        .game_map = *game_map,
        .character_location = *character_location,
        .score = score,
        .current_level = current_level,
        .inventory = *inventory, // Save inventory
        .save_time = time(NULL)
    };
    strncpy(save.name, save_name, sizeof(save.name) - 1);
    save.name[sizeof(save.name) - 1] = '\0'; // Ensure null-termination

    fwrite(&save, sizeof(struct SavedGame), 1, file);
    fclose(file);

    mvprintw(2, 0, "Game saved successfully!");
    refresh();
    getch();
}

// Loading the game
bool load_saved_game(struct UserManager* manager, struct SavedGame* saved_game) {
    if (!manager->current_user) {
        mvprintw(0, 0, "Cannot load game as guest user.");
        refresh();
        getch();
        return false;
    }

    clear();
    mvprintw(0, 0, "Enter save name to load: ");
    refresh();

    char save_name[100];
    echo();
    getnstr(save_name, sizeof(save_name) - 1); // Limit input to fit within buffer
    noecho();

    char safe_username[94];
    char safe_save_name[94];
    strncpy(safe_username, manager->current_user->username, 93);
    safe_username[93] = '\0';
    strncpy(safe_save_name, save_name, 93);
    safe_save_name[93] = '\0';

    char filename[256];
    int written = snprintf(filename, sizeof(filename), "saves/%s_%s.sav", safe_username, safe_save_name);
    if (written < 0 || written >= sizeof(filename)) {
        mvprintw(2, 0, "Error: Filename too long.");
        refresh();
        getch();
        return false;
    }

    FILE* file = fopen(filename, "rb");
    if (!file) {
        mvprintw(2, 0, "Error: Could not find save file.");
        refresh();
        getch();
        return false;
    }

    fread(saved_game, sizeof(struct SavedGame), 1, file);
    fclose(file);
    return true;
}


void list_saved_games(struct UserManager* manager) {
    if (!manager->current_user) return;

    char cmd[MAX_STRING_LEN * 3];
    snprintf(cmd, sizeof(cmd), "ls saves/%s_*.sav 2>/dev/null", 
             manager->current_user->username);

    FILE* pipe = popen(cmd, "r");
    if (!pipe) return;

    mvprintw(0, 0, "Available saved games:");
    int line = 2;
    char buffer[256];
    
    while (fgets(buffer, sizeof(buffer), pipe)) {
        // Extract save name from filename
        char* start = strrchr(buffer, '_') + 1;
        char* end = strrchr(buffer, '.');
        if (start && end) {
            *end = '\0';
            mvprintw(line++, 0, "%s", start);
        }
    }
    
    pclose(pipe);
    refresh();
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

