#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <dirent.h>
#include <ctype.h>
#include "game.h"
#include "users.h"

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


//For Ancient Keys
int ancient_key_count = 0;
int broken_key_count = 0;

// Initialize a player structure (Modify existing player initialization if necessary)
void initialize_player(Player* player, struct Point start_location) {
    player->location = start_location;
    player->hitpoints = 100;
    player->hunger_rate = 0;
    player->score = 0;
    player->weapon_count = 1;
    player->equipped_weapon = 0; // Equip first weapon
    player->ancient_key_count = 0;
    player->broken_key_count = 0;
    // Initialize other player attributes as needed

    player->weapons[0] = create_weapon(WEAPON_MACE);
}

void play_game(struct UserManager* manager, struct Map* game_map, 
               struct Point* character_location, int initial_score) {
    const int max_level = 4;  // Define maximum levels
    int current_level = 1;    // Start at level 1
    bool game_running = true;
    int score = initial_score;

    bool show_map = false;

    // Initialize player attributes
    Player player;
    initialize_player(&player, game_map->initial_position);

    // Initialize other variables
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

    // Message Queue for game messages
    struct MessageQueue message_queue = { .count = 0 };

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

        // Render enemies
        render_enemies(game_map);

        // Show game info
        mvprintw(MAP_HEIGHT + 1, 0, "Level: %d", current_level);
        if (player.equipped_weapon != -1) {
            mvprintw(MAP_HEIGHT + 2, 0, "Equipped Weapon: %s (Damage: %d)", 
                     player.weapons[player.equipped_weapon].name, 
                     player.weapons[player.equipped_weapon].damage);
        } else {
            mvprintw(MAP_HEIGHT + 2, 0, "Equipped Weapon: None");
        }
        mvprintw(MAP_HEIGHT + 3, 0, "Score: %d", score);
        mvprintw(MAP_HEIGHT + 4, 0, "Hitpoints: %d", hitpoints);
        if (manager->current_user) {
            mvprintw(MAP_HEIGHT + 5, 0, "Player: %s", manager->current_user->username);
        } else {
            mvprintw(MAP_HEIGHT + 5, 0, "Player: Guest");
        }
        mvprintw(MAP_HEIGHT + 6, 0, "Controls: Arrow Keys to Move, 'i' - Weapon Inventory, 'e' - General Inventory, 'q' - Quit");
        refresh();

        // Display messages
        draw_messages(&message_queue, MAP_HEIGHT + 8, 0);
        update_messages(&message_queue);

        // Increase hunger rate over time
        if (frame_count % HUNGER_INCREASE_INTERVAL == 0) {
            hunger_rate++;
        }

        // Decrease hitpoints if hunger exceeds threshold
        if (hunger_rate >= MAX_HUNGER) {
            if (hunger_damage_timer % HUNGER_DAMAGE_INTERVAL == 0) {
                hitpoints -= HITPOINT_DECREASE_RATE;
                add_game_message(&message_queue, "Hunger is too high! HP decreased.", 4); // COLOR_PAIR_TRAPS
            }
            hunger_damage_timer++;
        } else {
            hunger_damage_timer = 0; // Reset damage timer if hunger is below threshold
        }

        // Check if hitpoints are zero
        if (hitpoints <= 0) {
            add_game_message(&message_queue, "Game Over! You ran out of hitpoints.", 7); // COLOR_PAIR_FINAL_FAIL
            game_running = false;
        }

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
                move_character(&player, key, game_map, &hitpoints);
                *character_location = player.location; // Synchronize positions

                // Check the tile the player moves onto
                char tile = game_map->grid[character_location->y][character_location->x];

                if (tile == STAIRS) {
                    // Handle level up logic
                    if (current_level < max_level) {
                        current_level++;
                        struct Room* current_room = NULL;

                        // Find the current room
                        for (int i = 0; i < game_map->room_count; i++) {
                            if (isPointInRoom(character_location, &game_map->rooms[i])) {
                                current_room = &game_map->rooms[i];
                                break;
                            }
                        }

                        // Generate the next map with current_level and max_level
                        struct Map new_map = generate_map(current_room, current_level, max_level);
                        *game_map = new_map;
                        *character_location = new_map.initial_position;

                        add_game_message(&message_queue, "Level up! Welcome to Level.", 3); // COLOR_PAIR_WEAPONS
                        // Optionally, append the level number to the message
                        char level_msg[50];
                        snprintf(level_msg, sizeof(level_msg), "Level up! Welcome to Level %d.", current_level);
                        add_game_message(&message_queue, level_msg, 3);
                        // Reset player stats or adjust as needed
                    } else {
                        // Final level completion (Treasure Room reached)
                        add_game_message(&message_queue, "Congratulations! You've completed all levels.", 2); // COLOR_PAIR_UNLOCKED_DOOR
                        game_running = false;
                    }
                } else if (tile == FOOD) {
                    // Collect food
                    if (food_count < 5) {
                        game_map->grid[character_location->y][character_location->x] = FLOOR;
                        food_inventory[food_count++] = 1;
                        add_game_message(&message_queue, "You collected some food.", 3); // COLOR_PAIR_WEAPONS
                    }
                } else if (tile == GOLD) {
                    // Collect gold
                    game_map->grid[character_location->y][character_location->x] = FLOOR;
                    gold_count++;
                    score += 10;
                    add_game_message(&message_queue, "You collected some gold.", 3); // COLOR_PAIR_WEAPONS
                } else if (tile == FLOOR) {
                    // Check for traps
                    for (int i = 0; i < game_map->trap_count; i++) {
                        Trap* trap = &game_map->traps[i];
                        if (trap->location.x == character_location->x && 
                            trap->location.y == character_location->y) {
                            if (!trap->triggered) {
                                trap->triggered = true;
                                game_map->grid[character_location->y][character_location->x] = TRAP_SYMBOL;
                                hitpoints -= 10;
                                add_game_message(&message_queue, "You triggered a trap! Hitpoints decreased.", 4); // COLOR_PAIR_TRAPS
                            }
                            break;
                        }
                    }
                }
                break;

            case 'e':
            case 'E':
                // Open inventory menu
                open_inventory_menu(food_inventory, &food_count, &gold_count, &score, &hunger_rate,
                        &player.ancient_key_count, &player.broken_key_count);
                break;

            case 'a':
            case 'A':
                // Attack using the equipped weapon
                use_weapon(&player);
                break;

            case 'i':
            case 'I':
                // Open the weapon inventory menu
                open_weapon_inventory_menu(&player, game_map);
                break;

            case 's':
            case 'S':
                // Open spell inventory menu
                open_spell_inventory_menu(game_map, &player);
                break;

            case 'q':
                game_running = false;
                break;
        }

        // After player movement, check for enemy activation
        for (int i = 0; i < game_map->enemy_count; i++) {
            Enemy* enemy = &game_map->enemies[i];
            
            if (!enemy->active && is_enemy_in_same_room(&player, enemy, game_map)) {
                enemy->active = true;
                add_game_message(&message_queue, "An enemy has spotted you!", 16); // COLOR_PAIR_ENEMIES
            }
        }

        // Update enemies' actions
        update_enemies(game_map, &player, &hitpoints);

        // Update password display
        update_password_display();

        // Increment frame count
        frame_count++;
    }
}


