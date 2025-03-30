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

typedef struct { uint16_t x; uint16_t y; } t_coord;

SDL_Window   * g_window;
SDL_Renderer * g_renderer;
SDL_Texture  * g_circle_texture;
bool           g_debug;
uint16_t       g_selected;
float          g_aco_evap_rate;
float          g_aco_alpha;          
float          g_aco_beta;        

struct {
    uint16_t    capacity;
    uint16_t    size; 
    t_coord   * centers;
    uint16_t    (*grids)[4];
    uint16_t  * edges_caps;
    uint16_t  * edges_sizes;
    uint16_t ** edges;
} NODES;

struct {
    uint16_t     capacity;
    uint16_t     size;
    SDL_FColor   color;
    SDL_Vertex * verts;
    int        * vidxs; // SDL_RenderGeometry() takes ints
    uint16_t   * a_nodes;
    uint16_t   * b_nodes;
    float      * lengths;
    float      * pheromons;
} EDGES;

void Init(void) {
    g_aco_evap_rate = 0.1f;
    g_aco_alpha     = 1.0f;
    g_aco_beta      = 2.0f;

    NODES.capacity    = 32;
    NODES.size        = 1; // 0 invalid idx
    NODES.centers     = SDL_malloc(NODES.capacity * sizeof(*NODES.centers));
    NODES.grids       = SDL_calloc(GRIDS_COUNT, sizeof(*NODES.grids));
    NODES.edges_caps  = SDL_malloc(NODES.capacity * sizeof(*NODES.edges_caps));
    NODES.edges_sizes = SDL_malloc(NODES.capacity * sizeof(*NODES.edges_sizes));
    NODES.edges       = SDL_malloc(NODES.capacity * sizeof(*NODES.edges));
    if (!NODES.centers || !NODES.grids || !NODES.edges_caps || !NODES.edges_sizes || !NODES.edges) {
        SDL_Log("Memory allocation failed in NODES initizalization.\n");
        exit(1);
    }
    for (int i = 1; i < NODES.capacity; i++) {
        NODES.edges_caps[i]  = 8;
        NODES.edges_sizes[i] = 0;
        NODES.edges[i]       = SDL_malloc(8 * sizeof(*NODES.edges[i])); // TODO rm mallocs
        if (!NODES.edges[i]) {
            SDL_Log("Memory allocation failed in NODES initizalization.\n");
            exit(1);
        }
    }

    EDGES.capacity  = 32;
    EDGES.size      = 0;
    EDGES.color     = (SDL_FColor) { 0, 255, 0, 255 };
    EDGES.verts     = SDL_malloc(EDGES.capacity * 4 * sizeof(*EDGES.verts));
    EDGES.vidxs     = SDL_malloc(EDGES.capacity * 6 * sizeof(*EDGES.vidxs));
    EDGES.a_nodes   = SDL_malloc(EDGES.capacity * sizeof(*EDGES.a_nodes));
    EDGES.b_nodes   = SDL_malloc(EDGES.capacity * sizeof(*EDGES.b_nodes));
    EDGES.lengths   = SDL_malloc(EDGES.capacity * sizeof(*EDGES.lengths));
    EDGES.pheromons = SDL_malloc(EDGES.capacity * sizeof(*EDGES.pheromons));
    if (!EDGES.verts || !EDGES.vidxs || !EDGES.a_nodes || !EDGES.b_nodes || !EDGES.lengths || !EDGES.pheromons) {
        SDL_Log("Memory allocation failed EDGE in initizalization.\n");
        exit(1);
    }
}

void RenderEdges(void) {
    if (EDGES.size == 0) { return; }
    SDL_RenderGeometry(g_renderer, NULL, EDGES.verts, EDGES.size * 4, EDGES.vidxs, EDGES.size * 6);
}

