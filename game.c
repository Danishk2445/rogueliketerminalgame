#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <stdbool.h>
#include <sys/time.h>

// constants
#define MAP_WIDTH 40
#define MAP_HEIGHT 20
#define NUM_ENEMIES 10
#define WALL_DENSITY 30
#define MAX_PROJECTILES 100

// character representations
#define PLAYER_CHAR '@'
#define ENEMY_CHAR 'E'
#define WALL_CHAR '#'
#define FLOOR_CHAR '.'
#define ARROW_CHAR '*'
#define ENEMY_ARROW_CHAR '+'

// color pair constants
#define COLOR_PLAYER 1
#define COLOR_ENEMY 2
#define COLOR_WALL 3
#define COLOR_FLOOR 4
#define COLOR_PROJECTILE 5
#define COLOR_ENEMY_PROJECTILE 6
#define COLOR_STATUS 7
#define COLOR_WIN_MESSAGE 8

// structs to represent game entities
typedef struct {
    float x;
    float y;
    char character;
} Entity;

typedef struct {
    Entity base;
    int health;
    float speed;
    int ammo;
} Player;

typedef struct {
    Entity base;
    int health;
    float speed;
    double last_shot_time;
    float shoot_delay;
} Enemy;

typedef struct {
    Entity base;
    float dx;
    float dy;
    float speed;
    bool is_enemy;
} Projectile;

typedef struct {
    WINDOW *stdscr;
    char map[MAP_HEIGHT][MAP_WIDTH];
    Player player;
    Enemy enemies[NUM_ENEMIES];
    int num_enemies;
    Projectile projectiles[MAX_PROJECTILES];
    int projectile_count;
    double last_update;
    bool game_over;
} Game;

// function prototypes
void setup_colors();
void generate_map(Game *game);
void spawn_player(Game *game);
void spawn_enemies(Game *game);
void move_entity(Game *game, Entity *entity, float dx, float dy);
void handle_input(Game *game);
void shoot(Game *game, float x, float y, float dx, float dy, bool is_enemy);
void update_enemies(Game *game, float delta_time);
void update_projectiles(Game *game, float delta_time);
void render(Game *game);
void run_game(Game *game);
void init_game(Game *game, WINDOW *stdscr);
double get_time();

// get current time in seconds
double get_time() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

void setup_colors() {
    start_color();
    use_default_colors();

    init_pair(COLOR_PLAYER, COLOR_GREEN, -1);    
    init_pair(COLOR_ENEMY, COLOR_RED, -1);
    init_pair(COLOR_WALL, COLOR_CYAN, -1);
    init_pair(COLOR_FLOOR, COLOR_BLACK, -1);
    init_pair(COLOR_PROJECTILE, COLOR_YELLOW, -1);
    init_pair(COLOR_ENEMY_PROJECTILE, COLOR_MAGENTA, -1);
    init_pair(COLOR_STATUS, COLOR_GREEN, -1);
    init_pair(COLOR_WIN_MESSAGE, COLOR_GREEN, -1);
}

void generate_map(Game *game) {
    // initialize map with floor tiles
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            game->map[y][x] = FLOOR_CHAR;
        }
    }

    // create border walls
    for (int x = 0; x < MAP_WIDTH; x++) {
        game->map[0][x] = WALL_CHAR; // top border
        game->map[MAP_HEIGHT-1][x] = WALL_CHAR; // top bottom
    }

    for (int y = 0; y < MAP_HEIGHT; y++) {
        game->map[y][0] = WALL_CHAR; // left border
        game->map[y][MAP_WIDTH-1] = WALL_CHAR; // right border
    }

    // random walls in arena
    for (int i = 0; i < (MAP_WIDTH * MAP_HEIGHT / WALL_DENSITY); i++) {
        int x = rand() % (MAP_WIDTH - 2) + 1;
        int y = rand() % (MAP_HEIGHT - 2) + 1;
        game->map[y][x] = WALL_CHAR;
    }
}