void print_full_map(struct Map* game_map, struct Point* character_location) {
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            char tile = game_map->grid[y][x];

            // Draw the player
            if (character_location->x == x && character_location->y == y) {
                attron(COLOR_PAIR(2)); // Example: Player color
                mvaddch(y, x, PLAYER_CHAR);
                attroff(COLOR_PAIR(2));
                continue;
            }

            // Determine room theme
            RoomTheme room_theme = THEME_NORMAL; // Default
            for (int i = 0; i < game_map->room_count; i++) {
                if (isPointInRoom(&(struct Point){x, y}, &game_map->rooms[i])) {
                    room_theme = game_map->rooms[i].theme;
                    break;
                }
            }

            // Apply color based on theme
            switch(room_theme) {
                case THEME_NORMAL:
                    attron(COLOR_PAIR(COLOR_PAIR_ROOM_NORMAL));
                    break;
                case THEME_ENCHANT:
                    attron(COLOR_PAIR(COLOR_PAIR_ROOM_ENCHANT));
                    break;
                case THEME_TREASURE:
                    attron(COLOR_PAIR(COLOR_PAIR_ROOM_TREASURE));
                    break;
                default:
                    // Default color
                    break;
            }

            // Apply color and print based on tile type
            switch(tile) {
                case TREASURE_CHEST:
                    attron(COLOR_PAIR(COLOR_PAIR_ROOM_TREASURE)); // Use Treasure Room color
                    mvaddch(y, x, TREASURE_CHEST);
                    attroff(COLOR_PAIR(COLOR_PAIR_ROOM_TREASURE));
                    break;
                    

                case DOOR_PASSWORD:
                    {
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
                    break;

                case ENEMY_FIRE_MONSTER:
                    attron(COLOR_PAIR(16)); // Define a new color pair for enemies
                    mvaddch(y, x, ENEMY_FIRE_MONSTER);
                    attroff(COLOR_PAIR(16));
                    break;

                case TRAP_SYMBOL:
                    // Red for triggered traps
                    attron(COLOR_PAIR(4));
                    mvaddch(y, x, tile);
                    attroff(COLOR_PAIR(4));
                    break;

                case ANCIENT_KEY:
                    // Golden yellow for Ancient Key
                    attron(COLOR_PAIR(8));
                    mvaddstr(y, x, "▲");  // Unicode symbol
                    attroff(COLOR_PAIR(8));
                    break;

                case SECRET_DOOR_CLOSED:
                    // Print as wall
                    mvaddch(y, x, WALL_HORIZONTAL);
                    break;

                case SECRET_DOOR_REVEALED:
                    // Print as '?', with distinct color
                    attron(COLOR_PAIR(9));
                    mvaddch(y, x, '?');
                    attroff(COLOR_PAIR(9));
                    break;

                case WEAPON_MACE:
                case WEAPON_DAGGER:
                case WEAPON_MAGIC_WAND:
                case WEAPON_ARROW:
                case WEAPON_SWORD:
                    // Color weapons in yellow
                    attron(COLOR_PAIR(3));
                    mvaddch(y, x, tile);
                    attroff(COLOR_PAIR(3));
                    break;

                // Handle spells with colors
                case SPELL_HEALTH:
                    attron(COLOR_PAIR(COLOR_PAIR_HEALTH));
                    mvaddch(y, x, SPELL_HEALTH);
                    attroff(COLOR_PAIR(COLOR_PAIR_HEALTH));
                    break;
                case SPELL_SPEED:
                    attron(COLOR_PAIR(COLOR_PAIR_SPEED));
                    mvaddch(y, x, SPELL_SPEED);
                    attroff(COLOR_PAIR(COLOR_PAIR_SPEED));
                    break;
                case SPELL_DAMAGE:
                    attron(COLOR_PAIR(COLOR_PAIR_DAMAGE));
                    mvaddch(y, x, SPELL_DAMAGE);
                    attroff(COLOR_PAIR(COLOR_PAIR_DAMAGE));
                    break;

                // [Handle other tile types...]

                default:
                    // Normal tile printing with room theme color
                    mvaddch(y, x, tile);
                    break;
            }

            // Turn off room theme color
            switch(room_theme) {
                case THEME_NORMAL:
                case THEME_ENCHANT:
                case THEME_TREASURE:
                    attroff(COLOR_PAIR(COLOR_PAIR_ROOM_NORMAL));
                    attroff(COLOR_PAIR(COLOR_PAIR_ROOM_ENCHANT));
                    attroff(COLOR_PAIR(COLOR_PAIR_ROOM_TREASURE));
                    break;
                default:
                    // Default color
                    break;
            }
        }
    }
}


struct Map generate_map(struct Room* previous_room, int current_level, int max_level) {
    struct Map map;
    init_map(&map);

    // Determine the number of rooms based on the level or other criteria
    int num_rooms = MIN_ROOM_COUNT + rand() % (MAX_ROOMS - MIN_ROOM_COUNT + 1);
    
    // If there's a previous room, retain it (e.g., the room with stairs)
    if (previous_room != NULL) {
        // Copy previous room details
        struct Room new_room = *previous_room;
        map.rooms[map.room_count++] = new_room;
        place_room(&map, &new_room);
    }