void AddNewEdge(int aX, int aY, int bX, int bY, int width) {
    if (EDGES.size >= EDGES.capacity) {
        EDGES.capacity *= 2;
        EDGES.verts = SDL_realloc(EDGES.verts, EDGES.capacity * 4 * sizeof(*EDGES.verts));
        EDGES.vidxs = SDL_realloc(EDGES.vidxs, EDGES.capacity * 6 * sizeof(*EDGES.vidxs));
        if (!EDGES.verts || !EDGES.vidxs) {
            SDL_Log("Memory reallocation failed at line %d.\n", __LINE__);
            exit(1);
        }
    }

    float dx  = bX - aX;
    float dy  = bY - aY;
    float len = SDL_sqrt(dx*dx + dy*dy);
    float pX  = (-dy / len) * width;
    float pY  = ( dx / len) * width;
    
    int vstart = EDGES.size * 4;
    SDL_Vertex v0 = { .position = { aX + pX, aY + pY }, .color = EDGES.color };
    SDL_Vertex v1 = { .position = { bX + pX, bY + pY }, .color = EDGES.color };
    SDL_Vertex v2 = { .position = { bX - pX, bY - pY }, .color = EDGES.color };
    SDL_Vertex v3 = { .position = { aX - pX, aY - pY }, .color = EDGES.color };    
    EDGES.verts[vstart + 0] = v0;
    EDGES.verts[vstart + 1] = v1;
    EDGES.verts[vstart + 2] = v2;
    EDGES.verts[vstart + 3] = v3;   
    
    EDGES.vidxs[EDGES.size * 6 + 0] = vstart + 0;
    EDGES.vidxs[EDGES.size * 6 + 1] = vstart + 1;
    EDGES.vidxs[EDGES.size * 6 + 2] = vstart + 2;
    EDGES.vidxs[EDGES.size * 6 + 3] = vstart + 0;
    EDGES.vidxs[EDGES.size * 6 + 4] = vstart + 2;
    EDGES.vidxs[EDGES.size * 6 + 5] = vstart + 3;
    
    EDGES.size++;
    SDL_Log("New edge added.\n");
}

void DrawCircle(int x, int y) {
    int radius = CIRCLE_RAD * 4;
    int offsetX = radius - 1;
    int offsetY = 0;
    int d = 1 - radius;
    while (offsetX >= offsetY) {
        SDL_RenderPoint(g_renderer, x + offsetX, y - offsetY);
        SDL_RenderPoint(g_renderer, x + offsetY, y - offsetX);
        SDL_RenderPoint(g_renderer, x - offsetY, y - offsetX);
        SDL_RenderPoint(g_renderer, x - offsetX, y - offsetY);
        SDL_RenderPoint(g_renderer, x - offsetX, y + offsetY);
        SDL_RenderPoint(g_renderer, x - offsetY, y + offsetX);
        SDL_RenderPoint(g_renderer, x + offsetY, y + offsetX);
        SDL_RenderPoint(g_renderer, x + offsetX, y + offsetY);
        offsetY++;
        if (d <= 0) {
            d += 2 * offsetY + 1;
        } else {
            offsetX--;
            d += 2 * (offsetY - offsetX) + 1;
        }
    }
}

void RenderNodes(void) {
    for (size_t i = 1; i < NODES.size; i++) {
        if (g_debug) { DrawCircle(NODES.centers[i].x, NODES.centers[i].y); }

        int x = NODES.centers[i].x - CIRCLE_RAD; // shifting coordinates to top left
        int y = NODES.centers[i].y - CIRCLE_RAD;
        SDL_RenderTexture(g_renderer, g_circle_texture, NULL, &(SDL_FRect){ x, y, CIRCLE_SIZE, CIRCLE_SIZE });
    }
}

uint16_t SearchNode(int x, int y, int area) {
    if (x < GRID_SIZE || x > WIN_WIDTH - GRID_SIZE || y < GRID_SIZE || y > WIN_HEIGHT - GRID_SIZE) {
       SDL_Log("Invalid position %d:%d.\n", x, y);
       return 0;
    }

    int grid_idx = ((int)(y / GRID_SIZE) * GRIDS_WIDTH) + (x / GRID_SIZE);
    for (int r = -1; r <= 1; r++) {
        for (int c = -1; c <= 1; c++) {
            int n_grid_idx = grid_idx + (r * GRIDS_WIDTH) + c;
            for (size_t j = 0; j < 4; j++) {
                uint16_t circle_idx = NODES.grids[n_grid_idx][j];
                if (circle_idx) {
                    uint16_t circle_x = NODES.centers[circle_idx].x;
                    uint16_t circle_y = NODES.centers[circle_idx].y;
                    int dx = circle_x - x;
                    int dy = circle_y - y;
                    int distance = dx * dx + dy * dy;
                    if (distance <= area) { return circle_idx; }
                }
            }
        }
    }
    return 0;
}

uint16_t SelectNode(int x, int y) {
    int dsquared = CIRCLE_RAD * CIRCLE_RAD;
    return SearchNode(x, y, dsquared);
}

