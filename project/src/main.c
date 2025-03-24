#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_image.h>

#define WIN_WIDTH       1280
#define WIN_HEIGHT      1024
#define CIRCLE_PATH     "./src/circle.png"
#define CIRCLE_SIZE     ((int)(WIN_HEIGHT / 24))
#define CIRCLE_RAD      (CIRCLE_SIZE / 2)
#define GRID_SIZE       (CIRCLE_SIZE * 2)
#define GRIDS_WIDTH     ((int)((WIN_WIDTH  + GRID_SIZE - 1) / GRID_SIZE))
#define GRIDS_HEIGHT    ((int)((WIN_HEIGHT + GRID_SIZE - 1) / GRID_SIZE))
#define GRIDS_COUNT     (GRIDS_WIDTH * GRIDS_HEIGHT)

SDL_Window   * window;
SDL_Renderer * renderer;
SDL_Texture  * circle_texture;
bool     g_debug;
uint32_t g_selected;

void DrawCircle(int x, int y);
void G_AddCircle(int x, int y);
uint32_t IsCircle(int x, int y);

struct {
    uint32_t   circles_capacity;
    uint32_t   circles_size;
    uint32_t * circles_data;            // | x 11 | y 11 | unused 9 | alive 1 bit
    uint32_t   grids[GRIDS_COUNT][4];
    // TODO
} G;
inline int GetX(uint32_t data) { return data >> 21; }
inline int GetY(uint32_t data) { return (data >> 10) & ((1U << 11) - 1); }

void G_Init(void) {
    G.circles_capacity = 10;
    G.circles_size     = 1; // 0 -> invalid idx
    G.circles_data     = SDL_malloc(G.circles_capacity * sizeof(*G.circles_data));
    if (!G.circles_data) {
        SDL_Log("Memory allocation failed at line %d.\n", __LINE__);
        exit(1);
    }
}

void G_RenderCircles(void) {
    for (size_t i = 1; i < G.circles_size; i++) {
        if (G.circles_data[i] & 1) {
            // shifting coordinates to top left
            int x = GetX(G.circles_data[i]) - CIRCLE_RAD;
            int y = GetY(G.circles_data[i]) - CIRCLE_RAD;
            SDL_RenderTexture(renderer, circle_texture, NULL, &(SDL_FRect){ x, y, CIRCLE_SIZE, CIRCLE_SIZE });
            
            if (g_debug) { DrawCircle(x + CIRCLE_RAD, y + CIRCLE_RAD); }
        }
    }
}

// 'main' function, running every frame
SDL_AppResult SDL_AppIterate(void * appstate)
{
    SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
    SDL_RenderClear(renderer);

    if (g_debug) {
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        SDL_RenderRect(renderer, &(SDL_FRect) {.x=GRID_SIZE, .y=GRID_SIZE, .w=WIN_WIDTH-(2*GRID_SIZE), .h=WIN_HEIGHT-(2*GRID_SIZE)});
    }

    G_RenderCircles();

    if (g_selected) {
        float x, y;
        SDL_GetMouseState(&x, &y);
        // TODO change line into rect
        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
        SDL_RenderLine(renderer, x, y, GetX(g_selected), GetY(g_selected));
    }
    
    SDL_RenderPresent(renderer);
    return SDL_APP_CONTINUE;
}

// runs at startup
SDL_AppResult SDL_AppInit(void ** appstate, int argc, char * argv[])
{
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    if (!SDL_CreateWindowAndRenderer("main.exe", WIN_WIDTH, WIN_HEIGHT, 0, &window, &renderer)) {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // load circle's texture
    circle_texture = IMG_LoadTexture(renderer, CIRCLE_PATH);
    if (!circle_texture) {
        SDL_Log("Couldn't load %s: %s", CIRCLE_PATH, SDL_GetError());
        return SDL_APP_FAILURE;
    }

    G_Init();
    
    return SDL_APP_CONTINUE;
}

// event handler function
SDL_AppResult SDL_AppEvent(void * appstate, SDL_Event * event)
{
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;
    }
    
    // KEY EVENTS
    if (event->type == SDL_EVENT_KEY_DOWN) {
        if (event->key.scancode == SDL_SCANCODE_D) {
            g_debug ^= 1;
        }
    }
    
    // MOUSE EVENTS
    if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        int x = event->motion.x;
        int y = event->motion.y;
        uint32_t selected = IsCircle(x, y);
        if (event->button.button == SDL_BUTTON_LEFT) {
            if (selected == 0) {
                // empty / invalid position selected
                g_selected = 0;
                G_AddCircle(x, y);
            } else if (selected == g_selected) {
                // same node selected
                g_selected = 0;
            } else if (g_selected) {
                // end of line selected
                // TODO add to rect-line if new edge
//bool SDL_RenderFillRect(SDL_Renderer *renderer, const SDL_FRect *rect); 
//bool SDL_RenderFillRects(SDL_Renderer *renderer, const SDL_FRect *rects, int count);
                g_selected = 0;
            } else {
                // first node selected
                g_selected = selected;
            }
        }
    } else {
        // TODO cancel & delete
        //
    }

    return SDL_APP_CONTINUE;
}