    // Generate additional rooms
    for (int i = (previous_room ? 1 : 0); i < num_rooms; i++) {  // Skip first room if previous_room exists
        struct Room room;
        int attempts = 0;
        bool placed = false;

        while (!placed && attempts < 100) {
            room.width = MIN_WIDTH_OR_LENGTH + rand() % (MAX_ROOM_SIZE - MIN_WIDTH_OR_LENGTH + 1);
            room.height = MIN_WIDTH_OR_LENGTH + rand() % (MAX_ROOM_SIZE - MIN_WIDTH_OR_LENGTH + 1);
            room.left_wall = 1 + rand() % (MAP_WIDTH - room.width - 2);
            room.top_wall = 1 + rand() % (MAP_HEIGHT - room.height - 2);
            room.right_wall = room.left_wall + room.width;
            room.bottom_wall = room.top_wall + room.height;
            room.door_count = 0;
            room.has_stairs = false;
            room.visited = false;
            room.theme = THEME_NORMAL; // Default theme

            if (is_valid_room_placement(&map, &room)) {
                // Assign theme based on level
                if (current_level == max_level && i == num_rooms - 1) {
                    // Last room of the last level is the Treasure Room
                    room.theme = THEME_TREASURE;
                }
                else {
                    // Assign Enchant or Normal themes based on probability or other criteria
                    int rand_num = rand() % 100;
                    if (rand_num < 10) { // 10% chance for Enchant Room
                        room.theme = THEME_ENCHANT;
                    }
                    else {
                        room.theme = THEME_NORMAL;
                    }
                }

                map.rooms[map.room_count++] = room;
                place_room(&map, &room);
                placed = true;
            }
            attempts++;
        }
    }

    // Connect rooms with corridors
    connect_rooms_with_corridors(&map);

    // Place secret doors (if applicable)
    place_secret_doors(&map);

    // Spawn items and features
    add_food(&map);
    add_gold(&map);
    add_traps(&map);
    add_weapons(&map);
    add_spells(&map);

    // Place the single Ancient Key
    add_ancient_key(&map);

    // Place stairs (if applicable)
    place_stairs(&map);

    add_enemies(&map, current_level);

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

void open_inventory_menu(int* food_inventory, int* food_count, int* gold_count,
                         int* score, int* hunger_rate,
                         int* ancient_key_count, int* broken_key_count){
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
        //    The working keys and broken key pieces
        mvprintw(13, 0, "Ancient Keys (working): %d", *ancient_key_count);
        mvprintw(14, 0, "Broken Key Pieces: %d", *broken_key_count);

        // 4) Instructions
        mvprintw(16, 0, "Press 'u' + slot number (1-5) to use food, 'c' to combine 2 broken keys, 'q' to quit.");
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
                mvprintw(18, 0, "Enter slot number (1-5): ");
                refresh();
                int slot = getch() - '0'; // Convert char to integer

                if (slot >= 1 && slot <= 5 && food_inventory[slot - 1] == 1) {
                    // Use food
                    food_inventory[slot - 1] = 0;
                    (*food_count)--;
                    *hunger_rate = MAX(*hunger_rate - 20, 0);

                    mvprintw(20, 0, "You used food from slot %d! Hunger decreased.", slot);
                } else {
                    mvprintw(20, 0, "Invalid slot or no food in slot!");
                }
                refresh();
                getch(); // Wait for player to acknowledge
            } break;