void AddNewNode(int x, int y) {
    int border_min   = GRID_SIZE  + CIRCLE_RAD;
    int border_max_x = WIN_WIDTH  - GRID_SIZE - CIRCLE_RAD;
    int border_max_y = WIN_HEIGHT - GRID_SIZE - CIRCLE_RAD;
    x = x < border_min   ? border_min   : x;
    x = x > border_max_x ? border_max_x : x;
    y = y < border_min   ? border_min   : y;
    y = y > border_max_y ? border_max_y : y;

    int dsquared = CIRCLE_SIZE * CIRCLE_SIZE * 4;
    if (SearchNode(x, y, dsquared)) {
        SDL_Log("Too close to another circle.\n");
        return;
    }

    if (NODES.size >= NODES.capacity) {
        NODES.capacity *= 2;
        NODES.centers  = SDL_realloc(NODES.centers, NODES.capacity * sizeof(*NODES.centers));
        if (!NODES.centers) {
            SDL_Log("Memory reallocation failed at line %d.\n", __LINE__);
            exit(1);
        }
    }

    int grid_idx = ((int)(y / GRID_SIZE) * GRIDS_WIDTH) + (x / GRID_SIZE);
    for (int i = 0; i < 4; i++) {
        if (NODES.grids[grid_idx][i] == 0) {
            NODES.centers[NODES.size] = (t_coord) { x, y };
            NODES.grids[grid_idx][i] = NODES.size;
            NODES.size++;
            return;
        }
    }
    SDL_Log("Grid is full.\n");
}

void DrawEdge(int aX, int aY, int bX, int bY, int width, SDL_FColor * color) {
    float dx  = bX - aX;
    float dy  = bY - aY;
    float len = SDL_sqrt(dx * dx + dy * dy);
    float pX  = (-dy / len) * width;
    float pY  = ( dx / len) * width;

    SDL_Vertex verts[] = {
        { .position = { aX + pX, aY + pY }, .color = *color },
        { .position = { bX + pX, bY + pY }, .color = *color },
        { .position = { bX - pX, bY - pY }, .color = *color },
        { .position = { aX - pX, aY - pY }, .color = *color }
    };
    int indices[] = { 0, 1, 2, 0, 2, 3 };

    SDL_RenderGeometry(g_renderer, NULL, verts, 4, indices, 6);
}


///////////////////////////////////////////////////////////////////////////////////
///SDL 
///////////////////////////////////////////////////////////////////////////////////

// 'main' function, running every frame
SDL_AppResult SDL_AppIterate(void * appstate)
{
    SDL_SetRenderDrawColor(g_renderer, 240, 240, 240, 255);
    SDL_RenderClear(g_renderer);

    if (g_debug) {
        SDL_SetRenderDrawColor(g_renderer, 255, 0, 0, 255);
        SDL_RenderRect(g_renderer, &(SDL_FRect){ GRID_SIZE, GRID_SIZE, WIN_WIDTH-(2*GRID_SIZE), WIN_HEIGHT-(2*GRID_SIZE) });
    }

    RenderEdges();
    RenderNodes();

    if (g_selected) {
        float x, y;
        SDL_GetMouseState(&x, &y);
        int dest_x = (int)x;
        int dest_y = (int)y;
        uint16_t src_x = NODES.centers[g_selected].x;
        uint16_t src_y = NODES.centers[g_selected].y;
        DrawEdge(src_x, src_y, dest_x, dest_y, 4, &(SDL_FColor){ 0, 0, 255, 255 });
    }

    SDL_RenderPresent(g_renderer);
    return SDL_APP_CONTINUE;
}

// runs at startup
SDL_AppResult SDL_AppInit(void ** appstate, int argc, char * argv[])
{
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    if (!SDL_CreateWindowAndRenderer("main.exe", WIN_WIDTH, WIN_HEIGHT, 0, &g_window, &g_renderer)) {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    g_circle_texture = IMG_LoadTexture(g_renderer, CIRCLE_PATH);
    if (!g_circle_texture) {
        SDL_Log("Couldn't load %s: %s", CIRCLE_PATH, SDL_GetError());
        return SDL_APP_FAILURE;
    }

    Init();
    
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
        if (event->button.button == SDL_BUTTON_LEFT) {
            uint16_t selected = SelectNode(x, y);
            if (selected && g_selected) {
                if (selected == g_selected) {
                    g_selected = 0;
                } else {
                AddNewEdge(NODES.centers[g_selected].x, NODES.centers[g_selected].y, NODES.centers[selected].x, NODES.centers[selected].y, 6);
                    g_selected = 0;
                }
            } else if (selected && !g_selected) {
                g_selected = selected;
            } else if (!selected && g_selected) {
                g_selected = 0;
            } else if (!selected && !g_selected) {
                AddNewNode(x, y);
            }
        }
    } else {
        // TODO cancel & delete
        //
    }

    return SDL_APP_CONTINUE;
}

// runs at shutdown
void SDL_AppQuit(void * appstate, SDL_AppResult result) { /*SDL automatically cleans up window/renderer*/ }

