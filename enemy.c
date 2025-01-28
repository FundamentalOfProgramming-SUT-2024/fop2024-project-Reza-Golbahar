#include "game.h"
#include "enemy.h"


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
            add_game_message(map->enemy_count > 0 ? &((struct MessageQueue){0}) : NULL, 
                           "An enemy attacks you! HP decreased.", 4);
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