void spawn_player(Game *game) {
    while (1) {
        int x = rand() % (MAP_WIDTH - 2) + 1;
        int y = rand() % (MAP_HEIGHT - 2) + 1;

        if (game->map[y][x] == FLOOR_CHAR) {
            game->player.base.x = x;
            game->player.base.y = y;
            game->player.base.character = PLAYER_CHAR;
            game->player.health = 100;
            game->player.speed = 1.0;
            game->player.ammo = 10;
            break;
        }
    }
}

void spawn_enemies(Game *game) {
    game->num_enemies = 0;
    for (int i = 0; i < NUM_ENEMIES; i++) {
        while (1) {
            int x = rand() % (MAP_WIDTH - 2) + 1;
            int y = rand() % (MAP_HEIGHT - 2) + 1;
            if (game->map[y][x] == FLOOR_CHAR) {
                game->enemies[game->num_enemies].base.x = x;
                game->enemies[game->num_enemies].base.y = y;
                game->enemies[game->num_enemies].base.character = ENEMY_CHAR;
                game->enemies[game->num_enemies].health = 50;
                game->enemies[game->num_enemies].speed = 0.5;
                game->enemies[game->num_enemies].last_shot_time = get_time();
                game->enemies[game->num_enemies].shoot_delay = 1.0 + (rand() % 200) / 100.0;
                
                game->num_enemies++;
                break;
            }
        }
    }
}

void move_entity(Game *game, Entity *entity, float dx, float dy) {
    int new_x = (int)(entity->x + dx);
    int new_y = (int)(entity->y + dy);
    if (new_x >= 0 && new_x < MAP_WIDTH && new_y >= 0 && new_y < MAP_HEIGHT) {
        if (game->map[new_y][new_x] == FLOOR_CHAR) {
            entity->x = new_x;
            entity->y = new_y;
        }
    }
}

void handle_input(Game *game) {
    int key = getch();

    switch (key) {
        case KEY_UP:
            move_entity(game, &game->player.base, 0, -1);
            break;
        case KEY_DOWN:
            move_entity(game, &game->player.base, 0, 1);
            break;
        case KEY_LEFT:
            move_entity(game, &game->player.base, -1, 0);
            break;
        case KEY_RIGHT:
            move_entity(game, &game->player.base, 1, 0);
            break;
        case 'w':
            shoot(game, game->player.base.x, game->player.base.y, 0, -1, false);
            break;
        case 's':
            shoot(game, game->player.base.x, game->player.base.y, 0, 1, false);
            break;
        case 'a':
            shoot(game, game->player.base.x, game->player.base.y, -1, 0, false);
            break;
        case 'd':
            shoot(game, game->player.base.x, game->player.base.y, 1, 0, false);
            break;
        case 'q':
            shoot(game, game->player.base.x, game->player.base.y, -1, -1, false);
            break;
        case 'e':
            shoot(game, game->player.base.x, game->player.base.y, 1, -1, false);
            break;
        case 'z':
            shoot(game, game->player.base.x, game->player.base.y, -1, 1, false);
            break;
        case 'c':
            shoot(game, game->player.base.x, game->player.base.y, 1, 1, false);
            break;
        case 'x':
            game->game_over = true;
            break;
    }
}

void shoot(Game *game, float x, float y, float dx, float dy, bool is_enemy) {
    if (!is_enemy) {
        if (game->player.ammo <= 0) return;
        game->player.ammo--;
    }
    if (game->projectile_count >= MAX_PROJECTILES) return;
    Projectile *projectile = &game->projectiles[game->projectile_count++];
    projectile->base.x = x;
    projectile->base.y = y;
    projectile->base.character = is_enemy ? ENEMY_ARROW_CHAR : ARROW_CHAR;
    projectile->dx = dx;
    projectile->dy = dy;
    projectile->speed = 2.0;
    projectile->is_enemy = is_enemy;
}

