#ifndef GLOBAL_H 
#define GLOBAL_H

#include <SDL3/SDL.h>

#define CIRCLE_IMG_PATH     "./src/circle.png"
#define FOOD_IMG_PATH       "./src/circle_outline_yellow.png"
#define NEST_IMG_PATH       "./src/circle_outline_red.png"
#define FORAGING_IMG_PATH   "./src/ant.png"
#define HOMING_IMG_PATH     "./src/ant_yellow.png"
#define FONT_PATH           "./src/dejavusans.ttf"

#define MAX                 (0xFFFF)
#define EMPTY               (MAX)
#define WIN_WIDTH           1280
#define WIN_HEIGHT          960
#define CIRCLE_SIZE         ((int)(WIN_HEIGHT / 32))
#define CIRCLE_RAD          (CIRCLE_SIZE / 2)
#define MIN_EDGE_WIDTH      2
#define MAX_EDGE_WIDTH      CIRCLE_SIZE
#define ANT_SIZE            ((int)(CIRCLE_SIZE / 2))
#define ANT_RAD             (ANT_SIZE / 2)

//#define DEBUG 

typedef uint16_t id;
typedef struct { int x; int y; } coord_t;

struct nodes_s {
    coord_t    * centers;
    id        ** edges;
    id         * ecapacities;
    id         * esizes;
    id           capacity;
    id           size;
};

struct edges_s {
    SDL_FColor   color;
    SDL_Vertex * verts;
    int        * vidxs;
    float      * widths;
    float      * lengths;
    float      * pheromones;
    id         * anodes;
    id         * bnodes;
    id           capacity;
    id           size;
};

struct paths_s {
    id         * nodes;
    id         * edges;
    id           chunksize;
};

struct ants_s {
    struct {
        float    progress;
        float    pathlength;
        int      TTL;
        id       src;
        id       dest;
        id       edge;
        id       pathidx;
        bool     foraging;
    } * colony;
    float   * probabilitiesBuffer;
    id      * edgesBuffer;
    int count;
    int actives;
};

struct grids_s {
    id  (*nodes)[4];
    int pxsize;
    int width;
    int height;
    int capacity;
};

extern struct edges_s   Edges;
extern struct nodes_s   Nodes;
extern struct ants_s    Ants;
extern struct paths_s   Paths;
extern struct grids_s   Grids;
extern id Nest;
extern id Food;
extern float EvaporationRate;
extern float EvaporationInterval;
extern float Alpha;
extern float Beta;
extern float Q;
extern float AntSpeed;
extern float PheromoneMin;
extern float PheromoneMax;

/* memory handling functions */
void InitializeNodes(void);
void InitializeGrids(void);
void InitializeEdges(void);
void InitializeAnts(void);
void InitializePaths(void);
void AddNewNode(int x, int y);
void AddNewEdge(id a, id b);
bool AddToGrid(int x, int y);
void FreeNodes(void);
void FreeGrids(void);
void FreeEdges(void);
void FreeAnts(void);
void FreePaths(void);
id SearchNodeInArea(int x, int y, int area);

/* rendering functions */
void RenderNodes(void);
void RenderAnts(void);
void RenderNest(void);
void RenderFood(void);
void RenderEdges(void);
void RenderEdgeToMouse(int aX, int aY, int bX, int bY);
void RenderDebugCircle(int x, int y);

/* ant colony algorithm's functions */
void UpdateAnts(float elapsedSecs);
void EvaporatePheromones(float elapsedSecs);
void ResetBaseAlgorithmParams(void);
void ResetBaseAntParams(id ant);

#endif //GLOBAL_H
