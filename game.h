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

#define WALL_VERTICAL '|'
#define WALL_HORIZONTAL '-'
#define FLOOR '.'
#define DOOR '+'
#define CORRIDOR '#'
#define PILLAR 'O'
#define WINDOW '='
#define STAIRS '>'
#define FOG ' '
#define GOLD '$'
#define FOOD 'T'
#define TRAP_SYMBOL '^'

// Map constants
#define MAP_WIDTH 80
#define MAP_HEIGHT 24
#define MIN_ROOMS 6
#define MAX_ROOMS 10
#define MIN_ROOM_SIZE 4
#define MAX_ROOM_SIZE 8
#define MIN_ROOM_DISTANCE 2
#define CORRIDOR_VIEW_DISTANCE 5
#define MAX_DOORS 4
#define WINDOW_CHANCE 20
#define SIGHT_RANGE 5

// Utility macros
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

#define MAX_MESSAGES 5
#define MESSAGE_LENGTH 100

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
    struct GameMessage messages[5];
    int count;
};

struct SavedGame {
    struct Map game_map;
    struct Point character_location;
    int score;
    int current_level;  // Add this to track the level
    time_t save_time;
    char name[MAX_STRING_LEN];
};

// Game core functions
void play_game(struct UserManager* manager, struct Map* game_map, 
               struct Point* character_location, int initial_score);
void init_map(struct Map* map);
void create_corridors(Room* rooms, int room_count, char map[MAP_HEIGHT][MAP_WIDTH]);
void open_inventory_menu(int* food_inventory, int* food_count, int* gold_count, int* score, int* hunger_rate);
void add_traps(struct Map* game_map);
// Room and map generation
bool is_valid_room_placement(struct Map* map, struct Room* room);
void create_corridor_and_place_doors(struct Map* map, struct Point start, struct Point end);
void place_room(struct Map* map, struct Room* room);
void place_stairs(struct Map* map);
void place_pillars(struct Map* map, struct Room* room);
void place_windows(struct Map* map, struct Room* room);
void add_food(struct Map* map);
bool validate_stair_placement(struct Map* map);
void save_current_game(struct UserManager* manager, struct Map* game_map, 
                      struct Point* character_location, int score, int current_level);
bool load_saved_game(struct UserManager* manager, struct SavedGame* saved_game);
void list_saved_games(struct UserManager* manager);
void connect_rooms_with_corridors(struct Map* map);
void add_gold(struct Map* game_map);


// Movement and visibility
void move_character(struct Point* character_location, int key, struct Map* game_map);
void update_visibility(struct Map* map, struct Point* player_pos, bool visible[MAP_HEIGHT][MAP_WIDTH]);
void sight_range(struct Map* game_map, struct Point* character_location);

// Room connectivity
bool canSeeRoomThroughWindow(struct Map* game_map, struct Room* room1, struct Room* room2);

// Helper functions
int manhattanDistance(int x1, int y1, int x2, int y2);
bool isPointInRoom(struct Point* point, struct Room* room);
void showRoom(struct Map* game_map, struct Room* room, char visible[MAP_HEIGHT][MAP_WIDTH]);
bool can_see_room(struct Map* map, struct Room* room1, struct Room* room2);
void print_point(struct Point p, const char* type);

// Message system
void add_game_message(struct MessageQueue* queue, const char* text, int color_pair);
void update_messages(struct MessageQueue* queue);
void draw_messages(struct MessageQueue* queue, int start_y, int start_x);

// Map display
void print_map(struct Map* game_map, bool visible[MAP_HEIGHT][MAP_WIDTH], struct Point character_location);
struct Map generate_map(struct Room* previous_room);
#endif