void update_enemies(Game *game, float delta_time) {
    double current_time = get_time();
    for (int i = 0; i < game->num_enemies; i++) {
        Enemy *enemy = &game->enemies[i];
        // movement towards player with slight randomness
        float dx = game->player.base.x - enemy->base.x;
        float dy = game->player.base.y - enemy->base.y;
        float distance = sqrt(dx*dx + dy*dy);
        if (distance > 0) {
            dx /= distance;
            dy /= distance;
        }
        // add slight random movement
        dx += (rand() % 100 - 50) / 500.0;
        dy += (rand() % 100 - 50) / 500.0;

        float new_x = enemy->base.x + dx * enemy->speed * delta_time;
        float new_y = enemy->base.y + dy * enemy->speed * delta_time;
        int int_new_x = (int)new_x;
        int int_new_y = (int)new_y;
        if (int_new_x >= 0 && int_new_x < MAP_WIDTH &&
            int_new_y >= 0 && int_new_y < MAP_HEIGHT &&
            game->map[int_new_y][int_new_x] == FLOOR_CHAR) {
                enemy->base.x = new_x;
                enemy->base.y = new_y;
            }
        // shooting
        if (current_time - enemy->last_shot_time >= enemy->shoot_delay) {
            float shot_dx = game->player.base.x - enemy->base.x;
            float shot_dy = game->player.base.y - enemy->base.y;
            float shot_distance = sqrt(shot_dx*shot_dx + shot_dy*shot_dy);
            if (shot_distance > 0) {
                shot_dx /= shot_distance;
                shot_dy /= shot_distance;
            }
            shoot(game, enemy->base.x, enemy->base.y, shot_dx, shot_dy, true);
            enemy->last_shot_time = current_time;
            enemy->shoot_delay = 1.0 + (rand() % 200) / 100.0;
        }
    }
}

void update_projectiles(Game *game, float delta_time) {
    for (int i = 0; i < game->projectile_count; i++) {
        Projectile *projectile = &game->projectiles[i];
        projectile->base.x += projectile->dx * projectile->speed * delta_time;
        projectile->base.y += projectile->dy * projectile->speed * delta_time;
        int x = (int)projectile->base.x;
        int y = (int)projectile->base.y;
        // check bounds
        if (x < 0 || x >= MAP_WIDTH || y < 0 || y >= MAP_HEIGHT) {
            // remove projectile
            memmove(projectile, projectile + 1,
                    (game->projectile_count - i - 1) * sizeof(Projectile));
            game->projectile_count--;
            i--;
            continue;
        }
        // wall collision
        if (game->map[y][x] == WALL_CHAR) {
            memmove(projectile, projectile + 1,
                    (game->projectile_count - i - 1) * sizeof(Projectile));
            game->projectile_count--;
            i--;
            continue;
        }
        // player hit by enemy projectile
        if (projectile->is_enemy) {
            if ((int)game->player.base.x == x && (int)game->player.base.y == y) {
                game->player.health -= 10;
                memmove(projectile, projectile + 1,
                    (game->projectile_count - i - 1) * sizeof(Projectile));
                game->projectile_count--;
                i--;
                if (game->player.health <= 0) {
                    game->game_over = true;
                }
                continue;
            }
        }
        // player projectile hitting enemy
        else {
            for (int j = 0; j < game->num_enemies; j++) {
                if ((int)game->enemies[j].base.x == x &&
                    (int)game->enemies[j].base.y == y) {
                        // remove enemy and projectile
                        memmove(&game->enemies[j], &game-> enemies[j+1],
                        (game->num_enemies - j - 1) * sizeof(Enemy));
                        game->num_enemies--;
                        memmove(projectile, projectile + 1,
                            (game->projectile_count - i - 1) * sizeof(Projectile));
                        game->projectile_count--;
                        game->player.ammo += 2;
                        i--;
                        break;
                    }
            }
        }
    }
}

