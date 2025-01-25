#ifndef GAME_H
#define GAME_H

#include <stdbool.h>
#include <stdint.h>
#include <ncurses.h>
#include <time.h>
#include "users.h"
#include "menu.h"

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


// -- Secret doors --
#define SECRET_DOOR_CLOSED  '?'  // Hidden door tile
#define SECRET_DOOR_REVEALED '/' // Revealed door tile
#define DOOR_PASSWORD '@'
#define PASSWORD_GEN  '&'
// Just store 'K' for "Key" in the grid
#define ANCIENT_KEY 'K' //it will be printed as ▲



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

typedef struct Trap {
    struct Point location; // Position of the trap
    bool triggered;        // Whether the trap has been triggered
} Trap;


typedef struct Room {
    int x, y;
    int left_wall;
    int right_wall;
    int top_wall;     // Previously up_wall
    int bottom_wall;  // Previously down_wall
    int width;
    int height;
    struct Point doors[MAX_DOORS];
    int door_count;
    bool has_stairs;
    bool visited;
        
    // --- Password door info ---
    bool has_password_door;         // Does this room have a '@' door?
    bool password_unlocked;         // Is the door currently unlocked?
    bool password_active;           // Is there a valid password now?
    time_t password_gen_time;       // When the password was generated
    char door_code[6];            // 4-digit (zero-padded) code, plus 1 for the null terminator
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
};


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
void move_character(struct Point* character_location, int key,
                    struct Map* game_map, int* hitpoints);
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
void print_map(struct Map* game_map, bool visible[MAP_HEIGHT][MAP_WIDTH], struct Point character_location);
struct Map generate_map(struct Room* previous_room);

#endif
