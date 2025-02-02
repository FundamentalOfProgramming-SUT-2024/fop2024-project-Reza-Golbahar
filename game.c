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

    // Initialize weapons
    player->weapons[0] = create_weapon(WEAPON_MACE);

    // Initialize spells
    player->spell_count = 0; // Assuming no spells initially

    // Initialize temporary effects
    player->temporary_damage = 0;
    player->temporary_damage_start_time = 0;
    player->temporary_speed = 0;
    player->temporary_speed_start_time = 0;
    player->temporary_damage_timer = 0;
    player->temporary_speed_timer = 0;
}

void play_game(struct UserManager* manager, struct Map* game_map, 
               Player* player, int initial_score) {
    const int max_level = 4;  // Define maximum levels
    int current_level = 1;    // Start at level 1
    bool game_running = true;

    bool show_map = false;

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

    game_map->last_hunger_decrease = time(NULL);
    game_map->last_attack_time = time(NULL);

    // Keep track of the last direction used for ranged shots
    static int last_dx = 0;
    static int last_dy = 0;

    // Initialize visibility array (all tiles hidden initially)
    bool visible[MAP_HEIGHT][MAP_WIDTH] = {0}; 
    visible[player->location.y][player->location.x] = true;

    // Message Queue for game messages
    struct MessageQueue message_queue = { .count = 0 };

    while (game_running) {
        clear();

        if (show_map) {
            // Display the entire map
            print_full_map(game_map, &player->location, manager);
        } else {
            // Update visibility based on player's field of view
            update_visibility(game_map, &player->location, visible);
            print_map(game_map, visible, player->location, manager);
        }

        // Render enemies
        render_enemies(game_map);
        

        // After player movement, check for enemy activation
        for (int i = 0; i < game_map->enemy_count; i++) {
            Enemy* enemy = &game_map->enemies[i];
            
            if (!enemy->active && is_enemy_in_same_room(player, enemy, game_map)) {
                enemy->active = true;
                add_game_message(&message_queue, "An enemy has spotted you!", 16); // COLOR_PAIR_ENEMIES
            }
        }
        
        mvprintw(MAP_HEIGHT + 1, 0, 
            "Score: %d   HP: %d   Player: %s",
            player->score,
            player->hitpoints,
            manager->current_user ? manager->current_user->username : "Guest");
        
        // Show game info
        if (player->equipped_weapon != -1) {
        mvprintw(MAP_HEIGHT + 2, 0, "Level: %d                             Equipped Weapon: %s (Damage: %d)",
                     current_level, player->weapons[player->equipped_weapon].name, 
                     player->weapons[player->equipped_weapon].damage);
        } else {
            mvprintw(MAP_HEIGHT + 2, 0, "Level: %d                             Equipped Weapon: None"
                    ,current_level);
        }
        mvprintw(MAP_HEIGHT + 4, 0, "Controls: Arrow Keys to Move, 'i' - Weapon Inventory, 'e' - General Inventory, 'q' - Quit g-save");
        refresh();

        // Increase hunger rate over time
        if (frame_count % HUNGER_INCREASE_INTERVAL == 0) {
            if (player->hunger_rate < MAX_HUNGER) {
                player->hunger_rate++;
            }
        }

        // Decrease hitpoints if hunger exceeds threshold
        if (player->hunger_rate >= MAX_HUNGER) {
            if (hunger_damage_timer % HUNGER_DAMAGE_INTERVAL == 0) {
                player->hitpoints -= HITPOINT_DECREASE_RATE;
                add_game_message(&message_queue, "Hunger is too high! HP decreased.", 4); // COLOR_PAIR_TRAPS
            }
            hunger_damage_timer++;
        } else {
            hunger_damage_timer = 0; // Reset damage timer if hunger is below threshold
        }

        // Check if hitpoints are zero
        if (player->hitpoints <= 0) {
            add_game_message(&message_queue, "Game Over! You ran out of hitpoints.", 7); // COLOR_PAIR_FINAL_FAIL
            game_running = false;
        }

        // Possibly add gold and food periodically
        // Example: Add a new food and gold every 30 seconds
        static time_t last_item_add_time = 0;
        if (difftime(time(NULL), last_item_add_time) >= 30.0) {
            add_food(game_map, player);
            add_gold(game_map, player);
            last_item_add_time = time(NULL);
        }
        update_temporary_effects(player, game_map, &message_queue);

        update_password_display();

        // Update hunger rate and health regeneration
        update_hunger_and_health(player, game_map, &message_queue);

        // Update enemies
        update_enemies(game_map, player, &message_queue);

        // Display messages
        draw_messages(&message_queue, 0, MAP_WIDTH+1);
        update_messages(&message_queue);
        
        update_food_inventory(player, &message_queue);
        // Handle input

        
        int key = getch();
        if (key == 'm' || key == 'M') {
            show_map = !show_map;  // Toggle the map visibility flag
            continue;  // Skip the rest of the loop to refresh the display
        }
        if (key=='g'){
            if (manager->current_user)
                save_current_game(manager, game_map, player, current_level);
        }
        switch (key) {
            case KEY_UP:
            case KEY_DOWN:
            case KEY_LEFT:
            case KEY_RIGHT:
                // Move the character
                move_character(player, key, game_map, &player->hitpoints, &message_queue);

                update_enemies(game_map, player, &message_queue);
                // Check the tile the player moves onto
                char tile = game_map->grid[player->location.y][player->location.x];

                if (tile == STAIRS) {
                    // Handle level up logic
                    if (current_level < max_level) {
                        current_level++;
                        struct Room* current_room = NULL;

                        // Find the current room
                        for (int i = 0; i < game_map->room_count; i++) {
                            if (isPointInRoom(&player->location, &game_map->rooms[i])) {
                                current_room = &game_map->rooms[i];
                                break;
                            }
                        }

                        // Generate the next map with current_level and max_level
                        struct Map new_map = generate_map(current_room, current_level, max_level);
                        *game_map = new_map;

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
                } else if (tile == FOOD_NORMAL_SYM || tile == FOOD_GREAT_SYM || 
                    tile == FOOD_MAGICAL_SYM || tile == FOOD_ROTTEN_SYM) {
                    // Collect food
                } else if (tile == GOLD_NORMAL_SYM || tile == GOLD_BLACK_SYM) {
                    // Collect gold
                    handle_gold_collection(player, game_map, &message_queue);
                } else if (tile == TREASURE_CHEST_SYM) {
                    // Handle treasure chest
                    // Example: Increase score or provide rewards
                    add_game_message(&message_queue, "You found a Treasure Chest!", COLOR_PAIR_HEALTH);
                    // Implement further logic as needed
                } else if (tile == FLOOR) {
                    // Check for traps
                    for (int i = 0; i < game_map->trap_count; i++) {
                        Trap* trap = &game_map->traps[i];
                        if (trap->location.x == player->location.x && 
                            trap->location.y == player->location.y) {
                            if (!trap->triggered) {
                                trap->triggered = true;
                                game_map->grid[player->location.y][player->location.x] = TRAP_SYMBOL;
                                player->hitpoints -= 10;
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
                //open_inventory_menu(&player, &message_queue);
                open_food_inventory_menu(player, &message_queue);
                break;

            case 'a':
            case 'A':
                if (player->equipped_weapon != -1) {
                    Weapon* w = &player->weapons[player->equipped_weapon];
                    if (w->type == MELEE) {
                        // Melee usage
                        use_melee_weapon(player, w, game_map, &message_queue);
                    } else {
                        // Ranged usage - ask user for direction every time
                        throw_ranged_weapon_with_drop(player, w, game_map, &message_queue,
                                                    last_dx, last_dy);
                    }
                }
                break;
            
            case 'i':
            case 'I':
                // Open the weapon inventory menu
                open_weapon_inventory_menu(player, game_map, &message_queue);
                break;

            case ' ': // SPACE
                // Use or shoot the currently equipped weapon
                if (player->equipped_weapon != -1) {
                    Weapon* w = &player->weapons[player->equipped_weapon];
                    if (w->type == MELEE) {
                        // Melee usage
                        use_melee_weapon(player, w, game_map, &message_queue);
                    } else {
                        // Ranged usage - ask user for direction every time
                        ask_ranged_direction(&last_dx, &last_dy, w->name);
                        throw_ranged_weapon_with_drop(player, w, game_map, &message_queue,
                                                    last_dx, last_dy);
                    }
                }
                break;

            case 's':
            case 'S':
                // Open spell inventory menu
                open_spell_inventory_menu(game_map, player, &message_queue);
                break;

            case 'q':
                if(manager->current_user)
                    save_current_game(manager, game_map, player, current_level);

                game_running = false;
                break;
        }
        frame_count++;
    }
}

void print_full_map(struct Map* game_map, struct Point* character_location, struct UserManager* manager) {
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            char tile = game_map->grid[y][x];

            if (character_location->x == x && character_location->y == y) {
                // Determine the color for the player based on user settings
                int player_color_pair = 0; // Default color pair (could be defined in your code)

                if (manager->current_user) {
                    // Check the player's chosen color from settings
                    if (strcmp(manager->current_user->character_color, "Red") == 0) {
                        player_color_pair = COLOR_PAIR_DAMAGE; // Example: Red
                    } else if (strcmp(manager->current_user->character_color, "Blue") == 0) {
                        player_color_pair = COLOR_PAIR_SPEED; // Example: Blue
                    } else if (strcmp(manager->current_user->character_color, "Green") == 0) {
                        player_color_pair = COLOR_PAIR_HEALTH; // Example: Green
                    } else {
                        player_color_pair = COLOR_PAIR_HEALTH; // Default color
                    }
                }

                // Apply the chosen color for the player
                attron(COLOR_PAIR(player_color_pair)); 
                mvaddch(y, x, PLAYER_CHAR);  // PLAYER_CHAR is your character's symbol
                attroff(COLOR_PAIR(player_color_pair));
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
                case TREASURE_CHEST_SYM:
                    attron(COLOR_PAIR(COLOR_PAIR_ROOM_TREASURE)); // Treasure Room color
                    mvaddch(y, x, TREASURE_CHEST_SYM);
                    attroff(COLOR_PAIR(COLOR_PAIR_ROOM_TREASURE));
                    break;

                case FOOD_NORMAL_SYM:
                    attron(COLOR_PAIR(COLOR_PAIR_ROOM_NORMAL));
                    mvaddch(y, x, FOOD_NORMAL_SYM);
                    attroff(COLOR_PAIR(COLOR_PAIR_ROOM_NORMAL));
                    break;
                case FOOD_GREAT_SYM:
                    attron(COLOR_PAIR(COLOR_PAIR_ROOM_ENCHANT));
                    mvaddch(y, x, FOOD_GREAT_SYM);
                    attroff(COLOR_PAIR(COLOR_PAIR_ROOM_ENCHANT));
                    break;
                case FOOD_MAGICAL_SYM:
                    attron(COLOR_PAIR(COLOR_PAIR_ROOM_TREASURE));
                    mvaddch(y, x, FOOD_MAGICAL_SYM);
                    attroff(COLOR_PAIR(COLOR_PAIR_ROOM_TREASURE));
                    break;
                case FOOD_ROTTEN_SYM:
                    attron(COLOR_PAIR(COLOR_PAIR_DAMAGE));
                    mvaddch(y, x, FOOD_ROTTEN_SYM);
                    attroff(COLOR_PAIR(COLOR_PAIR_DAMAGE));
                    break;

                case GOLD_NORMAL_SYM:
                    attron(COLOR_PAIR(COLOR_PAIR_ROOM_NORMAL));
                    mvaddch(y, x, GOLD_NORMAL_SYM);
                    attroff(COLOR_PAIR(COLOR_PAIR_ROOM_NORMAL));
                    break;
                case GOLD_BLACK_SYM:
                    attron(COLOR_PAIR(COLOR_PAIR_ROOM_ENCHANT));
                    mvaddch(y, x, GOLD_BLACK_SYM);
                    attroff(COLOR_PAIR(COLOR_PAIR_ROOM_ENCHANT));
                    break;

                case DOOR_PASSWORD:
                    {
                        Room* door_room = find_room_by_position(game_map, x, y);
                        if (door_room && door_room->password_unlocked) {
                            // Green for unlocked door
                            attron(COLOR_PAIR(COLOR_PAIR_HEALTH));
                            mvaddch(y, x, DOOR_PASSWORD);
                            attroff(COLOR_PAIR(COLOR_PAIR_HEALTH));
                        } else {
                            // Red for locked door
                            attron(COLOR_PAIR(COLOR_PAIR_DAMAGE));
                            mvaddch(y, x, DOOR_PASSWORD);
                            attroff(COLOR_PAIR(COLOR_PAIR_DAMAGE));
                        }
                    }
                    break;

                case ENEMY_FIRE_MONSTER:
                    attron(COLOR_PAIR(COLOR_PAIR_SPEED)); // Define a new color pair for enemies
                    mvaddch(y, x, ENEMY_FIRE_MONSTER);
                    attroff(COLOR_PAIR(COLOR_PAIR_SPEED));
                    break;

                case TRAP_SYMBOL:
                    // Red for triggered traps
                    attron(COLOR_PAIR(COLOR_PAIR_DAMAGE));
                    mvaddch(y, x, tile);
                    attroff(COLOR_PAIR(COLOR_PAIR_DAMAGE));
                    break;

                case ANCIENT_KEY:
                    // Golden yellow for Ancient Key
                    attron(COLOR_PAIR(COLOR_PAIR_HEALTH));
                    mvaddstr(y, x, "▲");  // Unicode symbol
                    attroff(COLOR_PAIR(COLOR_PAIR_HEALTH));
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

        // Initialize player attributes
    Player player;
    initialize_player(&player, map.initial_position);

    add_food(&map, &player);
    add_gold(&map, &player);
    add_weapons(&map, &player);

    // Spawn items and features
    add_traps(&map);
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

// void open_inventory_menu(Player* player, struct MessageQueue* message_queue){
//     bool menu_open = true;

//     while (menu_open) {
//         clear();
//         mvprintw(0, 0, "General Inventory");

//         // 1) Show food inventory
//         mvprintw(2, 0, "Food Inventory:");
//         if (player->food_count == 0) {
//             mvprintw(3, 2, "No food items.");
//         } else {
//             for (int i = 0; i < player->food_count; i++) {
//                 mvprintw(3 + i, 2, "Food Slot %d", i + 1);
//             }
//         }

//         // 2) Show gold count
//         mvprintw(5 + player->food_count, 0, "Gold Collected: %d", player->gold_count);

//         // 3) Show Ancient Key info
//         mvprintw(6 + player->food_count, 0, "Ancient Keys (working): %d", player->ancient_key_count);
//         mvprintw(7 + player->food_count, 0, "Broken Key Pieces: %d", player->broken_key_count);

//         // 4) Instructions
//         mvprintw(9 + player->food_count, 0, "Options:");
//         mvprintw(10 + player->food_count, 2, "u <number> - Use food");
//         mvprintw(11 + player->food_count, 2, "c         - Combine broken keys");
//         mvprintw(12 + player->food_count, 2, "q         - Quit inventory");
//         refresh();

//         // Handle input
//         int key = getch();
//         if (key == 'q' || key == 'Q') {
//             menu_open = false;
//             continue;
//         }

//         switch (tolower(key)) {
//             case 'u': {
//                 // Using food from a slot
//                 mvprintw(14, 0, "Enter food slot number to use (1-%d): ", player->food_count);
//                 refresh();
//                 char input[10];
//                 echo();
//                 getnstr(input, sizeof(input) - 1);
//                 noecho();

//                 int slot = atoi(input) - 1; // Convert to 0-based index

//                 if (slot >= 0 && slot < player->food_count) {
//                     consume_food(player, slot, message_queue);
//                 } else {
//                     add_game_message(message_queue, "Invalid food slot number!", 7); // Red color
//                 }
//                 break;
//             }

//             case 'c': {
//                 // Combine broken key pieces into one working Ancient Key
//                 if (player->broken_key_count >= 2) {
//                     player->broken_key_count -= 2;
//                     player->ancient_key_count++;
//                     add_game_message(message_queue, "You combined two broken key pieces into one Ancient Key!", 2); // Green color
//                 } else {
//                     add_game_message(message_queue, "Not enough broken key pieces to combine!", 7); // Red color
//                 }
//                 break;
//             }

//             default:
//                 // Invalid key
//                 add_game_message(message_queue, "Invalid command in inventory!", 7); // Red color
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

void add_food(struct Map* map, Player* player) {
    if (map->food_count >= 100) return; // Prevent overflow

    // Randomly decide the type of food to add
    FoodType type = FOOD_NORMAL;
    int rand_val = rand() % 100;
    if (rand_val < 60) {
        type = FOOD_NORMAL;
    } else if (rand_val < 80) {
        type = FOOD_GREAT;
    } else if (rand_val < 95) {
        type = FOOD_MAGICAL;
    } else {
        type = FOOD_ROTTEN;
    }

    // Find a random empty tile to place the food
    int x, y;
    bool placed = false;
    for (int i = 0; i < 100; i++) { // Try 100 times
        x = rand() % MAP_WIDTH;
        y = rand() % MAP_HEIGHT;
        if (map->grid[y][x] == FLOOR) {
            // Place food
            map->foods[map->food_count].type = type;
            map->foods[map->food_count].position.x = x;
            map->foods[map->food_count].position.y = y;
            map->foods[map->food_count].spawn_time = time(NULL);
            map->foods[map->food_count].consumed = false;

            // Assign symbol based on food type
            switch(type) {
                case FOOD_NORMAL:
                    map->grid[y][x] = FOOD_NORMAL_SYM;
                    break;
                case FOOD_GREAT:
                    map->grid[y][x] = FOOD_GREAT_SYM;
                    break;
                case FOOD_MAGICAL:
                    map->grid[y][x] = FOOD_MAGICAL_SYM;
                    break;
                case FOOD_ROTTEN:
                    map->grid[y][x] = FOOD_ROTTEN_SYM;
                    break;
                default:
                    map->grid[y][x] = FOOD_NORMAL_SYM;
                    break;
            }

            map->food_count++;
            placed = true;
            break;
        }
    }

    if (!placed) {
        // Could not place food after 100 attempts
        // Optionally, handle this scenario
        printf("Warning: Failed to place food after 100 attempts.\n");
    }
}

// Function to update food items (transformations)
void update_food_items(struct Map* map, struct MessageQueue* message_queue) {
    time_t current_time = time(NULL);
    for (int i = 0; i < map->food_count; i++) {
        if (map->foods[i].consumed) continue;

        FoodType type = map->foods[i].type;
        time_t elapsed = current_time - map->foods[i].spawn_time;

        // Define transformation times (in seconds)
        // For example, after 60 seconds, transform Great and Magical to Normal
        // and Normal to Rotten
        if (type == FOOD_GREAT || type == FOOD_MAGICAL) {
            if (elapsed >= 60 && elapsed < 120) {
                // Transform Great/Magical to Normal
                map->foods[i].type = FOOD_NORMAL;
                map->grid[map->foods[i].position.y][map->foods[i].position.x] = 'F';
                char message[100];
                snprintf(message, sizeof(message), "Food at (%d, %d) has transformed to Normal Food.", 
                         map->foods[i].position.x, map->foods[i].position.y);
                add_game_message(message_queue, message, 14); // Yellow color
            }
            else if (elapsed >= 120) {
                // Transform Normal to Rotten
                map->foods[i].type = FOOD_ROTTEN;
                map->grid[map->foods[i].position.y][map->foods[i].position.x] = 'F';
                char message[100];
                snprintf(message, sizeof(message), "Food at (%d, %d) has spoiled into Rotten Food.", 
                         map->foods[i].position.x, map->foods[i].position.y);
                add_game_message(message_queue, message, 7); // Red color
            }
        }
        else if (type == FOOD_NORMAL) {
            if (elapsed >= 120) {
                // Transform Normal to Rotten
                map->foods[i].type = FOOD_ROTTEN;
                map->grid[map->foods[i].position.y][map->foods[i].position.x] = 'F';
                char message[100];
                snprintf(message, sizeof(message), "Food at (%d, %d) has spoiled into Rotten Food.", 
                         map->foods[i].position.x, map->foods[i].position.y);
                add_game_message(message_queue, message, 7); // Red color
            }
        }
    }
}

void update_food_inventory(Player* player, struct MessageQueue* message_queue) {
    time_t current_time = time(NULL);
    for (int i = 0; i < player->food_count; i++) {
        Food* food = &player->foods[i];
        if (!food->consumed && difftime(current_time, food->pickup_time) >= 60.0) {
            if (food->type == FOOD_GREAT || food->type == FOOD_MAGICAL) {
                food->type = FOOD_NORMAL;
                add_game_message(message_queue, "Great or Magical Food has turned into Normal Food.", 14);
            } else if (food->type == FOOD_NORMAL) {
                food->type = FOOD_ROTTEN;
            }
        }
    }
}

void open_food_inventory_menu(Player* player, struct MessageQueue* message_queue) {
    clear();
    
    // Tally how many of each type are unconsumed
    int normal_count = 0;
    int great_count = 0;
    int magical_count = 0;
    
    time_t current_time = time(NULL);

    // Track the most recent pickup_time for each type, to show elapsed seconds
    time_t normal_last_pick = 0;
    time_t great_last_pick = 0;
    time_t magical_last_pick = 0;

    // Count items in inventory
    for (int i = 0; i < player->food_count; i++) {
        Food* f = &player->foods[i];
        if (f->consumed) continue;  // skip consumed
        switch(f->type) {
            case FOOD_NORMAL:
                normal_count++;
                if (f->pickup_time > normal_last_pick) {
                    normal_last_pick = f->pickup_time;
                }
                break;
            case FOOD_GREAT:
                great_count++;
                if (f->pickup_time > great_last_pick) {
                    great_last_pick = f->pickup_time;
                }
                break;
            case FOOD_MAGICAL:
                magical_count++;
                if (f->pickup_time > magical_last_pick) {
                    magical_last_pick = f->pickup_time;
                }
                break;
            case FOOD_ROTTEN:
                // You might or might not want to display rotten here,
                // but user asked for Normal/Great/Magical specifically.
                break;
        }
    }

    // Compute how many seconds since each last item was picked
    double normal_elapsed   = normal_last_pick   ? difftime(current_time, normal_last_pick)   : 0;
    double great_elapsed    = great_last_pick    ? difftime(current_time, great_last_pick)    : 0;
    double magical_elapsed  = magical_last_pick  ? difftime(current_time, magical_last_pick)  : 0;

    // Print the menu
    mvprintw(0, 0, "Food Inventory:");

    // Show normal food row
    // If normal_count > 0, show how many seconds since last item was picked
    // If no items, that row's 'elapsed' is just 0 or some placeholder
    mvprintw(2, 0,
             "Normal Food: %d  (%.0f sec since last pick)  - Increases 10 HP, decreases hunger by 10.",
             normal_count, normal_elapsed);

    mvprintw(3, 0,
             "Great Food:  %d  (%.0f sec since last pick)  - +10 HP, +5 weapon dmg for 30s.",
             great_count, great_elapsed);

    mvprintw(4, 0,
             "Magical Food:%d  (%.0f sec since last pick)  - +10 HP, +Speed for 30s.",
             magical_count, magical_elapsed);

    // Instructions
    mvprintw(6, 0, "Press 'u' followed by <type> to use last item of that type (normal/great/magical).");
    mvprintw(7, 0, "Press 'q' to quit.");
    mvprintw(9, 0, "Choice: ");

    refresh();

    // Input (example approach)
    char input[32];
    echo();
    getnstr(input, 31);
    noecho();

    if (tolower(input[0]) == 'q') {
        // Quit
        return;
    }
    else if (tolower(input[0]) == 'u') {
        // e.g. user typed "u normal"
        char type_str[16];
        if (sscanf(input + 1, "%s", type_str) == 1) {
            // convert to lower
            for (int i = 0; type_str[i]; i++) {
                type_str[i] = tolower((unsigned char)type_str[i]);
            }

            // Decide which type to use
            FoodType type_to_consume = FOOD_NORMAL;
            if (strcmp(type_str, "normal") == 0) {
                type_to_consume = FOOD_NORMAL;
            }
            else if (strcmp(type_str, "great") == 0) {
                type_to_consume = FOOD_GREAT;
            }
            else if (strcmp(type_str, "magical") == 0) {
                type_to_consume = FOOD_MAGICAL;
            }
            else {
                add_game_message(message_queue, "Unknown food type! Use 'normal', 'great', or 'magical'.", 7);
                return;
            }

            // Find the last item of that type
            int index = find_last_food_of_type(player, type_to_consume);
            if (index == -1) {
                add_game_message(message_queue, "You have no unconsumed items of that type!", 7);
            } else {
                // Actually consume it
                consume_food(player, index, message_queue);
            }
        }
    }
    // If typed something else, just ignore or handle error
    getch();  // pause to let user see any resulting messages
}

void print_map(struct Map* game_map,
              bool visible[MAP_HEIGHT][MAP_WIDTH],
              struct Point character_location,
              struct UserManager* manager) {
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {

            // Determine the tile to print
            char tile = game_map->grid[y][x];

            // Draw the player
            if (character_location.x == x && character_location.y == y) {
                // Determine the color for the player based on user settings
                int player_color_pair = 0; // Default color pair (could be defined in your code)

                if (manager->current_user) {
                    // Check the player's chosen color from settings
                    if (strcmp(manager->current_user->character_color, "Red") == 0) {
                        player_color_pair = 7; // Assuming color pair 1 is Red
                    } else if (strcmp(manager->current_user->character_color, "Blue") == 0) {
                        player_color_pair = 9; // Assuming color pair 2 is Blue
                    } else if (strcmp(manager->current_user->character_color, "Green") == 0) {
                        player_color_pair = 2; // Assuming color pair 3 is Green
                    } else {
                        player_color_pair = 20; // Default color is White (pair 4)
                    }
                }

                // Apply the chosen color for the player
                attron(COLOR_PAIR(player_color_pair)); 
                mvaddch(y, x, PLAYER_CHAR);  // PLAYER_CHAR is your character's symbol
                attroff(COLOR_PAIR(player_color_pair));
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
                case 'T':  // Normal Food
                    attron(COLOR_PAIR(COLOR_PAIR_ROOM_NORMAL));
                    mvaddch(y, x, tile);
                    attroff(COLOR_PAIR(COLOR_PAIR_ROOM_NORMAL));
                    break;
                case 'G':  // Great Food
                    attron(COLOR_PAIR(COLOR_PAIR_ROOM_ENCHANT));
                    mvaddch(y, x, tile);
                    attroff(COLOR_PAIR(COLOR_PAIR_ROOM_ENCHANT));
                    break;
                case 'M':  // Magical Food
                    attron(COLOR_PAIR(COLOR_PAIR_ROOM_TREASURE));
                    mvaddch(y, x, tile);
                    attroff(COLOR_PAIR(COLOR_PAIR_ROOM_TREASURE));
                    break;
                case 'R':  // Rotten Food
                    attron(COLOR_PAIR(COLOR_PAIR_DAMAGE));
                    mvaddch(y, x, tile);
                    attroff(COLOR_PAIR(COLOR_PAIR_DAMAGE));
                    break;

                case '$':  // Normal Gold
                    attron(COLOR_PAIR(COLOR_PAIR_ROOM_NORMAL));
                    mvaddch(y, x, tile);
                    attroff(COLOR_PAIR(COLOR_PAIR_ROOM_NORMAL));
                    break;
                case 'B':  // Black Gold
                    attron(COLOR_PAIR(COLOR_PAIR_ROOM_ENCHANT));
                    mvaddch(y, x, tile);
                    attroff(COLOR_PAIR(COLOR_PAIR_ROOM_ENCHANT));
                    break;


                // case TREASURE_CHEST:
                //     attron(COLOR_PAIR(COLOR_PAIR_ROOM_TREASURE)); // Use Treasure Room color
                //     mvaddch(y, x, TREASURE_CHEST);
                //     attroff(COLOR_PAIR(COLOR_PAIR_ROOM_TREASURE));
                //     break;


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
                    attron(COLOR_PAIR(3));
                    mvprintw(y, x, "\u2692"); // ⚒
                    attroff(COLOR_PAIR(3));
                    break;
                case WEAPON_DAGGER:
                    attron(COLOR_PAIR(3));
                    mvprintw(y, x, "\U0001F5E1"); // 🗡
                    attroff(COLOR_PAIR(3));
                    break;
                case WEAPON_MAGIC_WAND:
                    attron(COLOR_PAIR(3));
                    mvprintw(y, x, "\U0001FA84"); // 🪄
                    attroff(COLOR_PAIR(3));
                    break;
                case WEAPON_ARROW:
                    attron(COLOR_PAIR(3));
                    mvprintw(y, x, "\u27B3");   // ➳
                    attroff(COLOR_PAIR(3));
                    break;
                case WEAPON_SWORD:
                    // Color weapons in yellow
                    attron(COLOR_PAIR(3));
                    mvprintw(y, x, "\u2694");   // ⚔
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

void move_character(Player* player, int key, struct Map* game_map, int* hitpoints, struct MessageQueue* message_queue){
    int steps_to_move = (player->speed_spell_steps > 0) ? 2 : 1;

    for (int step = 0; step < steps_to_move; step++) {
        struct Point new_location = player->location;
        switch (key) {
            case KEY_UP:    new_location.y--;      break;

            case KEY_DOWN:  new_location.y++;      break;

            case KEY_LEFT:  new_location.x--;      break;

            case KEY_RIGHT: new_location.x++;      break;
            default: return;
        }

        // Bounds check
        if (new_location.x < 0 || new_location.x >= MAP_WIDTH ||
            new_location.y < 0 || new_location.y >= MAP_HEIGHT) {
            break; // Out of map bounds, ignore
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
            add_game_message(message_queue, "You picked up an Ancient Key!", 2); //at the time 2 is the color green
            refresh();
            getch();
        }

        
            // Handle secret doors
        else if (target_tile == SECRET_DOOR_CLOSED) {
            // Reveal the secret door
            game_map->grid[new_location.y][new_location.x] = SECRET_DOOR_REVEALED;
            add_game_message(message_queue, "You discovered a secret door!", 2); //at the time 2 is the color green
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

        
        handle_weapon_pickup(player, game_map, new_location, message_queue);

        handle_spell_pickup(player, game_map, new_location, message_queue);

        
        collect_food(player, game_map, message_queue);
        handle_gold_collection(player, game_map, message_queue);


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
                    add_game_message(message_queue, "You triggered a trap! HP -10.", 7); //at the time 7 is the color red
                    refresh();
                }
                break;
            }
        }
    }


    if (player->health_spell_steps > 0) {
        player->health_spell_steps--;
        if (player->health_spell_steps == 0) {
            add_game_message(message_queue,
                "Your health regeneration is back to normal!",
                14); // e.g. yellow color
        }
    }
    if (player->damage_spell_steps > 0) {
        player->damage_spell_steps--;
        if (player->damage_spell_steps == 0) {
            add_game_message(message_queue,
                "Your damage is back to normal!",
                14);
        }
    }
    if (player->speed_spell_steps > 0) {
        player->speed_spell_steps--;
        if (player->speed_spell_steps == 0) {
            add_game_message(message_queue,
                "Your speed is back to normal!",
                14);
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
    map->trap_count = 0;
    map->enemy_count = 0;

    map->gold_count = 0;
    for (int i = 0; i < MAX_GOLDS; i++) {
        map->golds[i].type = GOLD_NORMAL;
        map->golds[i].collected = false;
        map->golds[i].position.x = -1;
        map->golds[i].position.y = -1;
    }

    map->food_count = 0;
    for (int i = 0; i < 100; i++) {
        map->foods[i].type = FOOD_NORMAL;
        map->foods[i].consumed = false;
        map->foods[i].position.x = -1;
        map->foods[i].position.y = -1;
        map->foods[i].spawn_time = 0;
    }

    map->last_hunger_decrease = time(NULL);
    map->last_attack_time = time(NULL);
}

void add_gold(struct Map* map, Player* player) {
    if (player->gold_count >= MAX_GOLDS) return; // Prevent overflow

    // Randomly decide the type of gold to add
    GoldType type = GOLD_NORMAL;
    int rand_val = rand() % 100;
    if (rand_val < 90) {
        type = GOLD_NORMAL;
    } else {
        type = GOLD_BLACK;
    }

    // Find a random empty tile to place the gold
    int x, y;
    bool placed = false;
    for (int i = 0; i < 100; i++) { // Try 100 times
        x = rand() % MAP_WIDTH;
        y = rand() % MAP_HEIGHT;
        if (map->grid[y][x] == FLOOR) {
            // Place gold
            map->golds[map->gold_count].type = type;
            map->golds[map->gold_count].position.x = x;
            map->golds[map->gold_count].position.y = y;
            map->golds[map->gold_count].collected = false;

            // Assign symbol based on gold type
            switch(type) {
                case GOLD_NORMAL:
                    map->grid[y][x] = GOLD_NORMAL_SYM;
                    break;
                case GOLD_BLACK:
                    map->grid[y][x] = GOLD_BLACK_SYM;
                    break;
                default:
                    map->grid[y][x] = GOLD_NORMAL_SYM;
                    break;
            }

            int gold_amount = (type == GOLD_NORMAL) ? (rand() % 100 + 1) : ((rand() % 100 + 1) * 2);
            player->gold_count += gold_amount;

            map->gold_count++;
            placed = true;
            break;
        }
    }

    if (!placed) {
        // Could not place gold after 100 attempts
        // Optionally, handle this scenario
        printf("Warning: Failed to place gold after 100 attempts.\n");
    }
}

// Function to handle gold collection
void handle_gold_collection(Player* player, struct Map* map, struct MessageQueue* message_queue) {
    struct Point pos = player->location;
    for (int i = 0; i < map->gold_count; i++) {
        if (i >= MAX_GOLDS) {
            printf("Error: Gold index out of bounds: %d\n", i);
            continue;
        }

        if (!map->golds[i].collected && map->golds[i].position.x == pos.x && map->golds[i].position.y == pos.y) {
            // Collect the gold
            GoldType type = map->golds[i].type;
            map->golds[i].collected = true;
            map->grid[pos.y][pos.x] = FLOOR; // Remove gold symbol from map

            if (type == GOLD_NORMAL) {
                int gold_amount = rand() % 100 + 1; // Random between 1 and 100
                player->gold_count += gold_amount;
                char message[100];
                snprintf(message, sizeof(message), "You collected Normal Gold: +%d Gold.", gold_amount);
                add_game_message(message_queue, message, COLOR_PAIR_HEALTH); // Green color
            }
            else if (type == GOLD_BLACK) {
                int gold_amount = (rand() % 100 + 1) * 2; // Twice normal gold
                player->gold_count += gold_amount;
                char message[100];
                snprintf(message, sizeof(message), "You collected Black Gold: +%d Gold.", gold_amount);
                add_game_message(message_queue, message, COLOR_PAIR_SPEED); // Blue color
            }

            break; // Assuming only one gold item per tile
        }
    }
}

// Function to update hunger rate and health regeneration
void update_hunger_and_health(Player* player, struct Map* map, struct MessageQueue* message_queue) {
    time_t current_time = time(NULL);

    // Check if 40 seconds have passed since last hunger decrease
    if (difftime(current_time, map->last_hunger_decrease) >= 40.0) {
        // Decrease hunger rate at a constant speed, e.g., decrease by 1
        if (player->hunger_rate > 0) {
            player->hunger_rate -= 1;
            map->last_hunger_decrease = current_time;
            char message[100];
            snprintf(message, sizeof(message), "Hunger rate decreased by 1.");
            add_game_message(message_queue, message, 14); // Yellow color
        }
    }

    if (player->hunger_rate == 0) {
        // If enough time has passed
        if ( difftime(current_time, map->last_attack_time) >= 60.0 ) {
            // Base = +1 HP
            int regen_amount = 1;
            // Double if health_spell_steps > 0
            if (player->health_spell_steps > 0) {
                regen_amount *= 2; // => 2 HP
            }
            player->hitpoints += regen_amount;
            if (player->hitpoints > 100) player->hitpoints = 100;

            add_game_message(message_queue,
                (regen_amount == 2)
                ? "Regenerated 2 HP (Health Spell active)!"
                : "Regenerated 1 HP.",
                2);
        }
    }

    // Handle temporary effects expiration
    if (player->temporary_damage > 0 && difftime(current_time, player->temporary_damage_start_time) >= 30.0) {
        player->temporary_damage = 0;
        char message[100];
        snprintf(message, sizeof(message), "Temporary damage boost has worn off.");
        add_game_message(message_queue, message, 14); // Yellow color
    }

    if (player->temporary_speed > 0 && difftime(current_time, player->temporary_speed_start_time) >= 30.0) {
        player->temporary_speed = 0;
        char message[100];
        snprintf(message, sizeof(message), "Temporary speed boost has worn off.");
        add_game_message(message_queue, message, 14); // Yellow color
    }
}


bool isPointInRoom(struct Point* point, struct Room* room) {
    return (point->x >= room->left_wall && point->x <= room->right_wall &&
            point->y >= room->top_wall && point->y <= room->bottom_wall);
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
    msg->time_to_live = 10;  // Show for ~2 seconds at 30 fps
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
        map->grid[center_y][center_x] = TREASURE_CHEST_SYM; // Define TREASURE_CHEST, e.g., '#'
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

void save_current_game(struct UserManager* manager, struct Map* game_map, 
                       Player* player, int current_level) {
    if (!manager->current_user) {
        mvprintw(0, 0, "Cannot save game as guest user.");
        refresh();
        getch();
        return;
    }

        // Update scoreboard info
    manager->current_user->score += player->score;
    manager->current_user->gold  += player->gold_count;
    save_users_to_json(manager);  // So scoreboard is updated

    char filename[256]; // Ensure buffer is large enough
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

    struct SavedGame save;
    // Ensure that SavedGame struct includes a Player member
    // Assuming SavedGame is defined as follows:
    // typedef struct {
    //     struct Map game_map;
    //     Player player;
    //     int current_level;
    //     char name[100];
    //     time_t save_time;
    // } SavedGame;

    save.game_map = *game_map;
    save.player = *player;
    save.current_level = current_level;
    strncpy(save.name, save_name, sizeof(save.name) - 1);
    save.name[sizeof(save.name) - 1] = '\0';
    save.save_time = time(NULL);

    fwrite(&save, sizeof(struct SavedGame), 1, file);
    fclose(file);

    mvprintw(2, 0, "Game saved successfully!");
    refresh();
    getch();
}

bool load_saved_game(struct UserManager* manager, struct SavedGame* saved_game) {
    if (!manager->current_user) {
        mvprintw(0, 0, "Cannot load game as guest user.");
        getch();
        return false;
    }

    clear();
    mvprintw(0, 0, "Enter save name to load: ");
    list_saved_games(manager); // prints saves
    mvprintw(2, 0, "> ");
    refresh();

    char save_name[100];
    echo();
    getnstr(save_name, sizeof(save_name)-1);
    noecho();

    char filename[256];
    snprintf(filename, sizeof(filename),
             "saves/%s_%s.sav",
             manager->current_user->username, save_name);

    FILE* file = fopen(filename, "rb");
    if (!file) {
        mvprintw(4, 0, "Error: Could not find save file.");
        getch();
        return false;
    }

    fread(saved_game, sizeof(*saved_game), 1, file);
    fclose(file);

    return true; // success
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
// -------------- Create weapon with new Unicode symbols -------------
Weapon create_weapon(char symbol) {
    Weapon weapon;
    weapon.symbol = symbol;

    switch (symbol) {
        case WEAPON_MACE: // '1'
            // Use the Unicode symbol ⚒
            strcpy(weapon.name, u8"⚒ Mace");
            weapon.damage = 5;
            weapon.type = MELEE;
            weapon.quantity = 1;
            break;

        case WEAPON_DAGGER: // '2'
            strcpy(weapon.name, u8"🗡 Dagger");
            weapon.damage = 12;
            weapon.type = RANGED;
            weapon.quantity = 10;
            break;

        case WEAPON_MAGIC_WAND: // '3'
            strcpy(weapon.name, u8"🪄 Magic Wand");
            weapon.damage = 15;
            weapon.type = RANGED;
            weapon.quantity = 8;
            break;

        case WEAPON_ARROW: // '4'
            strcpy(weapon.name, u8"➳ Arrow");
            weapon.damage = 5;
            weapon.type = RANGED;
            weapon.quantity = 20;
            break;

        case WEAPON_SWORD: // '5'
            strcpy(weapon.name, u8"⚔ Sword");
            weapon.damage = 10;
            weapon.type = MELEE;
            weapon.quantity = 1;
            break;

        default:
            strcpy(weapon.name, "Unknown");
            weapon.damage = 0;
            weapon.type = MELEE;
            weapon.quantity = 1;
            break;
    }

    return weapon;
}


void add_weapons(struct Map* map, Player* player) {
    const int WEAPON_COUNT = 10; // Adjust as needed
    int weapons_placed = 0;
    char weapon_symbols[] = {WEAPON_DAGGER, WEAPON_MAGIC_WAND, WEAPON_ARROW, WEAPON_SWORD};
    int num_weapon_types = sizeof(weapon_symbols) / sizeof(weapon_symbols[0]);

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
            char weapon_symbol = weapon_symbols[rand() % num_weapon_types];

            // Ensure player can only own one sword
            if (weapon_symbol == WEAPON_SWORD) {
                bool sword_owned = false;
                for (int i = 0; i < player->weapon_count; i++) {
                    if (player->weapons[i].symbol == WEAPON_SWORD) {
                        sword_owned = true;
                        break;
                    }
                }

                if (sword_owned) {
                    continue; // Skip placing another sword if already owned
                }
            }

            map->grid[y][x] = weapon_symbol;
            weapons_placed++;
        }
    }
}

void open_weapon_inventory_menu(Player* player,
                                struct Map* map,
                                struct MessageQueue* msg_queue){
    bool menu_open = true;

    while (menu_open) {
        clear();
        mvprintw(0, 0, "Weapon Inventory:");

        // Show all weapons by index
        for (int i = 0; i < player->weapon_count; i++) {
            Weapon* w = &player->weapons[i];
            bool is_equipped = (player->equipped_weapon == i);
            
            // If you have a damage spell, multiply displayed damage
            int effective_damage = w->damage + player->temporary_damage;
            if (player->damage_spell_steps > 0) {
                effective_damage *= 2;
            }

            mvprintw(i + 2, 0,
               "[%d] %s - Damage: %d, Quantity: %d%s",
               i + 1,
               w->name,
               effective_damage,
               w->quantity,
               is_equipped ? " (Equipped)" : "");
        }

        mvprintw(player->weapon_count + 4, 0, 
            "Commands:\n"
            "  e <index> - Equip weapon\n"
            "  d <index> - Drop weapon\n"
            "  q         - Quit\n"
            "Your choice: ");

        refresh();

        char input[32];
        echo();
        getnstr(input, sizeof(input)-1);
        noecho();
        if (!strlen(input)) continue;

        char cmd;
        int index;
        if (sscanf(input, "%c %d", &cmd, &index) == 2) {
            int widx = index - 1;  // zero-based
            if (widx < 0 || widx >= player->weapon_count) {
                add_game_message(msg_queue, "Invalid weapon index!", 7);
                continue;
            }

            if (tolower(cmd) == 'e') {
                // Equip
                player->equipped_weapon = widx;
                char buf[64];
                snprintf(buf, sizeof(buf),
                         "Equipped %s.", player->weapons[widx].name);
                add_game_message(msg_queue, buf, 2);

            } else if (tolower(cmd) == 'd') {
                // Drop logic: 
                // 1) Check if tile is floor
                // 2) If RANGED => drop 1 ammo or entire stack
                // 3) If MELEE => remove from inventory, place tile if you want
                struct Point drop_loc = player->location;
                if (map->grid[drop_loc.y][drop_loc.x] != FLOOR) {
                    add_game_message(msg_queue, "Cannot drop here!", 7);
                    continue;
                }
                Weapon* w = &player->weapons[widx];
                if (w->type == RANGED) {
                    if (w->quantity > 0) {
                        w->quantity--; // dropping 1
                        map->grid[drop_loc.y][drop_loc.x] = w->symbol; // place it
                        add_game_message(msg_queue,
                            "Dropped 1 ammo of your ranged weapon.",
                            2);
                        // If quantity is 0 => remove from inventory
                        if (w->quantity == 0) {
                            if (player->equipped_weapon == widx) {
                                player->equipped_weapon = -1;
                            }
                            // shift everything
                            for (int j = widx; j < player->weapon_count - 1; j++) {
                                player->weapons[j] = player->weapons[j + 1];
                            }
                            player->weapon_count--;
                        }
                    }
                } else {
                    // MELEE => remove from inventory completely
                    map->grid[drop_loc.y][drop_loc.x] = FLOOR; // or place the symbol if you want
                    if (player->equipped_weapon == widx) {
                        player->equipped_weapon = -1;
                    }
                    for (int j = widx; j < player->weapon_count - 1; j++) {
                        player->weapons[j] = player->weapons[j + 1];
                    }
                    player->weapon_count--;
                    add_game_message(msg_queue, "Dropped melee weapon.", 2);
                }

            }
        } else if (tolower(input[0]) == 'q') {
            menu_open = false;
        } else {
            add_game_message(msg_queue, "Invalid command!", 7);
        }
    }
}

// Update weapon pickup logic to increment counts appropriately
void handle_weapon_pickup(Player* player, struct Map* map, struct Point new_location, struct MessageQueue* message_queue) {
    char tile = map->grid[new_location.y][new_location.x];

    if (tile == WEAPON_MACE || tile == WEAPON_DAGGER || tile == WEAPON_MAGIC_WAND ||
        tile == WEAPON_ARROW || tile == WEAPON_SWORD) {

        Weapon* existing_weapon = NULL;
        for (int i = 0; i < player->weapon_count; i++) {
            if (player->weapons[i].symbol == tile) {
                existing_weapon = &player->weapons[i];
                break;
            }
        }

        if (tile == WEAPON_SWORD && existing_weapon) {
            add_game_message(message_queue, "You already own a Sword!", 7); // Red color
            return;
        }

        if (!existing_weapon) {
            if (player->weapon_count >= MAX_WEAPONS) {
                add_game_message(message_queue, "Your weapon inventory is full!", 7);
                return;
            }

            Weapon picked_weapon = create_weapon(tile);
            player->weapons[player->weapon_count++] = picked_weapon;
            add_game_message(message_queue, "You picked up a new weapon!", 2);
        } else {
            switch (tile) {
                case WEAPON_DAGGER:
                    existing_weapon->quantity += 10;
                    if (existing_weapon->quantity > 50) existing_weapon->quantity = 50;
                    add_game_message(message_queue, "Picked up 10 Daggers.", 2);
                    break;
                case WEAPON_MAGIC_WAND:
                    existing_weapon->quantity += 8;
                    if (existing_weapon->quantity > 50) existing_weapon->quantity = 50;
                    add_game_message(message_queue, "Picked up 8 Magic Wands.", 2);
                    break;
                case WEAPON_ARROW:
                    existing_weapon->quantity += 20;
                    if (existing_weapon->quantity > 50) existing_weapon->quantity = 50;
                    add_game_message(message_queue, "Picked up 20 Arrows.", 2);
                    break;
                default:
                    break;
            }
        }

        if (tile == 'x') {
            // This is dropped ammo
            // create a new weapon with quantity=1
            Weapon dropped;
            dropped.symbol = WEAPON_ARROW; // or dagger, depends on how you know which weapon was dropped
            // or parse from the original weapon name if needed
            // for simplicity let's assume you only do this for arrow, or you store the original symbol
            strcpy(dropped.name, "➳ Arrow (dropped)");
            dropped.damage = 5;
            dropped.type = RANGED;
            dropped.quantity = 1;

            // Now add it to the player's inventory
            // ...
        }

        map->grid[new_location.y][new_location.x] = FLOOR;
    }
}

bool is_melee_weapon(Weapon weapon) {
    // Return true if weapon is melee, false if ranged
    return (weapon.symbol == WEAPON_SWORD || weapon.symbol == WEAPON_MACE);
}

int get_ranged_weapon_count(Player* player, Weapon weapon) {
    int count = 0;
    for (int i = 0; i < player->weapon_count; i++) {
        if (player->weapons[i].symbol == weapon.symbol) {
            count++;
        }
    }
    return count;
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

void handle_spell_pickup(Player* player, struct Map* map, struct Point new_location, struct MessageQueue* message_queue) {
    char tile = map->grid[new_location.y][new_location.x];
    
    // Check if the tile is a spell
    if (tile == SPELL_HEALTH || tile == SPELL_SPEED || tile == SPELL_DAMAGE) {
        if (player->spell_count >= MAX_SPELLS) {
            char full_message[100];
            snprintf(full_message, sizeof(full_message), "Your spell inventory is full! Cannot pick up %s.", spell_type_to_name(
               (tile == SPELL_HEALTH) ? SPELL_HEALTH_TYPE :
                (tile == SPELL_SPEED)  ? SPELL_SPEED_TYPE :
               SPELL_DAMAGE_TYPE
            ));
            add_game_message(message_queue, full_message, 7); //at the time 7 is the color red
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
        add_game_message(message_queue, message, 2); // at the time 2 is the color green
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

void open_spell_inventory_menu(struct Map* game_map, Player* player, struct MessageQueue* message_queue) {
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
                use_spell(player, NULL, message_queue); // Pass game_map if spell effects depend on it
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

void use_spell(Player* player, struct Map* game_map, struct MessageQueue* message_queue) {
    // If no spells in inventory
    if (player->spell_count == 0) {
        add_game_message(message_queue, "No spells to use!", 7);
        return;
    }

    // Clear the screen or show them a small prompt
    clear();
    mvprintw(0, 0, "Your Spells:\n");
    for (int i = 0; i < player->spell_count; i++) {
        mvprintw(i + 1, 2, "%d) %s (Effect: %d)",
                 i + 1,
                 player->spells[i].name,
                 player->spells[i].effect_value);
    }
    mvprintw(player->spell_count + 2, 0,
             "Enter the number of the spell to use (or 0 to cancel): ");
    refresh();

    // Get user input
    echo();
    char input[10];
    getnstr(input, sizeof(input) - 1);
    noecho();

    int choice = atoi(input);
    if (choice <= 0 || choice > player->spell_count) {
        // either canceled (0) or invalid index
        return;
    }

    // Convert to zero-based
    int index = choice - 1;
    Spell* chosen_spell = &player->spells[index];

    // Apply effect:
    switch (chosen_spell->type) {
        case SPELL_HEALTH_TYPE: {
            // Double regen: set `player->health_spell_steps = 10;`
            player->health_spell_steps = 10;
            add_game_message(message_queue,
                "You used a Health Spell! Health regeneration is doubled for the next 10 moves.",
                2);
            break;
        }

        case SPELL_DAMAGE_TYPE: {
            // Double weapon damage: set `player->damage_spell_steps = 10;`
            player->damage_spell_steps = 10;
            add_game_message(message_queue,
                "You used a Damage Spell! Weapon damage is doubled for the next 10 moves.",
                2);
            break;
        }

        case SPELL_SPEED_TYPE: {
            // Speed up movement: `player->speed_spell_steps = 10;`
            player->speed_spell_steps = 10;
            add_game_message(message_queue,
                "You used a Speed Spell! You will move 2 tiles per step for the next 10 moves.",
                2);
            break;
        }

        default:
            add_game_message(message_queue,
                "Unknown spell type. Nothing happens.", 7);
            return;
    }

    // If spells are single-use, remove from inventory
    for (int i = index; i < player->spell_count - 1; i++) {
        player->spells[i] = player->spells[i + 1];
    }
    player->spell_count--;

    // Wait a moment or keypress, if you like:
    getch();
}


void add_enemies(struct Map* map, int current_level) {
    int enemies_to_add = current_level * 2;

    for (int i = 0; i < enemies_to_add && map->enemy_count < MAX_ENEMIES; i++) {
        if (map->room_count == 0) break;

        int room_index = rand() % map->room_count;
        Room* room = &map->rooms[room_index];
        if (room->theme == THEME_TREASURE) continue;

        int x = room->left_wall + 1 + rand() % (room->width - 2);
        int y = room->top_wall + 1 + rand() % (room->height - 2);

        if (map->grid[y][x] == FLOOR) {
            Enemy enemy;
            int enemy_type_rand = rand() % 5;

            switch (enemy_type_rand) {
                case 0: enemy.type = ENEMY_DEMON; break;
                case 1: enemy.type = ENEMY_FIRE_BREATHING_MONSTER; break;
                case 2: enemy.type = ENEMY_GIANT; break;
                case 3: enemy.type = ENEMY_SNAKE; break;
                case 4: enemy.type = ENEMY_UNDEAD; break;
            }


            enemy.position.x = x;
            enemy.position.y = y;
            enemy.active = false;
            enemy.chasing = false;
            enemy.adjacent_attack = false;

            map->enemies[map->enemy_count++] = enemy;
            map->grid[y][x] = enemy.type;
        }
    }
}

void render_enemies(struct Map* map) {
    for (int i = 0; i < map->enemy_count; i++) {
        Enemy* enemy = &map->enemies[i];
        
        // Only render active enemies
        if (enemy->active) {
            // Check if the enemy is still in the map
            char symbol = enemy->type;
            if (symbol == ENEMY_FIRE_MONSTER || symbol == ENEMY_DEMON || symbol == ENEMY_GIANT ||
                symbol == ENEMY_SNAKE || symbol == ENEMY_UNDEAD) {
                
                // Determine color based on enemy type
                int color_pair = 16; // Default enemy color
                switch(enemy->type) {
                    case ENEMY_DEMON:
                        color_pair = 17; // Define in your color initialization
                        break;
                    case ENEMY_GIANT:
                        color_pair = 18;
                        break;
                    case ENEMY_SNAKE:
                        color_pair = 19;
                        break;
                    case ENEMY_UNDEAD:
                        color_pair = 20;
                        break;
                    // Add cases as needed
                    default:
                        color_pair = 16;
                        break;
                }

                attron(COLOR_PAIR(color_pair));
                mvaddch(enemy->position.y, enemy->position.x, symbol);
                attroff(COLOR_PAIR(color_pair));
            }
        }
    }
}

bool is_enemy_in_same_room(Player* player, Enemy* enemy, struct Map* map) {
    return (find_room_by_position(map, player->location.x, player->location.y) == find_room_by_position(map, enemy->position.x, enemy->position.y));
}

void update_enemies(struct Map* map, Player* player, struct MessageQueue* message_queue) {
    for (int i = 0; i < map->enemy_count; i++) {
        Enemy* enemy = &map->enemies[i];
        if ( is_adjacent(player->location, enemy->position)) {
            enemy->active = true;
            enemy->chasing = true;
            char msg[100];
            snprintf(msg, sizeof(msg), "A %c is activated with %d HP!", enemy->type, enemy->hp);
            add_game_message(message_queue, msg, 2);
        }

        if (enemy->chasing) {
            move_enemy_towards_player(enemy, player, map);

            if (enemy->type == 'G' || enemy->type == 'U') {
                int distance = manhattanDistance(player->location.x, player->location.y, enemy->position.x, enemy->position.y);
                if (distance > 5) {
                    enemy->chasing = false;
                }
            } else if (enemy->type == 'D' || enemy->type == 'F') {
                if (!is_enemy_in_same_room(player, enemy, map)) {
                    enemy->chasing = false;
                }
            }
        }

        if (is_adjacent(player->location, enemy->position)) {
            player->hitpoints -= enemy->hp;
            map->last_attack_time = time(NULL);
            char attack_msg[100];
            snprintf(attack_msg, sizeof(attack_msg), "A %c attacked you! Damage: %d", enemy->type, enemy->hp);
            add_game_message(message_queue, attack_msg, 7);
            if (player->hitpoints <= 0) {
                add_game_message(message_queue, "Game Over! You were defeated.", 7);
            }
        }
    }
}

void move_enemy_towards_player(Enemy* enemy, Player* player, struct Map* map) {
    int dx = player->location.x - enemy->position.x;
    int dy = player->location.y - enemy->position.y;

    if (dx != 0) dx = dx / abs(dx);
    if (dy != 0) dy = dy / abs(dy);

    int new_x = enemy->position.x + dx;
    int new_y = enemy->position.y + dy;

    if (new_x >= 0 && new_x < MAP_WIDTH && new_y >= 0 && new_y < MAP_HEIGHT && map->grid[new_y][new_x] == FLOOR) {
        map->grid[enemy->position.y][enemy->position.x] = FLOOR;
        enemy->position.x = new_x;
        enemy->position.y = new_y;
        map->grid[new_y][new_x] = enemy->type;
    }
}

// Function to check if two points are adjacent (including diagonal)
bool is_adjacent(struct Point p1, struct Point p2) {
    return (abs(p1.x - p2.x) <= 1 && abs(p1.y - p2.y) <= 1);
}

// Function to check if the tile is valid for weapon interaction
bool is_valid_tile(int x, int y) {
    return (x >= 0 && x < MAP_WIDTH && y >= 0 && y < MAP_HEIGHT);
}

// Function to deal damage to enemy at specific tile location
void deal_damage_to_enemy(struct Map* map, int x, int y, int damage, struct MessageQueue* message_queue) {
    for (int i = 0; i < map->enemy_count; i++) {
        Enemy* enemy = &map->enemies[i];
        if (enemy->position.x == x && enemy->position.y == y) {
            enemy->hp -= damage;
            if (enemy->hp <= 0) {
                // Remove enemy from the map
                map->grid[enemy->position.y][enemy->position.x] = FLOOR;
                // Remove enemy from the enemy list
                for (int j = i; j < map->enemy_count - 1; j++) {
                    map->enemies[j] = map->enemies[j + 1];
                }
                map->enemy_count--;
                add_game_message(message_queue, "You defeated an enemy!", 2); // Green color
                break; // Exit the loop after removing the enemy
            }
        }
    }
}

// Function to stun an enemy
void stun_enemy(struct Map* map, int x, int y, int damage, struct MessageQueue* message_queue) {
    for (int i = 0; i < map->enemy_count; i++) {
        Enemy* enemy = &map->enemies[i];
        if (enemy->position.x == x && enemy->position.y == y) {
            enemy->active = false;  // Stunned, cannot move
            add_game_message(message_queue, "The enemy is stunned and cannot move!", 2);  // Green color
            break; // Exit the loop after stunning the enemy
        }
    }
}

void update_temporary_effects(Player* player, struct Map* map, struct MessageQueue* message_queue) {
    time_t current_time = time(NULL);

    // Handle temporary damage boost
    if (player->temporary_damage > 0 && difftime(current_time, player->temporary_damage_start_time) >= 30.0) {
        player->temporary_damage = 0;
        char message[100];
        snprintf(message, sizeof(message), "Temporary damage boost has worn off.");
        add_game_message(message_queue, message, 14); // Yellow color
    }

    // Handle temporary speed boost
    if (player->temporary_speed > 0 && difftime(current_time, player->temporary_speed_start_time) >= 30.0) {
        player->temporary_speed = 0;
        char message[100];
        snprintf(message, sizeof(message), "Temporary speed boost has worn off.");
        add_game_message(message_queue, message, 14); // Yellow color
    }
}

// Function to handle food consumption from inventory
void consume_food(Player* player, int food_index, struct MessageQueue* message_queue) {
    if (food_index < 0 || food_index >= player->food_count) return;
    Food* food = &player->foods[food_index];

    switch(food->type) {
        case FOOD_NORMAL:
            player->hitpoints += 10;
            player->hunger_rate -= 10;
            add_game_message(message_queue, "Consumed Normal Food.", 2);
            break;
        case FOOD_GREAT:
            player->hitpoints += 10;
            player->temporary_damage += 5;
            player->temporary_damage_start_time = time(NULL);
            add_game_message(message_queue, "Consumed Great Food: Weapon damage increased.", 2);
            break;
        case FOOD_MAGICAL:
            player->hitpoints += 10;
            // Instead of a separate `temporary_speed`, just reuse speed_spell_steps:
            player->speed_spell_steps = 10;
            add_game_message(message_queue,
                "Consumed Magical Food: Speed increased for the next 10 moves!",
                2);
            break;//That way, you unify the code for speed effect. Whenever you decrement speed_spell_steps in move_character(), it will apply equally to either a Speed Spell usage or Magical Food usage.
        case FOOD_ROTTEN:
            player->hitpoints -= 5;
            add_game_message(message_queue, "Consumed Rotten Food: HP decreased.", 7);
            break;
    }

    food->consumed = true;
}

// Function to collect food (add to inventory)
void collect_food(Player* player, struct Map* map, struct MessageQueue* message_queue) {
    struct Point pos = player->location;
    for (int i = 0; i < map->food_count; i++) {
        if (!map->foods[i].consumed && map->foods[i].position.x == pos.x && map->foods[i].position.y == pos.y) {
            if (player->food_count < MAX_FOOD_COUNT) {
                player->foods[player->food_count++] = map->foods[i];
                player->foods[player->food_count - 1].pickup_time = time(NULL);
                map->foods[i].consumed = true;
                map->grid[pos.y][pos.x] = FLOOR;
                add_game_message(message_queue, "Picked up food.", 2);
            } else {
                add_game_message(message_queue, "Food inventory full!", 7);
            }
            break;
        }
    }
}

// Finds the array index of the most recently picked (i.e., highest pickup_time)
// item of a given FoodType. Returns -1 if none is found.
int find_last_food_of_type(Player* player, FoodType type) {
    int index = -1;
    time_t latest_time = 0;

    for (int i = 0; i < player->food_count; i++) {
        Food* f = &player->foods[i];
        if (!f->consumed && f->type == type) {
            // Check if this item was picked up more recently
            if (f->pickup_time > latest_time) {
                latest_time = f->pickup_time;
                index = i;
            }
        }
    }

    return index;
}


void use_melee_weapon(Player* player, Weapon* weapon, struct Map* map, struct MessageQueue* msg_queue) {
    // Damages all adjacent tiles (8 directions)
    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            if (dx == 0 && dy == 0) continue;
            int tx = player->location.x + dx;
            int ty = player->location.y + dy;
            if (is_valid_tile(tx, ty)) {
                // Use weapon->damage plus any tempDamage
                int dmg = weapon->damage + player->temporary_damage;
                int base_damage = weapon->damage; 
                // If “temporary_damage” or Great Food is also used, add that too:
                base_damage += player->temporary_damage; 

                if (player->damage_spell_steps > 0) {
                    base_damage *= 2;  // double
                }
                // Then apply base_damage to the enemy
                deal_damage_to_enemy(map, tx, ty, dmg, msg_queue);
            }
        }
    }

    char msg[128];
    snprintf(msg, sizeof(msg), "You used your %s (melee)!", weapon->name);
    add_game_message(msg_queue, msg, 2);
}

void ask_ranged_direction(int* dx, int* dy, const char* weapon_name) {
    // Clear
    clear();
    mvprintw(0,0, "In which direction do you want to shoot your %s? (w/a/s/d): ", weapon_name);
    refresh();

    *dx = 0;
    *dy = 0;
    int ch = getch();
    switch (tolower(ch)) {
        case 'w': *dy = -1; break;
        case 's': *dy =  1; break;
        case 'a': *dx = -1; break;
        case 'd': *dx =  1; break;
        default:
            // If invalid, keep dx=dy=0
            break;
    }
}

void throw_ranged_weapon_with_drop(
    Player* player,
    Weapon* weapon,
    struct Map* map,
    struct MessageQueue* message_queue,
    int dx,
    int dy){
    // 1) Check ammo
    if (weapon->quantity <= 0) {
        add_game_message(message_queue, "No ammunition!", 7);
        return;
    }
    weapon->quantity--; // use 1 shot

    // 2) Setup
    int cx = player->location.x;
    int cy = player->location.y;

    int max_range = (weapon->symbol == WEAPON_MAGIC_WAND) ? 10 : 5;
    int last_floor_x = cx;
    int last_floor_y = cy;

    // 3) Travel loop
    for (int step = 0; step < max_range; step++) {
        cx += dx;
        cy += dy;

        // Out of bounds => stop
        if (!is_valid_tile(cx, cy)) break;

        // If it's a wall => destroy it, projectile is lost
        if (map->grid[cy][cx] == WALL_HORIZONTAL ||
            map->grid[cy][cx] == WALL_VERTICAL)
        {
            map->grid[cy][cx] = FLOOR;
            add_game_message(message_queue,
                             "Your projectile destroyed the wall and was lost.",
                             2);
            return;  // end
        }

        // If there's an enemy => deal damage, stop
        bool hitEnemy = false;
        for (int i = 0; i < map->enemy_count; i++) {
            Enemy* e = &map->enemies[i];
            if (e->position.x == cx && e->position.y == cy) {
                // Found an enemy
                int base_damage = weapon->damage + player->temporary_damage;
                if (player->damage_spell_steps > 0) {
                    base_damage *= 2;  // double if Damage Spell active
                }
                deal_damage_to_enemy(map, cx, cy, base_damage, message_queue);

                add_game_message(message_queue,
                    "Your projectile hit an enemy and stopped!",
                    2);
                hitEnemy = true;
                break;
            }
        }
        if (hitEnemy) return; // Stop traveling, no drop

        // Otherwise, if floor or corridor => keep track of last floor
        if (map->grid[cy][cx] == FLOOR ||
            map->grid[cy][cx] == CORRIDOR)
        {
            last_floor_x = cx;
            last_floor_y = cy;
        }
    }

    // 4) If we reach here => no wall/enemy was hit
    // => drop the projectile with quantity = 1
    if (last_floor_x != player->location.x || 
        last_floor_y != player->location.y)
    {
        map->grid[last_floor_y][last_floor_x] = weapon->symbol;
        add_game_message(message_queue,
            "Your projectile fell to the ground with 1 ammo remaining.",
            2);
    }
}
