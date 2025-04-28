#include <global.h>

float EvaporationRate;
float EvaporationInterval;
float Alpha;
float Beta;
float Q;
float AntSpeed;
float PheromoneMin;
float PheromoneMax;

static float evaporationTimer = 0.0f;

/* helper functions */
static inline id   SelectEdgeAtNode(id, id);
static inline void DepositPheromone(id, id);
static inline void Homing(id);
static inline void Foraging(id);
static inline void ForagingGetNext(id);
static inline id GetOtherNodeOnEdge(id, id);
static inline int GetPathStart(id);

void UpdateAnts(float elapsedSecs) {
    for (id a = 0; a < Ants.actives; a++) {
        if (Ants.colony[a].progress >= 1.0f) { /* arrived to a node */
            Ants.colony[a].src = Ants.colony[a].dest;
            if (Ants.colony[a].foraging) Foraging(a); 
            else Homing(a);
        
        } else { /* on edge */
            float length = Edges.lengths[Ants.colony[a].edge];
            Ants.colony[a].progress += (elapsedSecs * AntSpeed) / length;
        }
    }
}

void ResetBaseAntParams(id a) {
    Ants.colony[a].progress     = 1.0f;
    Ants.colony[a].pathlength   = 0.0f;
    Ants.colony[a].src          = Nest;
    Ants.colony[a].dest         = Nest;
    Ants.colony[a].edge         = EMPTY;
    Ants.colony[a].pathidx      = 0;
    Ants.colony[a].foraging     = true;
    Ants.colony[a].TTL          = Nodes.size * 2;
}

void ResetBaseAlgorithmParams(void) {
    EvaporationRate     = 0.1f;   
    EvaporationInterval = 1.0f; /* seconds */
    Alpha               = 1.0f;   
    Beta                = 2.0f;
    Q                   = 10.0f;
    AntSpeed            = 100.0f;
    PheromoneMin        = 0.1f;
    PheromoneMax        = 15.0f;
}

/* updating the edges' pheromone values */
void EvaporatePheromones(float elapsedSecs) {
    evaporationTimer += elapsedSecs;
    if (evaporationTimer >= EvaporationInterval) {
        for (id e = 0; e < Edges.size; e++) {
            float p = Edges.pheromones[e] * (1.0f - EvaporationRate);
            p = p < PheromoneMin ? PheromoneMin : p > PheromoneMax ? PheromoneMax : p;
            Edges.pheromones[e] = p;
        }
        evaporationTimer -= EvaporationInterval;
    }
}

/* picking next edge by probability distribution, exclude source edge if possible */ 
static inline id SelectEdgeAtNode(id node, id prevEdge) {
    id count   = Nodes.esizes[node];
    id * edges = Nodes.edges[node];
    
    int b = 0;
    float totalProbability = 0.0f;
    for (int i = 0; i < count; i++) {
        id e = edges[i];
        if (e == prevEdge) { /* exclude source edge by default */
            continue;
        }

        /* calculate the probabilities with the Ant Colony algorithm for every edge */
        float pheromone = SDL_powf(Edges.pheromones[e], Alpha);
        float heuristic = SDL_powf((1.0f / Edges.lengths[e]), Beta);
        float probability = pheromone * heuristic;

        Ants.probabilitiesBuffer[b] = probability;
        Ants.edgesBuffer[b] = e;
        totalProbability += probability;
        b++;
    }

#ifdef DEBUG
    SDL_Log("Total probabilites = %f\n", totalProbability);
    for (int i = 0; i < b; i++) { SDL_Log("\t[%d] edge's probability = %f\n", Ants.edgesBuffer[i], Ants.probabilitiesBuffer[i]); }
#endif

    float r = SDL_randf() * totalProbability;
    float rsum = 0.0f;
    id selected = edges[0];
    for (int i = 0; i < b; i++) {
        rsum += Ants.probabilitiesBuffer[i];
        if (r <= rsum) {
            selected = Ants.edgesBuffer[i];
            break;
        }
    }

#ifdef DEBUG
    SDL_Log("Selected edge = [%d] (r = %f, rsum = %f)\n", selected, r, rsum);
#endif

    return selected;
}

static inline void DepositPheromone(id edge, id ant) {
    float value = Q / Ants.colony[ant].pathlength;
    Edges.pheromones[edge] += value;
}

static inline void Homing(id a) {
    int p = GetPathStart(a) + Ants.colony[a].pathidx;
    id e  = Paths.edges[p];
    Ants.colony[a].edge = e;

    if (Ants.colony[a].pathidx == 0) { /* finished backtracking */
        if (Ants.colony[a].src == Nest) {
            ResetBaseAntParams(a);
        } else { /* finished backtracking - travel last edge to the nest */
            DepositPheromone(e, a);
            Ants.colony[a].dest = Nest;
            Ants.colony[a].progress = 0.0f;
        }
    } else { /* backtracking */
        DepositPheromone(e, a);
        Ants.colony[a].dest = Paths.nodes[p - 1];
        Ants.colony[a].pathidx--;
        Ants.colony[a].progress = 0.0f;
    }
}

static inline void ForagingGetNext(id a) {
    if (--Ants.colony[a].TTL <= 0) {
        ResetBaseAntParams(a);
        return;
    }
    
    id n = Ants.colony[a].src;
    id nextEdge = SelectEdgeAtNode(n, Ants.colony[a].edge);
    id nextDest = GetOtherNodeOnEdge(nextEdge, n);

    Ants.colony[a].edge = nextEdge;
    Ants.colony[a].dest = nextDest;
    Ants.colony[a].pathlength += Edges.lengths[nextEdge];

    int p = GetPathStart(a) + Ants.colony[a].pathidx++;

    Paths.edges[p] = nextEdge;
    Paths.nodes[p] = nextDest;

    Ants.colony[a].progress = 0.0f;
}

static inline void Foraging(id a) {
    id n = Ants.colony[a].src;
    if (n == Nest) {
        if (Ants.colony[a].pathidx == 0) { /* new path start */
            Ants.colony[a].edge = EMPTY;
            ForagingGetNext(a);
        } else { /* returned back without finding Food */
            Ants.colony[a].pathidx = 0;
            Ants.colony[a].pathlength = 0;
        }
    } else if (n == Food) {
        Ants.colony[a].foraging = false;
        Ants.colony[a].pathidx--;
    } else { /* at other node */
        float newLength = 0.f; /* unloop */
        int start = GetPathStart(a);
        for (id i = 0; i < Ants.colony[a].pathidx; i++) { 
            int p = start + i;
            id e = Paths.edges[p];
            newLength += Edges.lengths[e];
            if (Ants.colony[a].src == Paths.nodes[p]) {
                Ants.colony[a].pathlength = newLength;
                Ants.colony[a].edge = e;
                Ants.colony[a].pathidx = ++i;
                break;
            }
        }
        ForagingGetNext(a);
    }
}

static inline id GetOtherNodeOnEdge(id edge, id node) {
    return (Edges.anodes[edge] == node) ? Edges.bnodes[edge] : Edges.anodes[edge];
}

static inline int GetPathStart(id ant) {
    return ant * Paths.chunksize;
}
