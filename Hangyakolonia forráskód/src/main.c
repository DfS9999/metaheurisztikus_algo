#define SDL_MAIN_USE_CALLBACKS 1
#include <global.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_image.h>
#include <SDL3/SDL_ttf.h>

/* globals */
struct nodes_s Nodes;
struct grids_s Grids;
struct edges_s Edges;
struct ants_s  Ants;
struct paths_s Paths;
id Nest;
id Food;

static SDL_Window       * Window;
static SDL_Renderer     * Renderer;
static SDL_Texture      * TextureCircle;
static SDL_Texture      * TextureNest;
static SDL_Texture      * TextureFood;
static SDL_Texture      * TextureForaging;
static SDL_Texture      * TextureHoming;
static TTF_Font         * Font;
static TTF_TextEngine   * TextEngine;
static TTF_Text         * Text;

#define TEXT_BUFFER_LEN 512
static char TextBuffer[TEXT_BUFFER_LEN];
static bool AnimationRunning;
static bool GraphModifiable;
static id SelectedNode;
static uint64_t LastTime = 0; /* timer */
static float AntTimer = 0.0f; /* timer for ants */
static float AntInterval = 0.1f;
static bool ShowAnts = true;

static void Initialize(void);
static void Restart(void);
static void Pause(void);
static void Reset(void);
static void SetAllAntsActive(void);
static void SaveGraph(void);
static bool LoadGraph(const char *);
static id   SearchNodeInArea(int, int, int); 
static bool AddToGrid(int, int);
static void ToggleAntsRender(void);

/**********************************************/
/************ SDL3 main functions *************/
/**********************************************/
/* runs per frame */
SDL_AppResult SDL_AppIterate(void * appstate)
{
    /* set current time & elapsed seconds */
    uint64_t currentTime = SDL_GetTicks();
    if (LastTime == 0) LastTime = currentTime;
    float elapsedSecs = (currentTime - LastTime) / 1000.f;
    LastTime = currentTime;

    /* background */
    SDL_SetRenderDrawColor(Renderer, 240, 240, 240, 255);
    SDL_RenderClear(Renderer);

    /* rendering Nest and Food */
    if (Nest != EMPTY) RenderNest();
    if (Food != EMPTY) RenderFood();
    
    /* rendering the edges */
    RenderEdges();

    /* updating the ants' properties, edges' pheromones (widths), and rendering the ants */
    if (AnimationRunning) {
        /* seperated start */
        if (Ants.actives < Ants.count) {
            AntTimer += elapsedSecs;
            if (AntTimer >= AntInterval) {
                AntTimer -= AntInterval;
                Ants.actives++;
            }
        }

        UpdateAnts(elapsedSecs);
        EvaporatePheromones(elapsedSecs);
    } else { /* paused or not started yet */
        SDL_SetRenderDrawColor(Renderer, 255, 0, 0, 255);
        SDL_RenderRect(Renderer, &(SDL_FRect){ Grids.pxsize, Grids.pxsize, WIN_WIDTH-(2*Grids.pxsize), WIN_HEIGHT-(2*Grids.pxsize) });
    }
    
    /* rendering the ants */
    if (ShowAnts) {
        RenderAnts();
    }

    /* rendering the nodes */
    RenderNodes();

    /* writing the text */
    SDL_snprintf(TextBuffer, 
                 TEXT_BUFFER_LEN, 
                 "INCREASE PARAMETER: [n]                    (RE)START: ENTER        RESET PARAMETERS: B            SET ALL ANTS ACTIVE: A\n"
                 "DECREASE PARAMETER: LALT+[n]         PAUSE: P                      RESET: R                                  HIDE/SHOW ANTS: H\n"
                 "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n"
                 "[1]ANT COUNT=%d   [2]EVAPAPORATION RATE=%.2f   [3]EVAPORATION INTERVAL=%.2f   [4]PHEROMONE MIN=%.2f   [5]PHEROMONE MAX=%.2f\n"
                 "[6]ALPHA=%.2f      [7]BETA=%.2f   [8]Q=%.2f   [9]SPEED=%.2f     [0]WEIGHT=%.2f\n",
                 Ants.count, EvaporationRate, EvaporationInterval, PheromoneMin, PheromoneMax, Alpha, Beta, Q, AntSpeed, Weight);
    TTF_SetTextString(Text, TextBuffer, 0);
    TTF_DrawRendererText(Text, 10.f, 5.f);

    /* render the line for adding a new edge */
    if (!AnimationRunning && SelectedNode != EMPTY) {
        float destX, destY;
        SDL_GetMouseState(&destX, &destY);
        int srcX = Nodes.centers[SelectedNode].x;
        int srcY = Nodes.centers[SelectedNode].y;
        RenderEdgeToMouse(srcX, srcY, (int)destX, (int)destY);
    }

    SDL_RenderPresent(Renderer);
    return SDL_APP_CONTINUE;
}

