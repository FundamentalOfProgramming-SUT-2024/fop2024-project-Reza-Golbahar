all: game

game:
    gcc main.c game.c -o game -lncurses -Wall -Wextra

clean:
    rm -f game

//gcc main.c game.c -o game -lncurses -Wall -Wextra