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
#define BASE_EDGECAP    8
#define MIN_EDGE_W      2
#define MAX_EDGE_W      CIRCLE_SIZE
#define EMPTY           (0xFFFF)

typedef uint16_t idx_t;
typedef struct { uint16_t x; uint16_t y; } coord_t;

SDL_Window   * g_window;
SDL_Renderer * g_renderer;
SDL_Texture  * g_circle_texture;
SDL_Texture  * g_ant_texture;
bool           g_run;
bool           g_debug;
uint16_t       g_selected;
uint16_t       g_aco_nest;
uint16_t       g_aco_food;
float          g_aco_evap_rate;
float          g_aco_alpha;          
float          g_aco_beta;        

#define ANT_SPEED   100.0f
#define ANT_PATH    "./src/ant.png"
#define ANT_SIZE    ((int)(CIRCLE_SIZE / 4))
#define ANT_RAD      (ANT_SIZE / 2)
struct Ant {
    uint16_t src_node;
    uint16_t des_node;
    uint16_t edge;
    float    edge_progress;
};
int g_ants_count;
float g_last_time;
struct Ant * ANTS;

struct {
    uint16_t    capacity;
    uint16_t    size; 
    coord_t   * centers;
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
    float      * widths;
    uint16_t   * a_nodes;
    uint16_t   * b_nodes;
    float      * lengths;
    float      * pheromons;
} EDGES;

void UpdateEdgeWidth(uint16_t edge, float multiplier) {
    uint16_t a_node = EDGES.a_nodes[edge];
    uint16_t b_node = EDGES.b_nodes[edge];
    uint16_t aX = NODES.centers[a_node].x;
    uint16_t aY = NODES.centers[a_node].y;
    uint16_t bX = NODES.centers[b_node].x;
    uint16_t bY = NODES.centers[b_node].y;
    float dX     = bX - aX;
    float dY     = bY - aY;
    float length = EDGES.lengths[edge];
    //TODO pheromonbol 
    EDGES.widths[edge] *= multiplier;
    float pX     = (-dY / length) * (EDGES.widths[edge] / 2);
    float pY     = ( dX / length) * (EDGES.widths[edge] / 2);
    int vstart = edge * 4;
    EDGES.verts[vstart + 0].position = (SDL_FPoint) { aX + pX, aY + pY }; 
    EDGES.verts[vstart + 1].position = (SDL_FPoint) { bX + pX, bY + pY };
    EDGES.verts[vstart + 2].position = (SDL_FPoint) { bX - pX, bY - pY };
    EDGES.verts[vstart + 3].position = (SDL_FPoint) { aX - pX, aY - pY };
}

void Init(void) {
    g_last_time     = (float)SDL_GetTicks();
    g_selected      = EMPTY;
    g_aco_evap_rate = 0.1f;
    g_aco_alpha     = 1.0f;
    g_aco_beta      = 2.0f;

    NODES.capacity    = 32;
    NODES.size        = 0;
    NODES.centers     = SDL_malloc(NODES.capacity * sizeof(*NODES.centers));
    NODES.grids       = SDL_malloc(GRIDS_COUNT    * sizeof(*NODES.grids));
    NODES.edges_caps  = SDL_malloc(NODES.capacity * sizeof(*NODES.edges_caps));
    NODES.edges_sizes = SDL_malloc(NODES.capacity * sizeof(*NODES.edges_sizes));
    NODES.edges       = SDL_malloc(NODES.capacity * sizeof(*NODES.edges));
    if (!NODES.centers || !NODES.grids || !NODES.edges_caps || !NODES.edges_sizes || !NODES.edges) {
        SDL_Log("Memory reallocation failed at line %d.\n", __LINE__);
        exit(1);
    }
    
    SDL_memset(NODES.grids, 0xFF, GRIDS_COUNT * sizeof(*NODES.grids));

    for (int i = 0; i < NODES.capacity; i++) {
        NODES.edges_caps[i]  = BASE_EDGECAP;
        NODES.edges_sizes[i] = 0;
        NODES.edges[i]       = SDL_malloc(BASE_EDGECAP * sizeof(*NODES.edges[i])); // TODO rm mallocs
        if (!NODES.edges[i]) {
            SDL_Log("Memory reallocation failed at line %d.\n", __LINE__);
            exit(1);
        }
    }

    EDGES.capacity  = 32;
    EDGES.size      = 0;
    EDGES.color     = (SDL_FColor) { 0, 255, 0, 255 };
    EDGES.verts     = SDL_malloc(EDGES.capacity * 4 * sizeof(*EDGES.verts));
    EDGES.vidxs     = SDL_malloc(EDGES.capacity * 6 * sizeof(*EDGES.vidxs));
    EDGES.widths    = SDL_malloc(EDGES.capacity * sizeof(*EDGES.widths));
    EDGES.a_nodes   = SDL_malloc(EDGES.capacity * sizeof(*EDGES.a_nodes));
    EDGES.b_nodes   = SDL_malloc(EDGES.capacity * sizeof(*EDGES.b_nodes));
    EDGES.lengths   = SDL_malloc(EDGES.capacity * sizeof(*EDGES.lengths));
    EDGES.pheromons = SDL_malloc(EDGES.capacity * sizeof(*EDGES.pheromons));
    if (!EDGES.verts || !EDGES.vidxs || !EDGES.widths || !EDGES.a_nodes || !EDGES.b_nodes || !EDGES.lengths || !EDGES.pheromons) {
        SDL_Log("Memory reallocation failed at line %d.\n", __LINE__);
        exit(1);
    }

    g_ants_count = 10000;
    ANTS = SDL_malloc(g_ants_count * sizeof(*ANTS));
    if (!ANTS) {
        SDL_Log("Memory reallocation failed at line %d.\n", __LINE__);
        exit(1);
    }
    for (int i = 0; i < g_ants_count; i++) {
        ANTS[i] = (struct Ant) { .src_node = 0, .des_node = 1/*TODO*/, .edge = 0, .edge_progress = 0.0f };
    }

}