// runs at shutdown
void SDL_AppQuit(void * appstate, SDL_AppResult result)
{
    // SDL automatically cleans up window/renderer
}

void DrawCircle(int x, int y) {
    int radius = CIRCLE_RAD * 4;
    int offsetX = radius - 1;
    int offsetY = 0;
    int d = 1 - radius;

    while (offsetX >= offsetY) {
        SDL_RenderPoint(renderer, x + offsetX, y - offsetY);
        SDL_RenderPoint(renderer, x + offsetY, y - offsetX);
        SDL_RenderPoint(renderer, x - offsetY, y - offsetX);
        SDL_RenderPoint(renderer, x - offsetX, y - offsetY);
        SDL_RenderPoint(renderer, x - offsetX, y + offsetY);
        SDL_RenderPoint(renderer, x - offsetY, y + offsetX);
        SDL_RenderPoint(renderer, x + offsetY, y + offsetX);
        SDL_RenderPoint(renderer, x + offsetX, y + offsetY);

        offsetY++;
        if (d <= 0) {
            d += 2 * offsetY + 1;
        } else {
            offsetX--;
            d += 2 * (offsetY - offsetX) + 1;
        }
    }
}

uint32_t IsCircle(int x, int y) {
    if (x < GRID_SIZE || x > WIN_WIDTH - GRID_SIZE || y < GRID_SIZE || y > WIN_HEIGHT - GRID_SIZE) {
       SDL_Log("Invalid position %d:%d.\n", x, y);
       return 0;
    }
    int grid_idx = ((int)(y / GRID_SIZE) * GRIDS_WIDTH) + (x / GRID_SIZE);
    for (size_t i = 0; i < 4; i++) {
        int circle_idx = G.grids[grid_idx][i];
        if (circle_idx) {
            uint32_t circle_data = G.circles_data[circle_idx];
            uint32_t circle_x = GetX(circle_data);
            uint32_t circle_y = GetY(circle_data);
            
            int distance = (circle_x - x) * (circle_x - x) + (circle_y - y) * (circle_y - y);
            if (distance <= CIRCLE_SIZE * CIRCLE_SIZE) {
                SDL_Log("Coordinate is inside a circle.\n");
                return circle_data;
            }
        }
    }
    return 0;
}

void G_AddCircle(int x, int y) {
    if (x < GRID_SIZE || x > WIN_WIDTH - GRID_SIZE || y < GRID_SIZE || y > WIN_HEIGHT - GRID_SIZE) {
        SDL_Log("Invalid position %d:%d.\n", x, y);
        return;
    }

    // shift inside from the edges
    if (x < GRID_SIZE + CIRCLE_RAD) { x = GRID_SIZE + CIRCLE_RAD; }
    else if (x > WIN_WIDTH - GRID_SIZE - CIRCLE_RAD)  { x = WIN_WIDTH  - GRID_SIZE - CIRCLE_RAD; }
    if (y < GRID_SIZE + CIRCLE_RAD) { y = GRID_SIZE + CIRCLE_RAD; }
    else if (y > WIN_HEIGHT - GRID_SIZE - CIRCLE_RAD) { y = WIN_HEIGHT - GRID_SIZE - CIRCLE_RAD; }

    // check collisions
    int grid_idx = ((int)(y / GRID_SIZE) * GRIDS_WIDTH) + (x / GRID_SIZE);
    for (int i = 0; i < 4; i++) {
        // checking is there any empty space left in the current grid
        if (G.grids[grid_idx][i] == 0) {
            // checking 3x3 grids' circles
            for (int r = -1; r <= 1; r++) {
                for (int c = -1; c <= 1; c++) {
                    int n_grid_idx = grid_idx + (r * GRIDS_WIDTH) + c;
                    // checking the distance between 
                    for (size_t j = 0; j < 4; j++) {
                        int n_circle_idx = G.grids[n_grid_idx][j];
                        if (n_circle_idx) {
                            uint32_t neighbour = G.circles_data[n_circle_idx];
                            uint32_t neighbour_x = GetX(neighbour);
                            uint32_t neighbour_y = GetY(neighbour);
                            int distance = (neighbour_x - x) * (neighbour_x - x) + (neighbour_y - y) * (neighbour_y - y);
                            if (distance <= CIRCLE_SIZE * CIRCLE_SIZE * 4) {
                                SDL_Log("Too close to another circle.\n");
                                return;
                            }
                        }
                    }
                }
            }
            // adding the new circle
            if (G.circles_size >= G.circles_capacity) {
                G.circles_data = SDL_realloc(G.circles_data, (G.circles_capacity * 2) * sizeof(*G.circles_data));
                if (!G.circles_data) {
                    SDL_Log("Memory reallocation failed at line %d.\n", __LINE__);
                    exit(1);
                }
                G.circles_capacity *= 2;
            }
            G.circles_data[G.circles_size] = x << 21 | y << 10 | 1;
            G.grids[grid_idx][i] = G.circles_size;
            G.circles_size++;
            SDL_Log("New circle placed: %d:%d, grid:%d/%d.\n", x, y, grid_idx, i);
            return;
        }
    }
}


