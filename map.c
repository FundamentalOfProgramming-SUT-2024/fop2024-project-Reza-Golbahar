#include <ncurses.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <limits.h>

#define VERTICAL '|'
#define HORIZ '_'
#define DOOR '+'
#define FLOOR '.'
#define PILLAR 'O'

#define MIN_ROOM_SIZE 4
#define MAX_ROOM_SIZE 16
#define MAX_ROOMS 12

#define MAP_WIDTH 160
#define MAP_HEIGHT 40

typedef struct {
    int x;
    int y;
} location;

typedef struct {
    int x;
    int y;
    int width;
    int height;
} Room;

char map[MAP_HEIGHT][MAP_WIDTH];
location spawn;
int i = 0;
location doors[MAX_ROOMS * 2];

void init_map() {
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            map[y][x] = ' ';
        }
    }
}

bool check_room_overlap(Room new_room, Room* rooms, int room_count) {
    const int BUFFER = 2;

    for (int i = 0; i < room_count; i++) {
        if (new_room.x - BUFFER < rooms[i].x + rooms[i].width + BUFFER &&
            new_room.x + new_room.width + BUFFER > rooms[i].x - BUFFER &&
            new_room.y - BUFFER < rooms[i].y + rooms[i].height + BUFFER &&
            new_room.y + new_room.height + BUFFER > rooms[i].y - BUFFER) {
            return true;
        }
    }

    if (new_room.x - BUFFER < 2 || new_room.x + new_room.width + BUFFER > MAP_WIDTH - 2 ||
        new_room.y - BUFFER < 2 || new_room.y + new_room.height + BUFFER > MAP_HEIGHT - 2) {
        return true;
    }

    return false;
}

void draw_room(Room room) {
    for (int y = room.y + 1; y < room.y + room.height; y++) {
        for (int x = room.x + 1; x < room.x + room.width; x++) {
            map[y][x] = FLOOR;
        }
    }

    for (int x = room.x; x <= room.x + room.width; x++) {
        map[room.y][x] = HORIZ;
        map[room.y + room.height][x] = HORIZ;
    }

    for (int y = room.y; y <= room.y + room.height; y++) {
        map[y][room.x] = VERTICAL;
        map[y][room.x + room.width] = VERTICAL;
    }
}

void create_corridors(Room* rooms, int room_count) {
    bool connected[MAX_ROOMS] = { false };
    connected[0] = true;


    for (int count = 1; count < room_count; count++) {
        int best_src = -1;
        int best_dst = -1;
        int min_distance = INT_MAX;

        // Find the closest connection between a connected and unconnected room
        for (int i = 0; i < room_count; i++) {
            if (!connected[i]) continue;
            
            for (int j = 0; j < room_count; j++) {
                if (connected[j]) continue;
                
                int distance = abs(rooms[i].x - rooms[j].x) + abs(rooms[i].y - rooms[j].y);
                if (distance < min_distance) {
                    min_distance = distance;
                    best_src = i;
                    best_dst = j;
                }
            }
        }

        if (best_src != -1 && best_dst != -1) {
            connected[best_dst] = true;

            // Draw corridor between rooms
            int start_x = rooms[best_src].x + rooms[best_src].width / 2;
            int start_y = rooms[best_src].y + rooms[best_src].height / 2;
            int end_x = rooms[best_dst].x + rooms[best_dst].width / 2;
            int end_y = rooms[best_dst].y + rooms[best_dst].height / 2;

            // Draw L-shaped corridor
            if (rand() % 2 == 0) {
                // Horizontal first, then vertical
                int current_x = start_x;
                while (current_x != end_x) {
                    if (map[start_y][current_x] == ' ') {
                        map[start_y][current_x] = '=';
                    }
                    current_x += (current_x < end_x) ? 1 : -1;
                }
                
                int current_y = start_y;
                while (current_y != end_y) {
                    if (map[current_y][end_x] == ' ') {
                        map[current_y][end_x] = '=';
                    }
                    current_y += (current_y < end_y) ? 1 : -1;
                }
            } else {
                // Vertical first, then horizontal
                int current_y = start_y;
                while (current_y != end_y) {
                    if (map[current_y][start_x] == ' ') {
                        map[current_y][start_x] = '=';
                    }
                    current_y += (current_y < end_y) ? 1 : -1;
                }
                
                int current_x = start_x;
                while (current_x != end_x) {
                    if (map[end_y][current_x] == ' ') {
                        map[end_y][current_x] = '=';
                    }
                    current_x += (current_x < end_x) ? 1 : -1;
                }
            }
        }
    }
}