void RenderEdges(void) {
    if (EDGES.size == 0) { return; }
    SDL_RenderGeometry(g_renderer, NULL, EDGES.verts, EDGES.size * 4, EDGES.vidxs, EDGES.size * 6);
}

uint16_t SearchEdgeAtNode(uint16_t a_node, uint16_t b_node) {
    for (uint16_t i = 0; i < NODES.edges_sizes[a_node]; i++) {
        uint16_t edge = NODES.edges[a_node][i];
        if (EDGES.b_nodes[edge] == b_node /*one condition must be enough*/) {
            return edge;
        }
    }
    return EMPTY;
}

void AddNewEdge(uint16_t a_node, uint16_t b_node) {
    if (a_node == b_node) {
        SDL_Log("AddNewEdge received same nodes.\n");
        return;
    }

    if (a_node > b_node) {
        uint16_t temp = a_node;
        a_node = b_node;
        b_node = temp;
    }
    
    if (SearchEdgeAtNode(a_node, b_node) != EMPTY) {
        SDL_Log("Edge already exists.\n");
        return;
    }
    
    uint16_t * size = &NODES.edges_sizes[a_node];
    if (*size >= NODES.edges_caps[a_node]) {
        NODES.edges_caps[a_node] *= 2;
        NODES.edges[a_node] = SDL_realloc(NODES.edges[a_node], NODES.edges_caps[a_node] * sizeof(*NODES.edges[a_node]));
        if (!NODES.edges[a_node]) {
            SDL_Log("Memory reallocation failed at line %d.\n", __LINE__);
            exit(1);
        }
    }
    NODES.edges[a_node][(*size)++] = EDGES.size;
    size = &NODES.edges_sizes[b_node];
    if (*size >= NODES.edges_caps[b_node]) {
        NODES.edges_caps[b_node] *= 2;
        NODES.edges[b_node] = SDL_realloc(NODES.edges[b_node], NODES.edges_caps[b_node] * sizeof(*NODES.edges[b_node]));
        if (!NODES.edges[b_node]) {
            SDL_Log("Memory reallocation failed at line %d.\n", __LINE__);
            exit(1);
        }
    }
    NODES.edges[b_node][(*size)++] = EDGES.size;

    if (EDGES.size >= EDGES.capacity) {
        EDGES.capacity *= 2;
        EDGES.verts     = SDL_realloc(EDGES.verts,     EDGES.capacity * 4 * sizeof(*EDGES.verts));
        EDGES.vidxs     = SDL_realloc(EDGES.vidxs,     EDGES.capacity * 6 * sizeof(*EDGES.vidxs));
        EDGES.widths    = SDL_realloc(EDGES.widths,    EDGES.capacity * sizeof(*EDGES.widths));
        EDGES.a_nodes   = SDL_realloc(EDGES.a_nodes,   EDGES.capacity * sizeof(*EDGES.a_nodes));
        EDGES.b_nodes   = SDL_realloc(EDGES.b_nodes,   EDGES.capacity * sizeof(*EDGES.b_nodes));
        EDGES.lengths   = SDL_realloc(EDGES.lengths,   EDGES.capacity * sizeof(*EDGES.lengths));
        EDGES.pheromons = SDL_realloc(EDGES.pheromons, EDGES.capacity * sizeof(*EDGES.pheromons));
        if (!EDGES.verts || !EDGES.vidxs || !EDGES.widths || !EDGES.a_nodes || !EDGES.b_nodes || !EDGES.lengths || !EDGES.pheromons) {
            SDL_Log("Memory reallocation failed at line %d.\n", __LINE__);
            exit(1);
        }
    }
    uint16_t aX = NODES.centers[a_node].x;
    uint16_t aY = NODES.centers[a_node].y;
    uint16_t bX = NODES.centers[b_node].x;
    uint16_t bY = NODES.centers[b_node].y;
    float dX     = bX - aX;
    float dY     = bY - aY;
    float length = SDL_sqrt(dX * dX + dY * dY);
    float pX     = (-dY / length) * MIN_EDGE_W;
    float pY     = ( dX / length) * MIN_EDGE_W;
    
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
    
    EDGES.widths[EDGES.size]    = MIN_EDGE_W;
    EDGES.a_nodes[EDGES.size]   = a_node;
    EDGES.b_nodes[EDGES.size]   = b_node;
    EDGES.lengths[EDGES.size]   = length;
    EDGES.pheromons[EDGES.size] = 0;

    SDL_Log("New edge added. Len=%f\n", EDGES.lengths[EDGES.size]);
    EDGES.size++;
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
    for (uint16_t i = 0; i < NODES.size; i++) {
        if (g_debug) { DrawCircle(NODES.centers[i].x, NODES.centers[i].y); }

        int x = NODES.centers[i].x - CIRCLE_RAD; // shifting coordinates to top left
        int y = NODES.centers[i].y - CIRCLE_RAD;
        SDL_RenderTexture(g_renderer, g_circle_texture, NULL, &(SDL_FRect){ x, y, CIRCLE_SIZE, CIRCLE_SIZE });
    }
}

