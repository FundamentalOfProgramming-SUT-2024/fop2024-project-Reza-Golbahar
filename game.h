#ifndef GAME_H
#define GAME_H

#include <stdbool.h>
#include <stdint.h>
#include <ncurses.h>
#include <time.h>
#include "users.h"
#include "menu.h"

// Probability thresholds (adjust as needed)
#define PROB_TREASURE_ROOM 20 // 20% chance
#define PROB_ENCHANT_ROOM  10 // 10% chance


#define MAX_ROOM_CONNECTION_DISTANCE 20  // Maximum allowable distance between room centers

// Map symbols
#define WALL_VERTICAL       '|'
#define WALL_HORIZONTAL     '-'
#define FLOOR               '.'
#define DOOR                '+'
#define CORRIDOR            '#'
#define PILLAR              'O'
#define WINDOW              '='
#define STAIRS              '>'
#define FOG                 ' '
#define GOLD                '$'
#define FOOD                'T'
#define TRAP_SYMBOL         '^'
#define PLAYER_CHAR         'P'

#define COLOR_PAIR_ROOM_NORMAL    13
#define COLOR_PAIR_ROOM_ENCHANT   14
#define COLOR_PAIR_ROOM_TREASURE  15

#define COLOR_PAIR_DAMAGE 10
#define COLOR_PAIR_SPEED 11
#define COLOR_PAIR_HEALTH 12

#define ENEMY_FIRE_MONSTER 'F'
#define MAX_ENEMIES 20           // Maximum number of enemies in the game
#define ENEMY_HP_FIRE_MONSTER 10 // HP for Fire Breathing Monster


// -- Secret doors --
#define SECRET_DOOR_CLOSED '/'    // Placeholder for secret doors in the grid
#define SECRET_DOOR_REVEALED '?'   // Symbol to display when revealed

#define DOOR_PASSWORD '@'
#define PASSWORD_GEN  '&'

// Just store 'K' for "Key" in the grid
#define ANCIENT_KEY 'K' //it will be printed as ▲

// Weapon symbols
#define WEAPON_MACE         '1'
#define WEAPON_DAGGER       '2'
#define WEAPON_MAGIC_WAND   '3'
#define WEAPON_ARROW        '4'
#define WEAPON_SWORD        '5'

// Spell Symbols
#define SPELL_HEALTH '6'
#define SPELL_SPEED  '7'
#define SPELL_DAMAGE '8'

#define TREASURE_CHEST 'T'

// Maximum number of weapons a player can carry
#define MAX_WEAPONS         10
#define MAX_SPELLS 10


// Map constants
#define MAP_WIDTH           80
#define MAP_HEIGHT          24
#define MIN_ROOMS           6
#define MAX_ROOMS           10
#define MIN_ROOM_SIZE       4
#define MAX_ROOM_SIZE       8
#define MIN_ROOM_DISTANCE   2
#define CORRIDOR_VIEW_DISTANCE 5
#define MAX_DOORS           4
#define WINDOW_CHANCE       20
#define SIGHT_RANGE         5

// Utility macros
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

#define MAX_MESSAGES        5
#define MESSAGE_LENGTH      100


struct Point {
    int x;
    int y;
};

// Enemy Types
typedef enum {
    ENEMY_FIRE_BREATHING_MONSTER,
    ENEMY_DEMON,
    ENEMY_GIANT,
    ENEMY_SNAKE,
    ENEMY_UNDEAD,
    // Future enemy types can be added here
} EnemyType;


typedef struct Enemy {
    EnemyType type;         // Type of the enemy
    struct Point position;  // Current position on the map
    int hp;                 // Hit Points
    bool active;            // Whether the enemy is active (chasing the player)
    bool chasing;           // Flag to indicate if the enemy is chasing the player
    bool adjacent_attack;   // Flag to check if the player is adjacent to the enemy
} Enemy;

typedef struct Trap {
    struct Point location; // Position of the trap
    bool triggered;        // Whether the trap has been triggered
} Trap;

// Define Room Themes
typedef enum {
    THEME_NORMAL,
    THEME_TREASURE,
    THEME_ENCHANT,
    THEME_UNKNOWN // For future extensions
} RoomTheme;

typedef struct Door {
    int x;
    int y;
} Door;