void place_doors() {
    for (int y = 1; y < MAP_HEIGHT - 1; y++) {
        for (int x = 1; x < MAP_WIDTH - 1; x++) {
            if (map[y][x] == '=') {
                bool is_corner = false;

                if ((map[y-1][x-1] == HORIZ && map[y][x-1] == VERTICAL) ||
                    (map[y-1][x+1] == HORIZ && map[y][x+1] == VERTICAL) ||
                    (map[y+1][x-1] == HORIZ && map[y][x-1] == VERTICAL) ||
                    (map[y+1][x+1] == HORIZ && map[y][x+1] == VERTICAL)) {
                    is_corner = true;
                }

                if (!is_corner) {
                    if (map[y][x-1] == VERTICAL) {
                        map[y][x-1] = DOOR;
                        if (map[y][x-2] == HORIZ)
                            map[y][x-2] = DOOR;
                        doors[i].x = x-1;
                        doors[i].y = y;
                        i++;
                    } else if (map[y][x+1] == VERTICAL) {
                        map[y][x+1] = DOOR;
                        if (map[y][x+2] == HORIZ)
                            map[y][x+2] = DOOR;
                        doors[i].x = x+1;
                        doors[i].y = y;
                        i++;
                    } else if (map[y-1][x] == HORIZ || map[y-1][x] == VERTICAL) {
                        map[y-1][x] = DOOR;
                        if (map[y-2][x] == VERTICAL)
                            map[y-2][x] = DOOR;
                        doors[i].x = x;
                        doors[i].y = y-1;
                        i++;
                    } else if (map[y+1][x] == HORIZ || map[y+1][x] == VERTICAL) {
                        map[y+1][x] = DOOR;
                        if (map[y+2][x] == VERTICAL)
                            map[y+2][x] = DOOR;
                        doors[i].x = x;
                        doors[i].y = y+1;
                        i++;
                    }
                }
            }
        }
    }
}

void place_pillars(Room room) {
    // Calculate maximum number of pillars based on room size
    int max_pillars = (room.width * room.height) / 20; // Adjust this ratio as needed
    
    // Place random number of pillars (1 to max_pillars)
    int num_pillars = 1 + (rand() % max_pillars);
    
    for (int i = 0; i < num_pillars; i++) {
        int attempts = 0;
        while (attempts < 10) { // Limit attempts to avoid infinite loops
            // Generate random position inside the room (excluding walls)
            int x = room.x + 1 + (rand() % (room.width - 1));
            int y = room.y + 1 + (rand() % (room.height - 1));
            
            // Check if the position is suitable (floor tile and not adjacent to doors)
            if (map[y][x] == FLOOR) {
                // Check surrounding tiles for doors
                bool near_door = false;
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        if (map[y + dy][x + dx] == DOOR || 
                            map[y + dy][x + dx] == '=') {
                            near_door = true;
                            break;
                        }
                    }
                    if (near_door) break;
                }
                
                // If not near a door, place the pillar
                if (!near_door) {
                    map[y][x] = PILLAR;
                    break;
                }
            }
            attempts++;
        }
    }
}