uint16_t SearchNodeInArea(int x, int y, int area) {
    if (x < GRID_SIZE || x > WIN_WIDTH - GRID_SIZE || y < GRID_SIZE || y > WIN_HEIGHT - GRID_SIZE) {
       SDL_Log("Invalid position %d:%d.\n", x, y);
       return EMPTY;
    }

    int grid_idx = ((int)(y / GRID_SIZE) * GRIDS_WIDTH) + (x / GRID_SIZE);
    for (int r = -1; r <= 1; r++) {
        for (int c = -1; c <= 1; c++) {
            int n_grid_idx = grid_idx + (r * GRIDS_WIDTH) + c;
            for (size_t j = 0; j < 4; j++) {
                uint16_t circle_idx = NODES.grids[n_grid_idx][j];
                if (circle_idx != EMPTY) {
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
    return EMPTY;
}

uint16_t SelectNode(int x, int y) {
    int dsquared = CIRCLE_RAD * CIRCLE_RAD;
    return SearchNodeInArea(x, y, dsquared);
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
    if (SearchNodeInArea(x, y, dsquared) != EMPTY) {
        SDL_Log("Too close to another circle.\n");
        return;
    }

    if (NODES.size >= NODES.capacity) {
        NODES.capacity *= 2;
        NODES.centers     = SDL_realloc(NODES.centers,     NODES.capacity * sizeof(*NODES.centers));
        NODES.edges_caps  = SDL_realloc(NODES.edges_caps,  NODES.capacity * sizeof(*NODES.edges_caps));
        NODES.edges_sizes = SDL_realloc(NODES.edges_sizes, NODES.capacity * sizeof(*NODES.edges_sizes));
        NODES.edges       = SDL_realloc(NODES.edges,       NODES.capacity * sizeof(*NODES.edges));
        if (!NODES.centers || !NODES.edges_caps || !NODES.edges_sizes || !NODES.edges) {
            SDL_Log("Memory reallocation failed at line %d.\n", __LINE__);
            exit(1);
        }
        for (uint16_t i = NODES.size; i < NODES.capacity; i++) {
            NODES.edges_caps[i]  = BASE_EDGECAP;
            NODES.edges_sizes[i] = 0;
            NODES.edges[i]       = SDL_malloc(BASE_EDGECAP * sizeof(*NODES.edges[i])); 
            if (!NODES.edges[i]) {
                SDL_Log("Memory reallocation failed at line %d.\n", __LINE__);
                exit(1);
            }
        }
    }

    int grid_idx = ((int)(y / GRID_SIZE) * GRIDS_WIDTH) + (x / GRID_SIZE);
    for (int i = 0; i < 4; i++) {
        if (NODES.grids[grid_idx][i] == EMPTY) {
            NODES.centers[NODES.size] = (coord_t) { x, y };
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
    float pX  = (-dy / len) * MIN_EDGE_W;
    float pY  = ( dx / len) * MIN_EDGE_W;

    SDL_Vertex verts[] = {
        { .position = { aX + pX, aY + pY }, .color = *color },
        { .position = { bX + pX, bY + pY }, .color = *color },
        { .position = { bX - pX, bY - pY }, .color = *color },
        { .position = { aX - pX, aY - pY }, .color = *color }
    };
    int indices[] = { 0, 1, 2, 0, 2, 3 };

    SDL_RenderGeometry(g_renderer, NULL, verts, 4, indices, 6);
}

void UpdateAnts(uint64_t time) {
    float elapsed = (time - g_last_time) / 1000.f;
    for (int i = 0; i < g_ants_count; i++) {
        float edge_length = EDGES.lengths[ANTS[i].edge];
        ANTS[i].edge_progress += elapsed / (edge_length / ANT_SPEED);

        if (ANTS[i].edge_progress >= 1.0f) {
            uint16_t new_src = ANTS[i].des_node; // arrived
            ANTS[i].src_node = new_src;
            if (NODES.edges_sizes[new_src]) {
                int r            = SDL_rand(NODES.edges_sizes[new_src]); // [0..n-1]
                uint16_t edge    = NODES.edges[new_src][r];
                ANTS[i].edge     = edge;
                ANTS[i].des_node = (EDGES.a_nodes[edge] == new_src) ? EDGES.b_nodes[edge] : EDGES.a_nodes[edge];
            }
            ANTS[i].edge_progress = 0.0f;
        }
    }
    g_last_time = time;
}

void RenderAnts(void) {
    for (int i = 0; i < g_ants_count; i++) {
        coord_t src  = NODES.centers[ANTS[i].src_node]; 
        coord_t dest = NODES.centers[ANTS[i].des_node];
        int x = src.x + (dest.x - src.x) * ANTS[i].edge_progress - ANT_RAD;
        int y = src.y + (dest.y - src.y) * ANTS[i].edge_progress - ANT_RAD;
        SDL_RenderTexture(g_renderer, g_ant_texture, NULL, &(SDL_FRect){ x, y, ANT_SIZE, ANT_SIZE });
    }
}


///////////////////////////////////////////////////////////////////////////////////
///SDL 
///////////////////////////////////////////////////////////////////////////////////

// 'main' function, running every frame
SDL_AppResult SDL_AppIterate(void * appstate)
{
    uint64_t time = SDL_GetTicks();

    SDL_SetRenderDrawColor(g_renderer, 240, 240, 240, 255);
    SDL_RenderClear(g_renderer);

    if (g_debug) {
        SDL_SetRenderDrawColor(g_renderer, 255, 0, 0, 255);
        SDL_RenderRect(g_renderer, &(SDL_FRect){ GRID_SIZE, GRID_SIZE, WIN_WIDTH-(2*GRID_SIZE), WIN_HEIGHT-(2*GRID_SIZE) });
    }

    RenderEdges();
    RenderNodes();

    if (g_run) {
        UpdateAnts(time);
        RenderAnts();
    }

    if (g_selected != EMPTY) {
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
    
    SDL_Time t = 0;
    SDL_GetCurrentTime(&t);
    SDL_srand(t); // sdl:'This should be called on the same thread that calls SDL_rand()'

    g_circle_texture = IMG_LoadTexture(g_renderer, CIRCLE_PATH);
    if (!g_circle_texture) {
        SDL_Log("Couldn't load %s: %s", CIRCLE_PATH, SDL_GetError());
        return SDL_APP_FAILURE;
    }
    g_ant_texture    = IMG_LoadTexture(g_renderer, ANT_PATH);
    if (!g_ant_texture) {
        SDL_Log("Couldn't load %s: %s", ANT_PATH, SDL_GetError());
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
        if (event->key.scancode == SDL_SCANCODE_K) {
            for (uint16_t i = 0; i < EDGES.size; i++) { UpdateEdgeWidth(i, 0.8f); }
        }
        if (event->key.scancode == SDL_SCANCODE_L) {
            for (uint16_t i = 0; i < EDGES.size; i++) { UpdateEdgeWidth(i, 1.2f); }
        }
        if (event->key.scancode == SDL_SCANCODE_R) {
            g_run = true;
        }
    }
    
    // MOUSE EVENTS
    if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        int x = event->motion.x;
        int y = event->motion.y;
        if (event->button.button == SDL_BUTTON_LEFT) {
            uint16_t selected = SelectNode(x, y);
            if (selected != EMPTY && g_selected != EMPTY) {
                if (selected == g_selected) {
                    g_selected = EMPTY;
                } else {
                    AddNewEdge(g_selected, selected);
                    g_selected = EMPTY;
                }
            } else if (selected != EMPTY && g_selected == EMPTY) {
                g_selected = selected;
            } else if (selected == EMPTY && g_selected != EMPTY) {
                g_selected = EMPTY;
            } else if (selected == EMPTY && g_selected == EMPTY) {
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