typedef struct Room {
    int x, y;
    int left_wall;
    int right_wall;
    int top_wall;     // Previously up_wall
    int bottom_wall;  // Previously down_wall
    int width;
    int height;
    int door_count;
    bool has_stairs;
    bool visited;
        
    // --- Password door info ---
    bool has_password_door;         // Does this room have a '@' door?
    bool password_unlocked;         // Is the door currently unlocked?
    bool password_active;           // Is there a valid password now?
    time_t password_gen_time;       // When the password was generated
    char door_code[6];            // 4-digit (zero-padded) code, plus 1 for the null terminator
    RoomTheme theme; // New field for room theme    
    Door doors[MAX_DOORS]; // Assuming a Door structure exists
} Room;

struct Map {
    char grid[MAP_HEIGHT][MAP_WIDTH];
    bool visibility[MAP_HEIGHT][MAP_WIDTH];
    bool discovered[MAP_HEIGHT][MAP_WIDTH];
    struct Room rooms[MAX_ROOMS];
    int room_count;

    Trap traps[100]; // Array of traps
    int trap_count;  // Number of traps

    struct Point stairs_location;
    struct Point initial_position;

    // Enemies
    Enemy enemies[MAX_ENEMIES];
    int enemy_count;
};

// Weapon structure
typedef struct Weapon {
    char symbol;            // '1' to '5'
    char name[20];          // Name of the weapon
    int damage;             // Damage value
    int quantity;           // Quantity of weapon, for throwable or reusable ones
} Weapon;

// Spell Types
typedef enum {
    SPELL_HEALTH_TYPE,
    SPELL_SPEED_TYPE,
    SPELL_DAMAGE_TYPE,
    SPELL_UNKNOWN_TYPE
} SpellType;

// Spell Structure
typedef struct Spell {
    SpellType type;
    char symbol;        // Symbol representing the spell on the map
    char name[20];      // Name of the spell
    int effect_value;   // Value associated with the spell (e.g., heal amount, speed boost)
} Spell;

// Player structure update to include weapon inventory
typedef struct Player {
    struct Point location;
    int hitpoints;
    int hunger_rate;
    int score;
    
    // Weapon inventory
    Weapon weapons[MAX_WEAPONS];
    int weapon_count;
    
    // Currently equipped weapon index (-1 if none)
    int equipped_weapon;

    // Key counts
    int ancient_key_count;
    int broken_key_count;

    Spell spells[MAX_SPELLS];
    int spell_count;
    
    // ... [other player attributes]
} Player;


struct GameMessage {
    char text[100];
    int time_to_live;
    int color_pair;
};

struct MessageQueue {
    struct GameMessage messages[MAX_MESSAGES];
    int count;
};

struct SavedGame {
    struct Map game_map;
    struct Point character_location;
    int score;
    int current_level;  // Track the current level
    time_t save_time;
    char name[MAX_STRING_LEN];
};


static bool code_visible = false;        // Is there a code currently on screen?
static time_t code_start_time = 0;           // When was it generated?
static char current_code[6] = "";    // Holds the 4-digit code
extern bool hasPassword;  // Just a declaration, no assignment

// If you only have one password door at a time, you could do:
static bool door_unlocked = false;



// Game core functions
void play_game(struct UserManager* manager, struct Map* game_map, 
               struct Point* character_location, int initial_score);
void init_map(struct Map* map);
void create_corridors(Room* rooms, int room_count, char map[MAP_HEIGHT][MAP_WIDTH]);
void open_inventory_menu(int* food_inventory, int* food_count, int* gold_count,
                         int* score, int* hunger_rate,
                         int* ancient_key_count, int* broken_key_count);
void add_traps(struct Map* game_map);
void print_password_messages(const char* message, int line_offset);
Room* find_room_by_position(struct Map* map, int x, int y);

// Room and map generation
bool is_valid_room_placement(struct Map* map, struct Room* room);
void create_corridor_and_place_doors(struct Map* map, struct Point start, struct Point end);
void place_room(struct Map* map, struct Room* room);
void place_stairs(struct Map* map);
void place_pillars(struct Map* map, struct Room* room);
void place_windows(struct Map* map, struct Room* room);
void add_food(struct Map* map);
bool validate_stair_placement(struct Map* map);
bool prompt_for_password_door(Room* door_room);
void place_secret_doors(struct Map* map);
void print_full_map(struct Map* game_map, struct Point* character_location, struct UserManager* manager);

