#include <ncurses.h>
#include <stdlib.h>
#include <unistd.h>

#define MAX_LEN 100

int DELAY = 100000;
typedef struct
{
    int x, y;
} Point;

typedef struct
{
    Point body[MAX_LEN];
    int length;
    int dir;
} Snake;

typedef struct
{
    Point pos;
    int score;
} Game;

Snake snake;
Game game;

void init_game();
void end_game();
void draw_snake();
void move_snake();
void handle_input();
void spawn_food();
int check_collision();
void draw_border();
void draw_food();
void draw_score();

int main()
{
    initscr();
    curs_set(FALSE);
    keypad(stdscr, TRUE);
    noecho();
    timeout(0);

    if (has_colors())
    {
        start_color();
        init_pair(1, COLOR_GREEN, COLOR_BLACK);
        init_pair(2, COLOR_RED, COLOR_BLACK);
        init_pair(3, COLOR_WHITE, COLOR_BLACK);
    }

    init_game();

    while (1)
    {
        handle_input();
        move_snake();

        if (check_collision())
        {
            end_game();
            break;
        }

        clear();
        draw_border();
        draw_food();
        draw_snake();
        draw_score();

        refresh();
        usleep(DELAY);
    }

    endwin();
    return 0;
}

void init_game()
{
    snake.length = 5;
    snake.dir = KEY_RIGHT;
    for (int i = 0; i < snake.length; i++)
    {
        snake.body[i].x = 10 - i;
        snake.body[i].y = 5;
    }

    game.score = 0;
    spawn_food();
}

void end_game()
{
    clear();
    attron(COLOR_PAIR(3));
    mvprintw(LINES / 2, COLS / 2 - 5, "Game Over!");
    mvprintw(LINES / 2 + 1, COLS / 2 - 10, "Score: %d", game.score);
    mvprintw(LINES / 2 + 2, COLS / 2 - 10, "Press any key to exit...");
    attroff(COLOR_PAIR(3));
    refresh();
    sleep(2);
    timeout(-1);
    getch();
}

void draw_snake()
{
    attron(COLOR_PAIR(1));
    for (int i = 0; i < snake.length; i++)
    {
        mvprintw(snake.body[i].y, snake.body[i].x, "X");
    }
    attroff(COLOR_PAIR(1));
}

void move_snake()
{
    for (int i = snake.length - 1; i > 0; i--)
    {
        snake.body[i] = snake.body[i - 1];
    }

    switch (snake.dir)
    {
    case KEY_UP:
        snake.body[0].y--;
        break;
    case KEY_DOWN:
        snake.body[0].y++;
        break;
    case KEY_LEFT:
        snake.body[0].x--;
        break;
    case KEY_RIGHT:
        snake.body[0].x++;
        break;
    }

    if (snake.body[0].x == game.pos.x && snake.body[0].y == game.pos.y)
    {
        snake.length++;
        if (snake.length > MAX_LEN)
            snake.length = MAX_LEN;
        game.score += 10;
        spawn_food();
    }
}

void handle_input()
{
    int ch = getch();
    if (ch != ERR)
    {
        if ((ch == KEY_UP && snake.dir != KEY_DOWN) ||
            (ch == KEY_DOWN && snake.dir != KEY_UP) ||
            (ch == KEY_LEFT && snake.dir != KEY_RIGHT) ||
            (ch == KEY_RIGHT && snake.dir != KEY_LEFT))
        {
            snake.dir = ch;
        }
        else if(ch == '-')
            DELAY *= 1.1;
        else if(ch == '+')
            DELAY /= 1.1;
    }
}

void spawn_food()
{
    int valid = 0;
    while (!valid)
    {
        game.pos.x = rand() % (COLS - 2) + 1;
        game.pos.y = rand() % (LINES - 2) + 1;
        valid = 1;
        for (int i = 0; i < snake.length; i++)
        {
            if (snake.body[i].x == game.pos.x && snake.body[i].y == game.pos.y)
            {
                valid = 0;
                break;
            }
        }
    }
}

int check_collision()
{
    if (snake.body[0].x <= 0 || snake.body[0].x >= COLS - 1 ||
        snake.body[0].y <= 0 || snake.body[0].y >= LINES - 1)
    {
        return 1;
    }

    for (int i = 1; i < snake.length; i++)
    {
        if (snake.body[0].x == snake.body[i].x && snake.body[0].y == snake.body[i].y)
        {
            return 1;
        }
    }

    return 0;
}

void draw_border()
{
    attron(COLOR_PAIR(3));
    for (int i = 0; i < COLS; i++)
    {
        mvprintw(0, i, "#");
        mvprintw(LINES - 1, i, "#");
    }
    for (int i = 0; i < LINES; i++)
    {
        mvprintw(i, 0, "#");
        mvprintw(i, COLS - 1, "#");
    }
    attroff(COLOR_PAIR(3));
}

void draw_food()
{
    attron(COLOR_PAIR(2));
    mvprintw(game.pos.y, game.pos.x, "O");
    attroff(COLOR_PAIR(2));
}

void draw_score()
{
    attron(COLOR_PAIR(3));
    mvprintw(0, 2, " Score: %d ", game.score);
    attroff(COLOR_PAIR(3));
}