void render(Game *game) {
    clear();
    // render map
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            int color_pair = COLOR_FLOOR;
            if (game->map[y][x] == WALL_CHAR) {
                color_pair = COLOR_WALL;
            }
            mvaddch(y, x, game->map[y][x] | COLOR_PAIR(color_pair));
        }
    }
    // render player
    mvaddch(game->player.base.y, game->player.base.x,
            game->player.base.character | COLOR_PAIR(COLOR_PLAYER));
    // render enemies
    for (int i = 0; i < game->num_enemies; i++) {
        mvaddch(game->enemies[i].base.y, game->enemies[i].base.x,
                game->enemies[i].base.character | COLOR_PAIR(COLOR_ENEMY));
    }
    // render projectiles
    for (int i = 0; i < game->projectile_count; i++) {
        int color = game->projectiles[i].is_enemy ?
            COLOR_ENEMY_PROJECTILE : COLOR_PROJECTILE;
        mvaddch(game->projectiles[i].base.y, game->projectiles[i].base.x,
                game->projectiles[i].base.character | COLOR_PAIR(color));
    }
    // render status bar below map
    char status[100];
    snprintf(status, sizeof(status),
        "Enemies Left: %d | Player Health: %d | Ammo: %d",
        game->num_enemies, game->player.health, game->player.ammo);
    // move to a new line after the map
    mvaddstr(MAP_HEIGHT, 0, status);
    attron(COLOR_PAIR(COLOR_STATUS));
    mvaddstr(MAP_HEIGHT, 0, status);
    attroff(COLOR_PAIR(COLOR_STATUS));

    refresh();
}

void run_game(Game *game) {
    nodelay(stdscr, TRUE);
    while (!game->game_over) {
        double current_time = get_time();
        float delta_time = current_time - game->last_update;
        game->last_update = current_time;
        handle_input(game);
        update_projectiles(game, delta_time);
        update_enemies(game, delta_time);
        render(game);
        // check for win condition
        if (game->num_enemies == 0) {
            clear();
            // get screen dimensions
            int max_y, max_x;
            getmaxyx(stdscr, max_y, max_x);
            // win message
            const char *win_message = "You Win!";
            const char *play_again_message = "Press 'y' to play again or 'q' to quit";
            int win_y = max_y / 2 - 1;
            int win_x = (max_x - strlen(win_message)) / 2;
            int play_again_y = max_y / 2 + 1;
            int play_again_x = (max_x - strlen(play_again_message)) / 2;
            // render win message
            attron(COLOR_PAIR(COLOR_WIN_MESSAGE) | A_BOLD);
            mvaddstr(win_y, win_x, win_message);
            attroff(COLOR_PAIR(COLOR_WIN_MESSAGE) | A_BOLD);
            // render play again message
            attron(COLOR_PAIR(COLOR_STATUS));
            mvaddstr(play_again_y, play_again_x, play_again_message);
            attroff(COLOR_PAIR(COLOR_STATUS));
            refresh();
            // wait for user input
            while (1) {
                int key = getch();
                if (key == 'y') {
                    init_game(game, game->stdscr);
                    break;
                } else if (key == 'q') {
                    game->game_over = true;
                    break;
                }
            }
        }
        // small delay to control game speed
        napms(50);
    }
}

void init_game(Game *game, WINDOW *stdscr) {
    // reset game state
    memset(game, 0, sizeof(Game));
    game->stdscr = stdscr;
    game->game_over = false;
    game->last_update = get_time();
    game->projectile_count = 0;
    // setup colors if not already done
    setup_colors();
    // generate map
    generate_map(game);
    // spawn player and enemies
    spawn_player(game);
    spawn_enemies(game);
}

int main() {
    // initialize ncurses
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    // seed random number generator
    srand(time(NULL));
    // setup game and run
    Game game;
    // first game initialization
    init_game(&game, stdscr);
    // main game loop
    run_game(&game);
    // clean up ncurses
    endwin();
    return 0;
}