// Saving/Loading
void save_current_game(struct UserManager* manager, struct Map* game_map, 
                      struct Point* character_location, int score, int current_level);
bool load_saved_game(struct UserManager* manager, struct SavedGame* saved_game);
void list_saved_games(struct UserManager* manager);

// Room connectivity
void connect_rooms_with_corridors(struct Map* map);
void add_gold(struct Map* game_map);
bool canSeeRoomThroughWindow(struct Map* game_map, struct Room* room1, struct Room* room2);

// Movement and visibility
// game.h
void move_character(Player* player, int key, struct Map* game_map, int* hitpoints, struct MessageQueue* message_queue);
void place_password_generator_in_corner(struct Map* map, struct Room* room);
void update_visibility(struct Map* map, struct Point* player_pos, bool visible[MAP_HEIGHT][MAP_WIDTH]);
void sight_range(struct Map* game_map, struct Point* character_location);
void add_ancient_key(struct Map* game_map);

// Helper functions
int manhattanDistance(int x1, int y1, int x2, int y2);
bool isPointInRoom(struct Point* point, struct Room* room);
void showRoom(struct Map* game_map, struct Room* room, char visible[MAP_HEIGHT][MAP_WIDTH]);
bool can_see_room(struct Map* map, struct Room* room1, struct Room* room2);
void print_point(struct Point p, const char* type);
void place_password_generator_in_corner(struct Map* game_map, struct Room* room);

// Message system
void add_game_message(struct MessageQueue* queue, const char* text, int color_pair);
void update_messages(struct MessageQueue* queue);
void draw_messages(struct MessageQueue* queue, int start_y, int start_x);
void update_password_display();

// Map display
void print_map(struct Map* game_map, bool visible[MAP_HEIGHT][MAP_WIDTH], struct Point character_location, struct UserManager* manager);
struct Map generate_map(struct Room* previous_room, int current_level, int max_level) ;

// Function declarations for weapons
void add_weapons(struct Map* map);
void handle_weapon_pickup(Player* player, struct Map* map, struct Point new_location, struct MessageQueue* message_queue);
void open_weapon_inventory_menu(Player* player, struct Map* map, struct MessageQueue* message_queue);
const char* symbol_to_name(char symbol);
Weapon create_weapon(char symbol);
//void display_weapons_inventory(Player* player);
void equip_weapon(Player* player, int weapon_index);
void use_weapon(Player* player, struct Map* map, struct MessageQueue* message_queue);

// Function Declarations
Spell create_spell(char symbol);
void handle_spell_pickup(Player* player, struct Map* map, struct Point new_location, struct MessageQueue* message_queue);
void open_spell_inventory_menu(struct Map* game_map, Player* player);
void use_spell(Player* player, struct Map* game_map);
const char* spell_type_to_name(SpellType type);
void add_spells(struct Map* game_map);

// Function Declarations for Enemies
void add_enemies(struct Map* map, int current_level);
void render_enemies(struct Map* map);
void update_enemies(struct Map* map, Player* player, int* hitpoints, struct MessageQueue* message_queue);
void move_enemy_towards(Player* player, Enemy* enemy, struct Map* map, struct MessageQueue* message_queue);
bool is_enemy_in_same_room(Player* player, Enemy* enemy, struct Map* map);
void combat(Player* player, Enemy* enemy, struct Map* map, int* hitpoints);

void throw_arrow(Player* player, Weapon* weapon, struct Map* map, struct MessageQueue* message_queue);
void stun_enemy(struct Map* map, int x, int y, int damage, struct MessageQueue* message_queue);
void throw_magic_wand(Player* player, Weapon* weapon, struct Map* map, struct MessageQueue* message_queue);
void throw_dagger(Player* player, Weapon* weapon, struct Map* map, struct MessageQueue* message_queue);
void deal_damage_to_enemy(struct Map* map, int x, int y, int damage, struct MessageQueue* message_queue);
bool is_valid_tile(int x, int y);
bool is_adjacent(struct Point p1, struct Point p2);

#endif