/* runs at startup */
SDL_AppResult SDL_AppInit(void ** appstate, int argc, char * argv[])
{
    /* create window & renderer */
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    if (!SDL_CreateWindowAndRenderer("Hangyakolonia", WIN_WIDTH, WIN_HEIGHT, 0, &Window, &Renderer)) {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* using SDL3_ttf writing text */
    if (!TTF_Init()) {
        SDL_Log("Couldn't initialize SD_TTF: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    Font        = TTF_OpenFont(FONT_PATH, 22);
    TextEngine  = TTF_CreateRendererTextEngine(Renderer);
    Text        = TTF_CreateText(TextEngine, Font, "text", 0);
    TTF_SetTextColor(Text, 0, 0, 255, 255);
    
    /* load textures */
    TextureCircle   = IMG_LoadTexture(Renderer, CIRCLE_IMG_PATH);
    TextureNest     = IMG_LoadTexture(Renderer, NEST_IMG_PATH);
    TextureFood     = IMG_LoadTexture(Renderer, FOOD_IMG_PATH);
    TextureForaging = IMG_LoadTexture(Renderer, FORAGING_IMG_PATH);
    TextureHoming   = IMG_LoadTexture(Renderer, HOMING_IMG_PATH);
    if (!TextureCircle || !TextureNest || !TextureFood || !TextureForaging || !TextureHoming) {
        SDL_Log("Texture loading failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* random seed */
    SDL_Time t = 0;
    SDL_GetCurrentTime(&t);
    SDL_srand(t);

    /* set ant colony algorithm's starting parameters */
    ResetBaseAlgorithmParams();

    /* initialize graph; InitializeAnts() and InitializePaths() gets called in SDL_AppEvent */
    Initialize();

    return SDL_APP_CONTINUE;
}

/* event handler function */
SDL_AppResult SDL_AppEvent(void * appstate, SDL_Event * event)
{
    switch (event->type) {
        case SDL_EVENT_QUIT: return SDL_APP_SUCCESS;
        case SDL_EVENT_DROP_FILE: /* drag & drop saved graph to load */
            Reset();
            const char * path = event->drop.data;
            if (LoadGraph(path)) SDL_Log("Graph successfully loaded from %s", path);
            else SDL_Log("Failed to load file: %s", path);
            break;
        case SDL_EVENT_KEY_DOWN: {
            const bool * kbs = SDL_GetKeyboardState(NULL);
            switch (event->key.scancode) {
                case SDL_SCANCODE_RETURN: 
                    if (!AnimationRunning) { /* run animation */
                        bool validgraph = true;
                        if (Nest == EMPTY) {
                            SDL_Log("Set the Nest nodes (Food is optional).\n");
                            validgraph = false;
                        } else if (Nodes.esizes[Nest] < 1) {
                            SDL_Log("Nest has no exit.\n");
                            validgraph = false;
                        }
                        
                        if (Ants.count < 1) {
                            SDL_Log("Invalid ant count.\n");
                            validgraph = false;
                        }

                        if (validgraph) {
                            SaveGraph();                /* saving graph */
                            InitializePaths();          /* allocate ants' paths */
                            InitializeAnts();           /* initialize number of ants */
                            AnimationRunning = true;    /* set AnimationRunning flag on */
                            GraphModifiable = false;    /* set GraphModifiable flag off */
                        }
                    } else { /* if running, then Restart */ Restart(); } 
                    break;
                case SDL_SCANCODE_R: Reset(); break;
                case SDL_SCANCODE_B: ResetBaseAlgorithmParams(); break;
                case SDL_SCANCODE_H: ToggleAntsRender(); break;
                case SDL_SCANCODE_A: 
                    if (AnimationRunning) {
                        SetAllAntsActive();
                    } 
                    break;
                case SDL_SCANCODE_P: 
                    if (!GraphModifiable) {
                        Pause(); 
                    }
                    break;
                case SDL_SCANCODE_1:
                    if (!AnimationRunning) {
                        if (kbs[SDL_SCANCODE_LALT] && Ants.count > 1)   Ants.count--;
                        else if (Ants.count < MAX - 1)                  Ants.count++;
                    }
                    break;
                case SDL_SCANCODE_2: 
                    if (kbs[SDL_SCANCODE_LALT] && EvaporationRate > 0.02f)  EvaporationRate -= 0.01f;
                    else if (EvaporationRate < 0.99f)                        EvaporationRate += 0.01f;
                    break;
                case SDL_SCANCODE_3: 
                    if (kbs[SDL_SCANCODE_LALT] && EvaporationInterval > 0.2f)   EvaporationInterval -= 0.1f;
                    else                                                        EvaporationInterval += 0.1f;
                    break;
                case SDL_SCANCODE_4: 
                    if (kbs[SDL_SCANCODE_LALT] && PheromoneMin > 0.2f)  PheromoneMin -= 0.1f;
                    else if (PheromoneMin < PheromoneMax - 0.2f)        PheromoneMin += 0.1f;
                    break;
                case SDL_SCANCODE_5: 
                    if (kbs[SDL_SCANCODE_LALT] && PheromoneMax > PheromoneMin + 0.2f)   PheromoneMax -= 0.1f;
                    else                                                                PheromoneMax += 0.1f;
                    break;
                case SDL_SCANCODE_6: 
                    if (kbs[SDL_SCANCODE_LALT])                         Alpha -= 0.01f;
                    else                                                Alpha += 0.01f;
                    break;
                case SDL_SCANCODE_7: 
                    if (kbs[SDL_SCANCODE_LALT])                         Beta -= 0.01f;
                    else                                                Beta += 0.01f;
                    break;
                case SDL_SCANCODE_8: 
                    if (kbs[SDL_SCANCODE_LALT] && Q > 0.2)              Q -= 0.1f;
                    else                                                Q += 0.1f;
                    break;
                case SDL_SCANCODE_9: 
                    if (kbs[SDL_SCANCODE_LALT] && AntSpeed > 2.0f)      AntSpeed -= 1.0f;
                    else                                                AntSpeed += 1.0f;
                    break;
                case SDL_SCANCODE_0: {
                    float step = 0.01f;
                    float min = 1.0f;
                    if (kbs[SDL_SCANCODE_LALT]) {
                        if (Weight - step >= min)                       Weight -= step;
                    } else                                              Weight += step;
                    break;
                }
                default:
                    break;
            }
            break;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN: { /* creating the graph */
            coord_t click = (coord_t) { event->button.x, event->button.y };
            id selection = SearchNodeInArea(click.x, click.y, CIRCLE_RAD*CIRCLE_RAD);
            switch (event->button.button) {
                case SDL_BUTTON_LEFT: /* try to add node or edge */
                    if (GraphModifiable) {
                        if (selection != EMPTY && SelectedNode != EMPTY) {
                            if (selection == SelectedNode) {
                                SelectedNode = EMPTY;
                            } else {
                                AddNewEdge(SelectedNode, selection);
                                SelectedNode = EMPTY;
                            }
                        } else if (selection != EMPTY && SelectedNode == EMPTY) {
                            SelectedNode = selection;
                        } else if (selection == EMPTY && SelectedNode != EMPTY) {
                            SelectedNode = EMPTY;
                        } else if (selection == EMPTY && SelectedNode == EMPTY) {
                            AddNewNode(click.x, click.y);
                        }
                    }
                    break;
                case SDL_BUTTON_RIGHT: /* add or remove nest and food */
                    if (selection != EMPTY) {
                        if (selection == Nest && GraphModifiable) { 
                            Nest = EMPTY; 
                        } else if (selection == Food) {
                            Food = EMPTY;
                        } else if (Nest == EMPTY && GraphModifiable) { 
                            Nest = selection; 
                        } else if (Food == EMPTY && selection != Nest) { 
                            Food = selection; 
                        }
                    }
                    break;
            }
            break;
        }
    }

    return SDL_APP_CONTINUE;
}

/* runs at shutdown */
void SDL_AppQuit(void * appstate, SDL_AppResult result) { /*SDL automatically cleans up window/renderer*/ }

/**********************************************/
/************ Rendering functions *************/
/**********************************************/
void RenderNodes(void) {
    for (int i = 0; i < Nodes.size; i++) {
#ifdef DEBUG
        RenderDebugCircle(Nodes.centers[i].x, Nodes.centers[i].y);
#endif
        int x = Nodes.centers[i].x - CIRCLE_RAD; /* shifting coordinates to top left */
        int y = Nodes.centers[i].y - CIRCLE_RAD;
        SDL_RenderTexture(Renderer, TextureCircle, NULL, &(SDL_FRect){ x, y, CIRCLE_SIZE, CIRCLE_SIZE });
    }
}

void RenderAnts(void) {
    for (int i = 0; i < Ants.actives; i++) {
        coord_t src  = Nodes.centers[Ants.colony[i].src];
        coord_t dest = Nodes.centers[Ants.colony[i].dest];

        int x = src.x + (dest.x - src.x) * Ants.colony[i].progress - ANT_RAD;
        int y = src.y + (dest.y - src.y) * Ants.colony[i].progress - ANT_RAD;
        SDL_Texture * texture = Ants.colony[i].foraging ? TextureForaging : TextureHoming;
        SDL_RenderTexture(Renderer, texture, NULL, &(SDL_FRect){ x, y, ANT_SIZE, ANT_SIZE });
    }
}

void RenderNest(void) {
    if (Nest == EMPTY || Nest >= Nodes.size) return;
    float x = Nodes.centers[Nest].x - CIRCLE_RAD * 1.5f;
    float y = Nodes.centers[Nest].y - CIRCLE_RAD * 1.5f;
    SDL_RenderTexture(Renderer, TextureNest, NULL, &(SDL_FRect){ x, y, CIRCLE_RAD * 3, CIRCLE_RAD * 3 });
}

void RenderFood(void) {
    if (Food == EMPTY || Food >= Nodes.size) return;
    float x = Nodes.centers[Food].x - CIRCLE_RAD * 1.5f;
    float y = Nodes.centers[Food].y - CIRCLE_RAD * 1.5f;
    SDL_RenderTexture(Renderer, TextureFood, NULL, &(SDL_FRect){ x, y, CIRCLE_RAD * 3, CIRCLE_RAD * 3 });
}

void RenderEdges(void) {
    if (!Edges.size) return;
    /* Update the width of the edges by their pheromone values */
    for (int e = 0; e < Edges.size; e++) {
        float ratio = ((Edges.pheromones[e] - PheromoneMin) / (PheromoneMax - PheromoneMin));
        ratio = ratio < 0.0f ? 0.0f : ratio > 1.0f ? 1.0f : ratio;

        float oldWidth = Edges.widths[e];
        float newWidth = MIN_EDGE_WIDTH + ratio * (MAX_EDGE_WIDTH - MIN_EDGE_WIDTH);
        if (SDL_fabsf(oldWidth - newWidth) > 0.001f) {
            Edges.widths[e] = newWidth;

            /* calculate new vertices for the edge */
            id anode = Edges.anodes[e];
            id bnode = Edges.bnodes[e];
            int aX = Nodes.centers[anode].x;
            int aY = Nodes.centers[anode].y;
            int bX = Nodes.centers[bnode].x;
            int bY = Nodes.centers[bnode].y;
            float dX = bX - aX;
            float dY = bY - aY;
            float length = Edges.lengths[e];
            float pX = (-dY / length) * (newWidth / 2);
            float pY = ( dX / length) * (newWidth / 2);
            int vstart = e * 4;
            Edges.verts[vstart + 0].position = (SDL_FPoint){ aX + pX, aY + pY };
            Edges.verts[vstart + 1].position = (SDL_FPoint){ bX + pX, bY + pY };
            Edges.verts[vstart + 2].position = (SDL_FPoint){ bX - pX, bY - pY };
            Edges.verts[vstart + 3].position = (SDL_FPoint){ aX - pX, aY - pY };
        }
    }

    SDL_RenderGeometry(Renderer, NULL, Edges.verts, Edges.size * 4, Edges.vidxs, Edges.size * 6);
}

void RenderEdgeToMouse(int aX, int aY, int bX, int bY) {
    float dx  = bX - aX;
    float dy  = bY - aY;
    float len = SDL_sqrt(dx * dx + dy * dy);
    float pX  = (-dy / len) * MIN_EDGE_WIDTH;
    float pY  = ( dx / len) * MIN_EDGE_WIDTH;

    SDL_FColor color = (SDL_FColor) { 0.f, 0.f, 1.f, 1.f };
    SDL_Vertex verts[] = {
        { .position = { aX + pX, aY + pY }, .color = color },
        { .position = { bX + pX, bY + pY }, .color = color },
        { .position = { bX - pX, bY - pY }, .color = color },
        { .position = { aX - pX, aY - pY }, .color = color }
    };
    int indices[] = { 0, 1, 2, 0, 2, 3 };

    SDL_RenderGeometry(Renderer, NULL, verts, 4, indices, 6);
}

void RenderDebugCircle(int x, int y) {
    int radius = CIRCLE_RAD * 4;
    int offsetX = radius - 1;
    int offsetY = 0;
    int d = 1 - radius;
    while (offsetX >= offsetY) {
        SDL_RenderPoint(Renderer, x + offsetX, y - offsetY);
        SDL_RenderPoint(Renderer, x + offsetY, y - offsetX);
        SDL_RenderPoint(Renderer, x - offsetY, y - offsetX);
        SDL_RenderPoint(Renderer, x - offsetX, y - offsetY);
        SDL_RenderPoint(Renderer, x - offsetX, y + offsetY);
        SDL_RenderPoint(Renderer, x - offsetY, y + offsetX);
        SDL_RenderPoint(Renderer, x + offsetY, y + offsetX);
        SDL_RenderPoint(Renderer, x + offsetX, y + offsetY);
        offsetY++;
        if (d <= 0) {
            d += 2 * offsetY + 1;
        } else {
            offsetX--;
            d += 2 * (offsetY - offsetX) + 1;
        }
    }
}

/**********************************************/
/********* Memory handling functions **********/
/**********************************************/
/* Graph's nodes */
void InitializeNodes(void) {
    int cap = 32;
    Nodes.capacity     = cap;
    Nodes.size         = 0;
    Nodes.centers      = SDL_malloc(cap * sizeof(*Nodes.centers));
    Nodes.edges        = SDL_malloc(cap * sizeof(*Nodes.edges));
    Nodes.ecapacities  = SDL_malloc(cap * sizeof(*Nodes.ecapacities));
    Nodes.esizes       = SDL_malloc(cap * sizeof(*Nodes.esizes));
    if (!Nodes.centers || !Nodes.edges || !Nodes.ecapacities || !Nodes.esizes) {
        SDL_Log("Memory allocation failed at line %d.\n", __LINE__);
        exit(1);
    }

    int edgecap = 8;
    for (int i = 0; i < cap; i++) {
        Nodes.ecapacities[i]   = edgecap;
        Nodes.esizes[i]        = 0;
        Nodes.edges[i]         = SDL_malloc(edgecap * sizeof(*Nodes.edges[i]));
        if (!Nodes.edges[i]) {
            SDL_Log("Memory allocation failed at line %d.\n", __LINE__);
            exit(1);
        }
    }
}

void FreeNodes(void) {
    for (int i = 0; i < Nodes.capacity; SDL_free(Nodes.edges[i++]));
    SDL_free(Nodes.centers);
    SDL_free(Nodes.edges);
    SDL_free(Nodes.ecapacities);
    SDL_free(Nodes.esizes);
}

void AddNewNode(int x, int y) {
    int min_xy = Grids.pxsize + CIRCLE_RAD;
    int max_x  = WIN_WIDTH  - Grids.pxsize - CIRCLE_RAD;
    int max_y  = WIN_HEIGHT - Grids.pxsize - CIRCLE_RAD;
    x = x < min_xy ? min_xy : x;
    x = x > max_x  ? max_x  : x;
    y = y < min_xy ? min_xy : y;
    y = y > max_y  ? max_y  : y;

    int dsquared = CIRCLE_SIZE * CIRCLE_SIZE * 4;
    if (SearchNodeInArea(x, y, dsquared) != EMPTY) {
        SDL_Log("Too close to another circle.\n");
        return;
    }

    int idx = Nodes.size;
    if (idx >= Nodes.capacity) {
        int cap = Nodes.capacity * 2;
        if (cap >= MAX) {
            SDL_Log("Too many nodes to allocate!\n");
            return;
        }
        Nodes.capacity = cap;
        Nodes.centers = SDL_realloc(Nodes.centers, cap * sizeof(*Nodes.centers));
        Nodes.esizes  = SDL_realloc(Nodes.esizes, cap * sizeof(*Nodes.esizes));
        Nodes.ecapacities = SDL_realloc(Nodes.ecapacities, cap * sizeof(*Nodes.ecapacities));
        Nodes.edges       = SDL_realloc(Nodes.edges, cap * sizeof(*Nodes.edges));
        if (!Nodes.centers || !Nodes.esizes || !Nodes.ecapacities || !Nodes.edges) {
            SDL_Log("Memory reallocation failed at line %d.\n", __LINE__);
            exit(1);
        }
        
        int edgecap = 8;
        for (int i = idx; i < cap; i++) {
            Nodes.ecapacities[i] = edgecap;
            Nodes.esizes[i]      = 0;
            Nodes.edges[i]       = SDL_malloc(edgecap * sizeof(*Nodes.edges[i])); 
            if (!Nodes.edges[i]) {
                SDL_Log("Memory allocation failed at line %d.\n", __LINE__);
                exit(1);
            }
        }
    }

    if (AddToGrid(x, y)) {
        Nodes.centers[idx] = (coord_t) { x, y };
        Nodes.size++;
    }
}

/* Grids for node placement */
void InitializeGrids(void) {
    int size = CIRCLE_SIZE * 2;
    Grids.pxsize   = size;
    Grids.width    = (WIN_WIDTH  + size - 1) / size;
    Grids.height   = (WIN_HEIGHT + size - 1) / size;
    Grids.capacity = Grids.width * Grids.height;
    Grids.nodes    = SDL_malloc(Grids.capacity * sizeof(*Grids.nodes));
    if (!Grids.nodes) {
        SDL_Log("Memory allocation failed at line %d.\n", __LINE__);
        exit(1);
    }
    SDL_memset(Grids.nodes, 0xFF, Grids.capacity * sizeof(*Grids.nodes));
}

void FreeGrids(void) {
    SDL_free(Grids.nodes);
}

/* Graph's edges */
void InitializeEdges(void) {
    int cap = 32;
    Edges.color        = (SDL_FColor) { 0.f, 200.f, 0.f, 255.f };
    Edges.capacity     = cap;
    Edges.size         = 0;
    Edges.verts        = SDL_malloc(cap * 4 * sizeof(*Edges.verts));
    Edges.vidxs        = SDL_malloc(cap * 6 * sizeof(*Edges.vidxs));
    Edges.widths       = SDL_malloc(cap * sizeof(*Edges.widths));
    Edges.lengths      = SDL_malloc(cap * sizeof(*Edges.lengths));
    Edges.pheromones   = SDL_malloc(cap * sizeof(*Edges.pheromones));
    Edges.anodes       = SDL_malloc(cap * sizeof(*Edges.anodes));
    Edges.bnodes       = SDL_malloc(cap * sizeof(*Edges.bnodes));
    if (!Edges.verts || !Edges.vidxs || !Edges.widths || !Edges.lengths || 
        !Edges.pheromones || !Edges.anodes || !Edges.bnodes) {
        SDL_Log("Memory allocation failed at line %d.\n", __LINE__);
        exit(1);
    }
}

void AddNewEdge(id a, id b) {
    if (a >= Nodes.size || b >= Nodes.size) {
        SDL_Log("Invalid nodes to add an edge!\n");
        return;
    }

    if (a == b) {
        SDL_Log("Same node selected.\n");
        return;
    }

    /* nodes saved in order */
    if (a > b) {
        id temp = a;
        a = b;
        b = temp;
    }

    /* checking at one node must be enough */
    for (id e = 0; e < Nodes.esizes[a]; e++) {
        id edge = Nodes.edges[a][e];
        if (Edges.bnodes[edge] == b) {
            SDL_Log("Edge already exists.\n");
            return;
        }
    }

    /* Adding the edge to the nodes */
    id esize = Nodes.esizes[a];
    id ecap  = Nodes.ecapacities[a];
    if (esize >= ecap) {
        int ecap = Nodes.ecapacities[a] * 2;
        if (ecap >= MAX) {
            SDL_Log("Too many edges to allocate!\n");
            exit(1);
        }
        Nodes.ecapacities[a] = ecap;
        Nodes.edges[a] = SDL_realloc(Nodes.edges[a], ecap * sizeof(*Nodes.edges[a]));
        if (!Nodes.edges[a]) {
            SDL_Log("Memory reallocation failed at line %d.\n", __LINE__);
            exit(1);
        }
    }
    Nodes.edges[a][esize] = Edges.size;
    Nodes.esizes[a]++;
    esize = Nodes.esizes[b];
    ecap  = Nodes.ecapacities[b];
    if (esize >= ecap) {
        int ecap = Nodes.ecapacities[b] * 2;
        if (ecap >= MAX) {
            SDL_Log("Too many edges to allocate!\n");
            exit(1);
        }
        Nodes.ecapacities[b] = ecap;
        Nodes.edges[b] = SDL_realloc(Nodes.edges[b], ecap * sizeof(*Nodes.edges[b]));
        if (!Nodes.edges[b]) {
            SDL_Log("Memory reallocation failed at line %d.\n", __LINE__);
            exit(1);
        }
    }
    Nodes.edges[b][esize] = Edges.size;
    Nodes.esizes[b]++;
    
    /* Adding the edge to the edges */
    id edge = Edges.size;
    if (edge >= Edges.capacity) {
        int cap = Edges.capacity * 2;
        if (cap >= MAX) {
            SDL_Log("Too many edges to allocate!\n");
            exit(1);
        }
        Edges.capacity   = cap;
        Edges.verts      = SDL_realloc(Edges.verts, cap * 4 * sizeof(*Edges.verts));
        Edges.vidxs      = SDL_realloc(Edges.vidxs, cap * 6 * sizeof(*Edges.vidxs));
        Edges.widths     = SDL_realloc(Edges.widths, cap * sizeof(*Edges.widths));
        Edges.anodes     = SDL_realloc(Edges.anodes, cap * sizeof(*Edges.anodes));
        Edges.bnodes     = SDL_realloc(Edges.bnodes, cap * sizeof(*Edges.bnodes));
        Edges.lengths    = SDL_realloc(Edges.lengths, cap * sizeof(*Edges.lengths));
        Edges.pheromones = SDL_realloc(Edges.pheromones, cap * sizeof(*Edges.pheromones));
        if (!Edges.verts || !Edges.vidxs || !Edges.widths || !Edges.anodes || !Edges.bnodes || !Edges.lengths || !Edges.pheromones) {
            SDL_Log("Memory reallocation failed at line %d.\n", __LINE__);
            exit(1);
        }
    }

    int aX = Nodes.centers[a].x;
    int aY = Nodes.centers[a].y;
    int bX = Nodes.centers[b].x;
    int bY = Nodes.centers[b].y;
    float dX = bX - aX;
    float dY = bY - aY;
    float length = SDL_sqrt(dX * dX + dY * dY);
    float pX = (-dY / length) * (MIN_EDGE_WIDTH / 2);
    float pY = ( dX / length) * (MIN_EDGE_WIDTH / 2);
    
    SDL_Vertex v0 = { .position = { aX + pX, aY + pY }, .color = Edges.color };
    SDL_Vertex v1 = { .position = { bX + pX, bY + pY }, .color = Edges.color };
    SDL_Vertex v2 = { .position = { bX - pX, bY - pY }, .color = Edges.color };
    SDL_Vertex v3 = { .position = { aX - pX, aY - pY }, .color = Edges.color };    

    int vstart = edge * 4;
    Edges.verts[vstart + 0] = v0;
    Edges.verts[vstart + 1] = v1;
    Edges.verts[vstart + 2] = v2;
    Edges.verts[vstart + 3] = v3;   
    
    Edges.vidxs[edge * 6 + 0] = vstart + 0;
    Edges.vidxs[edge * 6 + 1] = vstart + 1;
    Edges.vidxs[edge * 6 + 2] = vstart + 2;
    Edges.vidxs[edge * 6 + 3] = vstart + 0;
    Edges.vidxs[edge * 6 + 4] = vstart + 2;
    Edges.vidxs[edge * 6 + 5] = vstart + 3;
    
    Edges.widths[edge]     = MIN_EDGE_WIDTH;
    Edges.anodes[edge]     = a;
    Edges.bnodes[edge]     = b;
    Edges.lengths[edge]    = length;
    Edges.pheromones[edge] = PheromoneMin;
    Edges.size++;
    SDL_Log("New Edge added. Edge[%d]. Anode=%d Bnode=%d Length=%.2f\n", edge, Edges.anodes[edge], Edges.bnodes[edge], Edges.lengths[edge]);
}

void FreeEdges(void) {
    SDL_free(Edges.verts);
    SDL_free(Edges.vidxs);
    SDL_free(Edges.widths);
    SDL_free(Edges.lengths);
    SDL_free(Edges.pheromones);
    SDL_free(Edges.anodes);
    SDL_free(Edges.bnodes);
}

/* Ants */
void InitializeAnts() { 
    //Ants.count initialized beforehand in Initialize() function
    Ants.actives = 0;
    Ants.colony  = SDL_malloc(Ants.count * sizeof(*Ants.colony));
    if (!Ants.colony) {
        SDL_Log("Memory allocation failed at line %d.\n", __LINE__);
        exit(1);
    }
    Ants.probabilitiesBuffer = SDL_malloc((Edges.size + 1) * sizeof(*Ants.probabilitiesBuffer)) ;
    Ants.edgesBuffer = SDL_malloc((Edges.size + 1) * sizeof(*Ants.edgesBuffer));
    if (!Ants.probabilitiesBuffer || !Ants.edgesBuffer) {
        SDL_Log("Memory reallocation failed at line %d.\n", __LINE__);
        exit(1);
    }

    for (id a = 0; a < Ants.count; a++) ResetBaseAntParams(a);
}

void FreeAnts(void) {
    if (!Ants.colony) SDL_free(Ants.colony);
    if (!Ants.probabilitiesBuffer) SDL_free(Ants.probabilitiesBuffer);
    if (!Ants.edgesBuffer) SDL_free(Ants.edgesBuffer);
}

/* Paths - runs only after the graph has been created */
void InitializePaths(void) {
    int size  = Nodes.size;
    int count = Ants.count;
    Paths.chunksize = size;
    Paths.nodes     = SDL_malloc(size * count * sizeof(*Paths.nodes));
    Paths.edges     = SDL_malloc(size * count * sizeof(*Paths.edges));
    if (!Paths.nodes || !Paths.edges) {
        SDL_Log("Memory allocation failed at line %d.\n", __LINE__);
        exit(1);
    }
}

void FreePaths(void) {
    if (!Paths.nodes) SDL_free(Paths.nodes);
    if (!Paths.edges) SDL_free(Paths.edges);
}

/**********************************************/
/************* Helper functions ***************/
/**********************************************/
static void Initialize(void) {
    InitializeNodes();
    InitializeGrids();
    InitializeEdges();

    Nest = EMPTY;
    Food = EMPTY;
    AnimationRunning = false;
    GraphModifiable = true;
    SelectedNode = EMPTY;

    Ants.actives = 0;
    Ants.count = 1;
}

static void Restart(void) {
    FreePaths();
    FreeAnts();

    InitializePaths();
    InitializeAnts();

    for (id e = 0; e < Edges.size; e++) /* reset pheromones */
        Edges.pheromones[e] = PheromoneMin;
}

static void Pause(void) {
    AnimationRunning = !AnimationRunning;
    if (AnimationRunning)
        LastTime = SDL_GetTicks();
}

static void Reset(void) {
    FreeNodes();
    FreeEdges();
    FreeGrids();
    FreePaths();
    FreeAnts();

    Initialize();
}

static void SetAllAntsActive(void) {
    if (Ants.actives < Ants.count) Ants.actives = Ants.count;
    SDL_Log("All ants are active.\n");
}

static void ToggleAntsRender(void) {
    ShowAnts ^= 1;
}

/* Checks is there a node in the area, returns its idx, or EMPTY */
static id SearchNodeInArea(int x, int y, int area) {
    if (x < Grids.pxsize || x > WIN_WIDTH - Grids.pxsize || y < Grids.pxsize || y > WIN_HEIGHT - Grids.pxsize) {
       return EMPTY;
    }

    int g = (y / Grids.pxsize) * Grids.width + x / Grids.pxsize;
    for (int r = -1; r <= 1; r++) {
        for (int c = -1; c <= 1; c++) {
            int ng = g + (r * Grids.width) + c;
            for (int i = 0; i < 4; i++) {
                id node = Grids.nodes[ng][i];
                if (node != EMPTY) {
                    int dx = Nodes.centers[node].x - x;
                    int dy = Nodes.centers[node].y - y;
                    int distance = dx * dx + dy * dy;
                    if (distance <= area) {
                        return node;
                    }
                }
            }
        }
    }
    return EMPTY;
}

/* Adds the current node to the grid of the pos */
static bool AddToGrid(int x, int y) {
    int g = (y / Grids.pxsize) * Grids.width + x / Grids.pxsize;
    for (int i = 0; i < 4; i++) {
        if (Grids.nodes[g][i] == EMPTY) { 
            Grids.nodes[g][i] = Nodes.size;
            return true;
        }
    }
    return false;
}

/**********************************************/
/********* Saving and loading graph ***********/
/**********************************************/
static void SaveGraph(void) {
    size_t size = (Nodes.size + Edges.size) * 32;
    char * buffer = SDL_malloc(size);
    if (!buffer) {
        SDL_Log("Memory allocation failed at line %d.\n", __LINE__);
        exit(1);
    }

    size_t p = 0;
    for (id n = 0; n < Nodes.size; n++)
        p += SDL_snprintf(buffer + p, size - p, "N %d %d\n", Nodes.centers[n].x, Nodes.centers[n].y);
    for (int e = 0; e < Edges.size; e++)
        p += SDL_snprintf(buffer + p, size - p, "E %d %d\n", Edges.anodes[e], Edges.bnodes[e]);

    p += SDL_snprintf(buffer + p, size - p, "-\n%hu\n%hu\n%d\n%.2f\n%.2f\n%.2f\n%.2f\n%.2f\n%.2f\n%.2f\n%.2f\n", 
                     Nest, Food, Ants.count, 
                     EvaporationRate, EvaporationInterval, PheromoneMin, PheromoneMax, 
                     Alpha, Beta, Q, AntSpeed);

    char outputPath[64];
    SDL_snprintf(outputPath, 64, "GRAPH%07llu.txt", SDL_GetTicks());

    if (!SDL_SaveFile(outputPath, buffer, p)) {
        SDL_Log("Saving graph failed: %s\n", SDL_GetError());
    } else {
        SDL_Log("Graph saved as %s", outputPath);
    }
    SDL_free(buffer);
}

static bool LoadGraph(const char * path) {
    size_t size = 0;
    char * data = SDL_LoadFile(path, &size);

    if (!size) {
        SDL_Log("No bytes loaded from the file. %s", SDL_GetError());
        SDL_free(data);
        return false;
    }

    char * l = data;
    char * end = data + size;

    while (l < end) {
        char * next = SDL_strchr(l, '\n');
        if (next) 
            *next = '\0';

        if (*l == 'N') {
            int x, y;
            if (SDL_sscanf(l + 1, "%d %d", &x, &y) == 2) AddNewNode(x, y);
        } else if (*l == 'E') {
            int a, b;
            if (SDL_sscanf(l + 1, "%d %d", &a, &b) == 2) AddNewEdge(a, b);
        } else if (*l == '-') {
            l = next + 1;
            break;
        } else {
            SDL_Log("Invalid graph file.\n");
            SDL_free(data);
            return false;
        }

        l = next + 1;
    }

    if (SDL_sscanf(l, "%hu\n%hu\n%d\n%f\n%f\n%f\n%f\n%f\n%f\n%f\n%f\n", 
                     &Nest, &Food, &Ants.count, 
                     &EvaporationRate, &EvaporationInterval, &PheromoneMin, &PheromoneMax, 
                     &Alpha, &Beta, &Q, &AntSpeed) != 11) {
        SDL_Log("Invalid graph file.\n");
        SDL_free(data);
        return false;
    }

    SDL_free(data);
    return true;
}

