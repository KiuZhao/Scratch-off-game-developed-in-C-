#include "game.h"

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    Game game;
    if (!game.init()) {
        printf("Failed to initialize game.\n");
        return 1;
    }

    game.run();
    return 0;
}