void generate_rooms() {
    Room rooms[MAX_ROOMS];
    int room_count = 0;
    i = 0;
    int attempts = 0;
    const int MAX_ATTEMPTS = 1000;
    const int MIN_ROOMS = 6;
    
    // First, generate the minimum required rooms (6)
    while (room_count < MIN_ROOMS && attempts < MAX_ATTEMPTS) {
        Room new_room;
        new_room.width = MIN_ROOM_SIZE + (rand() % (MAX_ROOM_SIZE - MIN_ROOM_SIZE));
        new_room.height = MIN_ROOM_SIZE + (rand() % (MAX_ROOM_SIZE - MIN_ROOM_SIZE));

        new_room.x = 3 + (rand() % (MAP_WIDTH - new_room.width - 6));
        new_room.y = 3 + (rand() % (MAP_HEIGHT - new_room.height - 6));

        if (!check_room_overlap(new_room, rooms, room_count)) {
        rooms[room_count] = new_room;
        draw_room(new_room);
        place_pillars(new_room);  // Add this line
        room_count++;
    }
        attempts++;
    }

    // Try to add additional rooms
    attempts = 0;
    while (room_count < MAX_ROOMS && attempts < MAX_ATTEMPTS/2) {
        // 40% chance to try adding another room
        if (rand() % 100 < 40) {
            Room new_room;
            new_room.width = MIN_ROOM_SIZE + (rand() % (MAX_ROOM_SIZE - MIN_ROOM_SIZE));
            new_room.height = MIN_ROOM_SIZE + (rand() % (MAX_ROOM_SIZE - MIN_ROOM_SIZE));

            new_room.x = 3 + (rand() % (MAP_WIDTH - new_room.width - 6));
            new_room.y = 3 + (rand() % (MAP_HEIGHT - new_room.height - 6));

            if (!check_room_overlap(new_room, rooms, room_count)) {
                rooms[room_count] = new_room;
                draw_room(new_room);
                room_count++;
            }
        }
        attempts++;
    }

    // If we couldn't generate minimum rooms, print error and exit
    if (room_count < MIN_ROOMS) {
        endwin();
        fprintf(stderr, "Error: Could not generate minimum number of rooms\n");
        exit(1);
    }

    create_corridors(rooms, room_count);
    place_doors();
}

void draw_borders() {
    for (int i = 1; i < MAP_WIDTH; i++) {
        if (i % 2 == 1) {
            mvprintw(0, i, "/");
            mvprintw(MAP_HEIGHT - 1, i, "\\");
        } else {
            mvprintw(0, i, "\\");
            mvprintw(MAP_HEIGHT - 1, i, "/");
        }
    }
    for (int i = 1; i < MAP_HEIGHT; i++) {
        mvprintw(i, 1, "<");
        mvprintw(i, MAP_WIDTH - 1, ">");
    }
}

void draw_map() {
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            if (map[y][x] != ' ') {
                mvprintw(y, x, "%c", map[y][x]);
            }
        }
    }
}

int main() {
    initscr();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    srand(time(NULL));

    init_map();
    generate_rooms();

    draw_borders();
    draw_map();
    location player = {doors[0].x, doors[0].y};
    mvprintw(player.y, player.x, "@");
    refresh();

    int ch;
    while ((ch = getch()) != 'q') {
        mvprintw(player.y, player.x, " ");

switch (ch) {
    case KEY_UP:
        if (player.y > 1 && map[player.y-1][player.x] != VERTICAL &&
            map[player.y-1][player.x] != HORIZ && map[player.y-1][player.x] != PILLAR &&
            (map[player.y-1][player.x] == FLOOR || map[player.y-1][player.x] == DOOR ||
             map[player.y-1][player.x] == '='))
            player.y--;
        break;
    case KEY_DOWN:
        if (player.y < MAP_HEIGHT - 2 && map[player.y+1][player.x] != VERTICAL &&
            map[player.y+1][player.x] != HORIZ && map[player.y+1][player.x] != PILLAR &&
            (map[player.y+1][player.x] == FLOOR || map[player.y+1][player.x] == DOOR ||
             map[player.y+1][player.x] == '='))
            player.y++;
        break;
    case KEY_LEFT:
        if (player.x > 1 && map[player.y][player.x-1] != VERTICAL &&
            map[player.y][player.x-1] != HORIZ && map[player.y][player.x-1] != PILLAR &&
            (map[player.y][player.x-1] == FLOOR || map[player.y][player.x-1] == DOOR ||
             map[player.y][player.x-1] == '='))
            player.x--;
        break;
    case KEY_RIGHT:
        if (player.x < MAP_WIDTH - 2 && map[player.y][player.x+1] != VERTICAL &&
            map[player.y][player.x+1] != HORIZ && map[player.y][player.x+1] != PILLAR &&
            (map[player.y][player.x+1] == FLOOR || map[player.y][player.x+1] == DOOR ||
             map[player.y][player.x+1] == '='))
            player.x++;
        break;
}

        draw_borders();
        draw_map();
        mvprintw(player.y, player.x, "@");
        refresh();
    }

    endwin();
    return 0;
}