            case 'c': {
                // Combine two broken key pieces into one working Ancient Key
                if (*broken_key_count >= 2) {
                    *broken_key_count -= 2;
                    (*ancient_key_count)++;
                    mvprintw(18, 0, "You combined two broken key pieces into one working Ancient Key!");
                } else {
                    mvprintw(18, 0, "Not enough broken key pieces to combine!");
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
                attron(COLOR_PAIR(2)); // Example: Player color
                mvaddch(y, x, PLAYER_CHAR);
                attroff(COLOR_PAIR(2));
                continue;
            }

            // Determine visibility
            bool should_draw = false;
            if (visible[y][x]) {
                should_draw = true;
            } else {
                // Check if it's in a visited room or discovered
                bool is_in_visited_room = false;
                RoomTheme room_theme = THEME_UNKNOWN;
                for (int i = 0; i < game_map->room_count; i++) {
                    if (game_map->rooms[i].visited &&
                        isPointInRoom(&(struct Point){x, y}, &game_map->rooms[i])) {
                        is_in_visited_room = true;
                        room_theme = game_map->rooms[i].theme;
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

            // Determine room theme
            RoomTheme room_theme = THEME_NORMAL; // Default
            for (int i = 0; i < game_map->room_count; i++) {
                if (isPointInRoom(&(struct Point){x, y}, &game_map->rooms[i])) {
                    room_theme = game_map->rooms[i].theme;
                    break;
                }
            }

            // Apply color based on theme
            switch(room_theme) {
                case THEME_NORMAL:
                    attron(COLOR_PAIR(COLOR_PAIR_ROOM_NORMAL));
                    break;
                case THEME_ENCHANT:
                    attron(COLOR_PAIR(COLOR_PAIR_ROOM_ENCHANT));
                    break;
                case THEME_TREASURE:
                    attron(COLOR_PAIR(COLOR_PAIR_ROOM_TREASURE));
                    break;
                default:
                    // Default color
                    break;
            }

            // Apply color and print based on tile type
            switch(tile) {
                case TREASURE_CHEST:
                    attron(COLOR_PAIR(COLOR_PAIR_ROOM_TREASURE)); // Use Treasure Room color
                    mvaddch(y, x, TREASURE_CHEST);
                    attroff(COLOR_PAIR(COLOR_PAIR_ROOM_TREASURE));
                    break;


                case ENEMY_FIRE_MONSTER:
                    attron(COLOR_PAIR(16)); // Define a new color pair for enemies
                    mvaddch(y, x, ENEMY_FIRE_MONSTER);
                    attroff(COLOR_PAIR(16));
                    break;

                
                case DOOR_PASSWORD:
                    {
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
                    break;

                case TRAP_SYMBOL:
                    // Red for triggered traps
                    attron(COLOR_PAIR(4));
                    mvaddch(y, x, tile);
                    attroff(COLOR_PAIR(4));
                    break;

                case ANCIENT_KEY:
                    // Golden yellow for Ancient Key
                    attron(COLOR_PAIR(8));
                    mvaddstr(y, x, "▲");  // Unicode symbol
                    attroff(COLOR_PAIR(8));
                    break;

                case SECRET_DOOR_CLOSED:
                    // Print as wall
                    mvaddch(y, x, WALL_HORIZONTAL);
                    break;

                case SECRET_DOOR_REVEALED:
                    // Print as '?', with distinct color
                    attron(COLOR_PAIR(9));
                    mvaddch(y, x, '?');
                    attroff(COLOR_PAIR(9));
                    break;

                case WEAPON_MACE:
                case WEAPON_DAGGER:
                case WEAPON_MAGIC_WAND:
                case WEAPON_ARROW:
                case WEAPON_SWORD:
                    // Color weapons in yellow
                    attron(COLOR_PAIR(3));
                    mvaddch(y, x, tile);
                    attroff(COLOR_PAIR(3));
                    break;

                // Handle spells with colors
                case SPELL_HEALTH:
                    attron(COLOR_PAIR(COLOR_PAIR_HEALTH));
                    mvaddch(y, x, SPELL_HEALTH);
                    attroff(COLOR_PAIR(COLOR_PAIR_HEALTH));
                    break;
                case SPELL_SPEED:
                    attron(COLOR_PAIR(COLOR_PAIR_SPEED));
                    mvaddch(y, x, SPELL_SPEED);
                    attroff(COLOR_PAIR(COLOR_PAIR_SPEED));
                    break;
                case SPELL_DAMAGE:
                    attron(COLOR_PAIR(COLOR_PAIR_DAMAGE));
                    mvaddch(y, x, SPELL_DAMAGE);
                    attroff(COLOR_PAIR(COLOR_PAIR_DAMAGE));
                    break;

                // [Handle other tile types...]

                default:
                    // Normal tile printing with room theme color
                    mvaddch(y, x, tile);
                    break;
            }

            // Turn off room theme color
            switch(room_theme) {
                case THEME_NORMAL:
                case THEME_ENCHANT:
                case THEME_TREASURE:
                    attroff(COLOR_PAIR(COLOR_PAIR_ROOM_NORMAL));
                    attroff(COLOR_PAIR(COLOR_PAIR_ROOM_ENCHANT));
                    attroff(COLOR_PAIR(COLOR_PAIR_ROOM_TREASURE));
                    break;
                default:
                    // Default color
                    break;
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
    return NULL;  // Not in any room
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

void move_character(Player* player, int key, struct Map* game_map, int* hitpoints){
    struct Point new_location = player->location;
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

    char target_tile = game_map->grid[new_location.y][new_location.x];

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
            player->location = new_location;
        } else {
            // if the user has at least 1 key, let them choose
            bool has_key = (ancient_key_count > 0);
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
                    ancient_key_count--;
                    broken_key_count++;
                    mvprintw(4, 2, "The Ancient Key broke!");
                } else {
                    // Key successfully used => remove 1 key
                    ancient_key_count--;
                    door_room->password_unlocked = true; 
                    mvprintw(4, 2, "Door unlocked with the Ancient Key!");
                    // Player can pass through now
                    player->location = new_location;
                }
                refresh();
                getch();
                return; 
            } else {
                // Normal prompt for password (as before)...
                bool success = prompt_for_password_door(door_room);
                if (success) {
                    door_room->password_unlocked = true;
                    player->location = new_location;
                }
                return;
            }
        }
    }

    else if (target_tile == ANCIENT_KEY) {
        // Pick up the Ancient Key
        game_map->grid[new_location.y][new_location.x] = FLOOR; // Remove the key from the map
        ancient_key_count++;
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
        player->location = new_location;
        return;
    }
    else if (target_tile == SECRET_DOOR_REVEALED) {
        // Allow movement through the revealed secret door
        player->location = new_location;
        return;
    }

    else if (target_tile == ENEMY_FIRE_MONSTER) {
        // Find the enemy at the target location
        Enemy* enemy = NULL;
        for (int i = 0; i < game_map->enemy_count; i++) {
            if (game_map->enemies[i].position.x == new_location.x &&
                game_map->enemies[i].position.y == new_location.y) {
                enemy = &game_map->enemies[i];
                break;
            }
        }

        if (enemy) {
            // Initiate combat
            combat(player, enemy, game_map, hitpoints);
        }
        return; // Player doesn't move into the enemy's tile during combat
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
    player->location = new_location;

    
    handle_weapon_pickup(player, game_map, new_location);

    handle_spell_pickup(player, game_map, new_location);

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

    // Additional rendering for Treasure Room
    if (room->theme == THEME_TREASURE) {
        // Example: Place a treasure chest symbol at the center
        int center_x = (room->left_wall + room->right_wall) / 2;
        int center_y = (room->top_wall + room->bottom_wall) / 2;
        map->grid[center_y][center_x] = TREASURE_CHEST; // Define TREASURE_CHEST, e.g., '#'
    }
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

void save_current_game(struct UserManager* manager, struct Map* game_map, 
                      struct Point* character_location, int score, int current_level) {
    if (!manager->current_user) {
        mvprintw(0, 0, "Cannot save game as guest user.");
        refresh();
        getch();
        return;
    }

    char filename[256]; // Increased buffer size to prevent truncation
    char save_name[100];

    clear();
    echo();
    mvprintw(0, 0, "Enter name for this save: ");
    refresh();
    getnstr(save_name, sizeof(save_name) - 1); // Limit input to fit within the buffer
    noecho();

    // Truncate username and save_name if they exceed a safe length
    char safe_username[94]; // Leave space for prefix, underscore, extension
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
        .save_time = time(NULL)
    };
    strncpy(save.name, save_name, sizeof(save.name) - 1);

    fwrite(&save, sizeof(struct SavedGame), 1, file);
    fclose(file);

    mvprintw(2, 0, "Game saved successfully!");
    refresh();
    getch();
}

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

// Function to create a weapon based on its symbol
Weapon create_weapon(char symbol) {
    Weapon weapon;
    weapon.symbol = symbol;
    
    switch(symbol) {
        case WEAPON_MACE:
            strcpy(weapon.name, "Mace");
            weapon.damage = 15;
            break;
        case WEAPON_DAGGER:
            strcpy(weapon.name, "Dagger");
            weapon.damage = 10;
            break;
        case WEAPON_MAGIC_WAND:
            strcpy(weapon.name, "Magic Wand");
            weapon.damage = 20;
            break;
        case WEAPON_ARROW:
            strcpy(weapon.name, "Arrow");
            weapon.damage = 8;
            break;
        case WEAPON_SWORD:
            strcpy(weapon.name, "Sword");
            weapon.damage = 18;
            break;
        default:
            strcpy(weapon.name, "Unknown");
            weapon.damage = 0;
    }
    
    return weapon;
}

void add_weapons(struct Map* map) {
    const int WEAPON_COUNT = 10; // Adjust as needed
    int weapons_placed = 0;
    char weapon_symbols[] = {WEAPON_DAGGER, WEAPON_MAGIC_WAND, WEAPON_ARROW, WEAPON_SWORD};
    int num_weapon_types = sizeof(weapon_symbols) / sizeof(weapon_symbols[0]) - 1; //WEAPON_MACE cannot be placed in map


    while (weapons_placed < WEAPON_COUNT) {
        int x = rand() % MAP_WIDTH;
        int y = rand() % MAP_HEIGHT;

        // Place weapons only on floor tiles within rooms and ensure no overlap with existing items
        bool inside_room = false;
        for (int i = 0; i < map->room_count; i++) {
            if (isPointInRoom(&(struct Point){x, y}, &map->rooms[i])) {
                inside_room = true;
                break;
            }
        }

        if (inside_room && map->grid[y][x] == FLOOR) {
            // Randomly select a weapon type
            char weapon_symbol = weapon_symbols[rand() % num_weapon_types];
            map->grid[y][x] = weapon_symbol;
            weapons_placed++;
        }
    }
}

void handle_weapon_pickup(Player* player, struct Map* map, struct Point new_location) {
    char tile = map->grid[new_location.y][new_location.x];
    
    // Check if the tile is a weapon
    if (tile == WEAPON_MACE || tile == WEAPON_DAGGER || tile == WEAPON_MAGIC_WAND ||
        tile == WEAPON_ARROW || tile == WEAPON_SWORD) {
        
        if (player->weapon_count >= MAX_WEAPONS) {
            // Inventory full, notify the player
            mvprintw(12, 80, "Your weapon inventory is full! Cannot pick up %s.", symbol_to_name(tile));
            refresh();
            getch();
            return;
        }

        // Create the weapon based on the symbol
        Weapon picked_weapon = create_weapon(tile);
        player->weapons[player->weapon_count++] = picked_weapon;

        // Remove the weapon from the map
        map->grid[new_location.y][new_location.x] = FLOOR;

        // Notify the player
        char message[100];
        snprintf(message, sizeof(message), "You picked up a %s!", picked_weapon.name);
        mvprintw(12, 80, "%s", message);
        refresh();
        getch();
    }
}


const char* symbol_to_name(char symbol) {
    switch(symbol) {
        case WEAPON_MACE:
            return "Mace";
        case WEAPON_DAGGER:
            return "Dagger";
        case WEAPON_MAGIC_WAND:
            return "Magic Wand";
        case WEAPON_ARROW:
            return "Arrow";
        case WEAPON_SWORD:
            return "Sword";
        default:
            return "Unknown Weapon";
    }
}

void equip_weapon(Player* player, int weapon_index) {
    if (weapon_index < 0 || weapon_index >= player->weapon_count) {
        mvprintw(14, 80, "Invalid weapon index!");
        refresh();
        getch();
        return;
    }

    player->equipped_weapon = weapon_index;

    char message[100];
    snprintf(message, sizeof(message), "You have equipped the %s.", player->weapons[weapon_index].name);
    mvprintw(14, 80, "%s", message);
    refresh();
    getch();
}

void use_weapon(Player* player) {
    if (player->equipped_weapon == -1) {
        mvprintw(16, 80, "No weapon equipped!");
        refresh();
        getch();
        return;
    }

    Weapon* weapon = &player->weapons[player->equipped_weapon];
    char message[100];
    snprintf(message, sizeof(message), "You use the %s to deal %d damage.", weapon->name, weapon->damage);
    mvprintw(16, 80, "%s", message);
    refresh();
    getch();

    // Implement actual attack logic here (e.g., targeting an enemy)
    // Example:
    // attack_enemy(player->equipped_weapon->damage);
}

void open_weapon_inventory_menu(Player* player, struct Map* map) {
    bool menu_open = true;

    while (menu_open) {
        clear();
        mvprintw(0, 0, "Weapon Inventory:");

        if (player->weapon_count == 0) {
            mvprintw(2, 2, "No weapons collected.");
        } else {
            for (int i = 0; i < player->weapon_count; i++) {
                mvprintw(2 + i, 2, "Weapon %d: %s (Damage: %d)%s", 
                         i + 1, 
                         player->weapons[i].name, 
                         player->weapons[i].damage,
                         (player->equipped_weapon == i) ? " [Equipped]" : "");
            }
        }

        // Display menu options
        mvprintw(4 + player->weapon_count, 0, "Options:");
        mvprintw(5 + player->weapon_count, 2, "e <number> - Equip weapon");
        mvprintw(6 + player->weapon_count, 2, "u         - Use equipped weapon");
        mvprintw(7 + player->weapon_count, 2, "d <number> - Drop weapon");
        mvprintw(8 + player->weapon_count, 2, "q         - Quit weapon inventory");
        refresh();

        // Prompt for user input
        mvprintw(10 + player->weapon_count, 0, "Enter your choice: ");
        refresh();

        // Read input (simple approach)
        char input[10];
        echo();
        getnstr(input, sizeof(input) - 1);
        noecho();

        // Parse input
        if (strlen(input) == 0) {
            continue;
        }

        char command = tolower(input[0]);

        if (command == 'q') {
            menu_open = false;
            continue;
        }
        else if (command == 'e') {
            // Equip weapon
            if (player->weapon_count == 0) {
                mvprintw(12, 0, "No weapons to equip.");
                refresh();
                getch();
                continue;
            }

            int weapon_num = atoi(&input[1]) - 1; // Convert to 0-based index
            if (weapon_num >= 0 && weapon_num < player->weapon_count) {
                equip_weapon(player, weapon_num);
            } else {
                mvprintw(12, 0, "Invalid weapon number!");
                refresh();
                getch();
            }
        }
        else if (command == 'u') {
            // Use equipped weapon
            use_weapon(player);
        }
        else if (command == 'd') {
            // Drop weapon
            if (player->weapon_count == 0) {
                mvprintw(12, 0, "No weapons to drop.");
                refresh();
                getch();
                continue;
            }

            int weapon_num = atoi(&input[1]) - 1; // Convert to 0-based index
            if (weapon_num >= 0 && weapon_num < player->weapon_count) {
                // Drop the weapon on the current location if possible
                struct Point drop_location = player->location;
                char current_tile = map->grid[drop_location.y][drop_location.x];

                if (current_tile == FLOOR) {
                    // Place the weapon symbol on the map
                    map->grid[drop_location.y][drop_location.x] = player->weapons[weapon_num].symbol;

                    // Notify the player
                    char message[100];
                    snprintf(message, sizeof(message), "You dropped a %s at your current location.", player->weapons[weapon_num].name);
                    mvprintw(12, 0, "%s", message);
                    refresh();
                    getch();

                    // Remove the weapon from inventory
                    for (int i = weapon_num; i < player->weapon_count - 1; i++) {
                        player->weapons[i] = player->weapons[i + 1];
                    }
                    player->weapon_count--;

                    // If the dropped weapon was equipped, unequip it
                    if (player->equipped_weapon == weapon_num) {
                        player->equipped_weapon = -1;
                        mvprintw(13, 0, "You have unequipped your weapon.");
                        refresh();
                        getch();
                    }
                } else {
                    mvprintw(12, 0, "Cannot drop weapon here. Tile is not empty.");
                    refresh();
                    getch();
                }
            } else {
                mvprintw(12, 0, "Invalid weapon number!");
                refresh();
                getch();
            }
        }
        else {
            mvprintw(12, 0, "Invalid command!");
            refresh();
            getch();
        }
    }
}

// Function to create a spell based on its symbol
Spell create_spell(char symbol) {
    Spell spell;
    spell.symbol = symbol;
    spell.type = SPELL_UNKNOWN_TYPE;
    spell.effect_value = 0;

    switch(symbol) {
        case SPELL_HEALTH:
            strcpy(spell.name, "Health Spell");
            spell.type = SPELL_HEALTH_TYPE;
            spell.effect_value = 20; // Heals 20 hitpoints
            break;
        case SPELL_SPEED:
            strcpy(spell.name, "Speed Spell");
            spell.type = SPELL_SPEED_TYPE;
            spell.effect_value = 5; // Increases speed by 5 units (implementation dependent)
            break;
        case SPELL_DAMAGE:
            strcpy(spell.name, "Damage Spell");
            spell.type = SPELL_DAMAGE_TYPE;
            spell.effect_value = 15; // Deals 15 damage to enemies
            break;
        default:
            strcpy(spell.name, "Unknown Spell");
            spell.type = SPELL_UNKNOWN_TYPE;
            spell.effect_value = 0;
    }

    return spell;
}

void add_spells(struct Map* game_map) {
    const int SPELL_COUNT = 5; // Adjust as needed
    int spells_placed = 0;
    char spell_symbols[] = {SPELL_HEALTH, SPELL_SPEED, SPELL_DAMAGE};
    int num_spell_types = sizeof(spell_symbols) / sizeof(spell_symbols[0]);

    while (spells_placed < SPELL_COUNT) {
        int x = rand() % MAP_WIDTH;
        int y = rand() % MAP_HEIGHT;

        // Place spells only on floor tiles within rooms and ensure no overlap with existing items
        bool inside_room = false;
        for (int i = 0; i < game_map->room_count; i++) {
            if (isPointInRoom(&(struct Point){x, y}, &game_map->rooms[i])) {
                inside_room = true;
                break;
            }
        }

        if (inside_room && game_map->grid[y][x] == FLOOR) {
            // Randomly select a spell type
            char spell_symbol = spell_symbols[rand() % num_spell_types];
            game_map->grid[y][x] = spell_symbol;
            spells_placed++;
        }
    }
}

void handle_spell_pickup(Player* player, struct Map* map, struct Point new_location) {
    char tile = map->grid[new_location.y][new_location.x];
    
    // Check if the tile is a spell
    if (tile == SPELL_HEALTH || tile == SPELL_SPEED || tile == SPELL_DAMAGE) {
        
        if (player->spell_count >= MAX_SPELLS) {
            // Inventory full, notify the player
            mvprintw(12, 80, "Your spell inventory is full! Cannot pick up %s.", spell_type_to_name(
                (tile == SPELL_HEALTH) ? SPELL_HEALTH_TYPE :
                (tile == SPELL_SPEED)  ? SPELL_SPEED_TYPE :
                SPELL_DAMAGE_TYPE
            ));
            refresh();
            getch();
            return;
        }

        // Create the spell based on the symbol
        Spell picked_spell = create_spell(tile);
        player->spells[player->spell_count++] = picked_spell;

        // Remove the spell from the map
        map->grid[new_location.y][new_location.x] = FLOOR;

        // Notify the player
        char message[100];
        snprintf(message, sizeof(message), "You picked up a %s!", picked_spell.name);
        mvprintw(12, 80, "%s", message);
        refresh();
        getch();
    }
}

const char* spell_type_to_name(SpellType type) {
    switch(type) {
        case SPELL_HEALTH_TYPE:
            return "Health Spell";
        case SPELL_SPEED_TYPE:
            return "Speed Spell";
        case SPELL_DAMAGE_TYPE:
            return "Damage Spell";
        default:
            return "Unknown Spell";
    }
}

void open_spell_inventory_menu(struct Map* game_map, Player* player) {
    bool menu_open = true;

    while (menu_open) {
        clear();
        mvprintw(0, 0, "Spell Inventory:");

        if (player->spell_count == 0) {
            mvprintw(2, 2, "No spells collected.");
        } else {
            for (int i = 0; i < player->spell_count; i++) {
                mvprintw(2 + i, 2, "Spell %d: %s (Effect: %d)%s", 
                         i + 1, 
                         player->spells[i].name, 
                         player->spells[i].effect_value,
                         ""); // Optionally mark if spell is active
            }
        }

        // Display menu options
        mvprintw(4 + player->spell_count, 0, "Options:");
        mvprintw(5 + player->spell_count, 2, "u <number> - Use spell");
        mvprintw(6 + player->spell_count, 2, "d <number> - Drop spell");
        mvprintw(7 + player->spell_count, 2, "q         - Quit spell inventory");
        refresh();

        // Prompt for user input
        mvprintw(9 + player->spell_count, 0, "Enter your choice: ");
        refresh();

        // Read input (simple approach)
        char input[10];
        echo();
        getnstr(input, sizeof(input) - 1);
        noecho();

        // Parse input
        if (strlen(input) == 0) {
            continue;
        }

        char command = tolower(input[0]);

        if (command == 'q') {
            menu_open = false;
            continue;
        }
        else if (command == 'u') {
            // Use spell
            if (player->spell_count == 0) {
                mvprintw(12, 0, "No spells to use.");
                refresh();
                getch();
                continue;
            }

            int spell_num = atoi(&input[1]) - 1; // Convert to 0-based index
            if (spell_num >= 0 && spell_num < player->spell_count) {
                use_spell(player, NULL); // Pass game_map if spell effects depend on it
            } else {
                mvprintw(12, 0, "Invalid spell number!");
                refresh();
                getch();
            }
        }
        else if (command == 'd') {
            // Drop spell
            if (player->spell_count == 0) {
                mvprintw(12, 0, "No spells to drop.");
                refresh();
                getch();
                continue;
            }

            int spell_num = atoi(&input[1]) - 1; // Convert to 0-based index
            if (spell_num >= 0 && spell_num < player->spell_count) {
                // Drop the spell on the current location if possible
                struct Point drop_location = player->location;
                char current_tile = game_map->grid[drop_location.y][drop_location.x];

                if (current_tile == FLOOR) {
                    // Place the spell symbol on the map
                    game_map->grid[drop_location.y][drop_location.x] = player->spells[spell_num].symbol;

                    // Notify the player
                    char message[100];
                    snprintf(message, sizeof(message), "You dropped a %s at your current location.", player->spells[spell_num].name);
                    mvprintw(12, 0, "%s", message);
                    refresh();
                    getch();

                    // Remove the spell from inventory
                    for (int i = spell_num; i < player->spell_count - 1; i++) {
                        player->spells[i] = player->spells[i + 1];
                    }
                    player->spell_count--;
                } else {
                    mvprintw(12, 0, "Cannot drop spell here. Tile is not empty.");
                    refresh();
                    getch();
                }
            } else {
                mvprintw(12, 0, "Invalid spell number!");
                refresh();
                getch();
            }
        }
        else {
            mvprintw(12, 0, "Invalid command!");
            refresh();
            getch();
        }
    }
}

void use_spell(Player* player, struct Map* game_map) {
    if (player->spell_count == 0) {
        mvprintw(14, 80, "No spells to use!");
        refresh();
        getch();
        return;
    }

    // Prompt user to select a spell
    mvprintw(14, 80, "Enter spell number to use: ");
    refresh();

    char input[10];
    echo();
    getnstr(input, sizeof(input) - 1);
    noecho();

    if (strlen(input) == 0) {
        return;
    }

    int spell_num = atoi(input) - 1; // Convert to 0-based index

    if (spell_num < 0 || spell_num >= player->spell_count) {
        mvprintw(16, 80, "Invalid spell number!");
        refresh();
        getch();
        return;
    }

    Spell* spell = &player->spells[spell_num];

    switch(spell->type) {
        case SPELL_HEALTH_TYPE:
            player->hitpoints += spell->effect_value;
            if (player->hitpoints > 100) player->hitpoints = 100; // Assuming 100 is max
            mvprintw(16, 80, "Used Health Spell! Hitpoints increased by %d.", spell->effect_value);
            break;

        case SPELL_SPEED_TYPE:
            // Implement speed effect (e.g., increase movement speed for a duration)
            // This requires additional game logic to handle temporary effects
            mvprintw(16, 80, "Used Speed Spell! (Effect not implemented yet.)");
            break;

        case SPELL_DAMAGE_TYPE:
            // Implement damage effect (e.g., attack all enemies in range)
            // Requires enemy management
            mvprintw(16, 80, "Used Damage Spell! (Effect not implemented yet.)");
            break;

        default:
            mvprintw(16, 80, "Unknown spell type!");
            break;
    }

    refresh();
    getch();

    // Optionally, remove the spell after use if spells are single-use
    // Uncomment the following lines if spells are consumable
    /*
    for (int i = spell_num; i < player->spell_count - 1; i++) {
        player->spells[i] = player->spells[i + 1];
    }
    player->spell_count--;
    */
}

void add_enemies(struct Map* map, int current_level) {
    // Define the number of enemies based on the current level
    // For example, increase the number of enemies with each level
    int enemies_to_add = current_level * 2; // Adjust as needed
    
    for (int i = 0; i < enemies_to_add && map->enemy_count < MAX_ENEMIES; i++) {
        // Select a random room to place the enemy
        if (map->room_count == 0) break; // No rooms to place enemies
        
        int room_index = rand() % map->room_count;
        Room* room = &map->rooms[room_index];
        
        // Ensure the room is not the Treasure Room
        if (room->theme == THEME_TREASURE) continue;
        
        // Select a random position within the room
        int x = room->left_wall + 1 + rand() % (room->width - 2);
        int y = room->top_wall + 1 + rand() % (room->height - 2);
        
        // Check if the tile is empty (floor)
        if (map->grid[y][x] == FLOOR) {
            // Initialize the enemy
            Enemy enemy;
            enemy.type = ENEMY_FIRE_BREATHING_MONSTER;
            enemy.position.x = x;
            enemy.position.y = y;
            enemy.hp = ENEMY_HP_FIRE_MONSTER;
            enemy.active = false; // Initially inactive
            
            // Add enemy to the map
            map->enemies[map->enemy_count++] = enemy;
            
            // Represent the enemy on the map
            map->grid[y][x] = ENEMY_FIRE_MONSTER;
        }
    }
}

void render_enemies(struct Map* map) {
    for (int i = 0; i < map->enemy_count; i++) {
        Enemy* enemy = &map->enemies[i];
        
        // Only render active enemies
        if (enemy->active) {
            // Check if the enemy is still in the map
            if (map->grid[enemy->position.y][enemy->position.x] == ENEMY_FIRE_MONSTER) {
                attron(COLOR_PAIR(16)); // Enemy color
                mvaddch(enemy->position.y, enemy->position.x, ENEMY_FIRE_MONSTER);
                attroff(COLOR_PAIR(16));
            }
        }
    }
}

bool is_enemy_in_same_room(Player* player, Enemy* enemy, struct Map* map) {
    // Find the room the player is in
    struct Room* player_room = NULL;
    for (int i = 0; i < map->room_count; i++) {
        if (isPointInRoom(&player->location, &map->rooms[i])) {
            player_room = &map->rooms[i];
            break;
        }
    }

    if (player_room == NULL) return false; // Player is not in any room

    // Check if the enemy is in the same room
    for (int i = 0; i < map->room_count; i++) {
        if (isPointInRoom(&enemy->position, &map->rooms[i])) {
            return (player_room == &map->rooms[i]);
        }
    }

    return false;
}

void update_enemies(struct Map* map, Player* player, int* hitpoints) {
    for (int i = 0; i < map->enemy_count; i++) {
        Enemy* enemy = &map->enemies[i];

        if (!enemy->active) continue; // Skip inactive enemies

        // Simple AI: Move one step towards the player if within the room
        if (is_enemy_in_same_room(player, enemy, map)) {
            move_enemy_towards(player, enemy, map);
        }

        // Check if enemy has reached the player
        if (enemy->position.x == player->location.x && 
            enemy->position.y == player->location.y) {
            // Enemy attacks the player
            *hitpoints -= 5; // Adjust damage as needed
            add_game_message(map->enemy_count > 0 ? &((struct MessageQueue){0}) : NULL, "An enemy attacks you! HP decreased.", 4);
        }
    }
}

void move_enemy_towards(Player* player, Enemy* enemy, struct Map* map) {
    int dx = player->location.x - enemy->position.x;
    int dy = player->location.y - enemy->position.y;

    // Normalize movement to one step
    if (dx != 0) dx = dx / abs(dx);
    if (dy != 0) dy = dy / abs(dy);

    // Determine new position
    int new_x = enemy->position.x + dx;
    int new_y = enemy->position.y + dy;

    // Check map boundaries
    if (new_x < 0 || new_x >= MAP_WIDTH || new_y < 0 || new_y >= MAP_HEIGHT)
        return;

    char target_tile = map->grid[new_y][new_x];

    // Check if the tile is passable (floor or player)
    if (target_tile == FLOOR || target_tile == PLAYER_CHAR) {
        // Check if another enemy is already on the target tile
        bool occupied = false;
        for (int i = 0; i < map->enemy_count; i++) {
            if (i == enemy - map->enemies) continue; // Skip self
            if (map->enemies[i].position.x == new_x && 
                map->enemies[i].position.y == new_y) {
                occupied = true;
                break;
            }
        }

        if (!occupied) {
            // Update the map grid
            map->grid[enemy->position.y][enemy->position.x] = FLOOR; // Remove enemy from current position
            enemy->position.x = new_x;
            enemy->position.y = new_y;
            map->grid[new_y][new_x] = ENEMY_FIRE_MONSTER; // Place enemy at new position
        }
    }
    // If the target tile is another enemy or impassable, enemy stays in place
}

void combat(Player* player, Enemy* enemy, struct Map* map, int* hitpoints) {
    bool in_combat = true;
    while (in_combat) {
        clear();
        mvprintw(0, 0, "Combat with Fire Breathing Monster!");
        mvprintw(2, 0, "Your HP: %d", player->hitpoints);
        mvprintw(3, 0, "Enemy HP: %d", enemy->hp);
        mvprintw(5, 0, "Choose action: (A)ttack or (R)un");
        refresh();

        int choice = getch();
        switch (tolower(choice)) {
            case 'a':
                {
                    // Player attacks enemy
                    int damage = 0;
                    if (player->equipped_weapon != -1) {
                        damage = player->weapons[player->equipped_weapon].damage;
                    } else {
                        damage = 5; // Unarmed damage
                    }

                    enemy->hp -= damage;
                    if (enemy->hp <= 0) {
                        // Enemy defeated
                        add_game_message(map->enemy_count > 0 ? &((struct MessageQueue){0}) : NULL, "You defeated the Fire Breathing Monster!", 2);
                        // Remove enemy from the map
                        map->grid[enemy->position.y][enemy->position.x] = FLOOR;
                        // Shift enemies in the array
                        for (int i = 0; i < map->enemy_count; i++) {
                            if (&map->enemies[i] == enemy) {
                                for (int j = i; j < map->enemy_count - 1; j++) {
                                    map->enemies[j] = map->enemies[j + 1];
                                }
                                map->enemy_count--;
                                break;
                            }
                        }
                        in_combat = false;
                        break;
                    }

                    // Enemy attacks player
                    player->hitpoints -= 5; // Enemy damage
                    if (player->hitpoints <= 0) {
                        add_game_message(map->enemy_count > 0 ? &((struct MessageQueue){0}) : NULL, "The enemy has defeated you!", 7);
                        in_combat = false;
                        break;
                    }

                    // Continue combat
                    add_game_message(map->enemy_count > 0 ? &((struct MessageQueue){0}) : NULL, "You attacked the enemy.", 3);
                    add_game_message(map->enemy_count > 0 ? &((struct MessageQueue){0}) : NULL, "The enemy attacked you.", 4);
                }
                break;

            case 'r':
                {
                    // Attempt to run
                    // Simple logic: 50% chance to escape
                    if (rand() % 100 < 50) {
                        add_game_message(map->enemy_count > 0 ? &((struct MessageQueue){0}) : NULL, "You successfully escaped!", 3);
                        in_combat = false;
                        // Move player back to previous position
                        // (Implement movement logic if necessary)
                    } else {
                        add_game_message(map->enemy_count > 0 ? &((struct MessageQueue){0}) : NULL, "Failed to escape! The enemy attacks you.", 4);
                        // Enemy attacks player
                        player->hitpoints -= 5;
                        if (player->hitpoints <= 0) {
                            add_game_message(map->enemy_count > 0 ? &((struct MessageQueue){0}) : NULL, "The enemy has defeated you!", 7);
                            in_combat = false;
                        }
                    }
                }
                break;

            default:
                // Invalid input, continue combat
                break;
        }

        // Check for game over
        if (player->hitpoints <= 0) {
            in_combat = false;
            // Handle game over in the main loop
        }

        refresh();
    }
}
