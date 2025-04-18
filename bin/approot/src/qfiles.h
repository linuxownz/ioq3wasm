#define MAX_QPATH 64

#define LINE printf("%s %d\n", __func__, __LINE__)

typedef unsigned char byte;

typedef float vec_t;
typedef vec_t vec3_t[3];

typedef struct {
    int     fileofs, filelen;
} lump_t;

typedef enum {qfalse, qtrue} qboolean;

#define BOX_BRUSHES     1
#define BOX_SIDES       6
#define BOX_LEAFS       2
#define BOX_PLANES      12

#define LUMP_ENTITIES       0
#define LUMP_SHADERS        1
#define LUMP_PLANES         2
#define LUMP_NODES          3
#define LUMP_LEAFS          4
#define LUMP_LEAFSURFACES   5
#define LUMP_LEAFBRUSHES    6
#define LUMP_MODELS         7
#define LUMP_BRUSHES        8
#define LUMP_BRUSHSIDES     9
#define LUMP_DRAWVERTS      10
#define LUMP_DRAWINDEXES    11
#define LUMP_FOGS           12
#define LUMP_SURFACES       13
#define LUMP_LIGHTMAPS      14
#define LUMP_LIGHTGRID      15
#define LUMP_VISIBILITY     16
#define HEADER_LUMPS        17

typedef struct {
    int         ident;
    int         version;

    lump_t      lumps[HEADER_LUMPS];
} dheader_t;

typedef struct {
    float       mins[3], maxs[3];
    int         firstSurface, numSurfaces;
    int         firstBrush, numBrushes;
} dmodel_t;

typedef struct {
    char        shader[MAX_QPATH];
    int         surfaceFlags;
    int         contentFlags;
} dshader_t;

// planes x^1 is allways the opposite of plane x

typedef struct {
    float       normal[3];
    float       dist;
} dplane_t;

typedef struct {
    int         planeNum;
    int         children[2];    // negative numbers are -(leafs+1), not nodes
    int         mins[3];        // for frustom culling
    int         maxs[3];
} dnode_t;

typedef struct {
    int         cluster;            // -1 = opaque cluster (do I still store these?)
    int         area;

    int         mins[3];            // for frustum culling
    int         maxs[3];

    int         firstLeafSurface;
    int         numLeafSurfaces;

    int         firstLeafBrush;
    int         numLeafBrushes;
} dleaf_t;

typedef struct {
    int         planeNum;           // positive plane side faces out of the leaf
    int         shaderNum;
} dbrushside_t;

typedef struct {
    int         firstSide;
    int         numSides;
    int         shaderNum;      // the shader that determines the contents flags
} dbrush_t;

typedef struct {
    char        shader[MAX_QPATH];
    int         brushNum;
    int         visibleSide;    // the brush side that ray tests need to clip against (-1 == none)
} dfog_t;

typedef struct {
    vec3_t      xyz;
    float       st[2];
    float       lightmap[2];
    vec3_t      normal;
    byte        color[4];
} drawVert_t;

#define MAX_SUBMODELS           256
#define BOX_MODEL_HANDLE        255
#define CAPSULE_MODEL_HANDLE    254

typedef struct cplane_s {
    vec3_t  normal;
    float   dist;
    byte    type;           // for fast side tests: 0,1,2 = axial, 3 = nonaxial
    byte    signbits;       // signx + (signy<<1) + (signz<<2), used as lookup during collision
    byte    pad[2];
} cplane_t;

typedef struct {
    cplane_t    *plane;
    int         children[2];        // negative numbers are leafs
} cNode_t;

typedef struct {
    int         cluster;
    int         area;

    int         firstLeafBrush;
    int         numLeafBrushes;

    int         firstLeafSurface;
    int         numLeafSurfaces;
} cLeaf_t;

typedef struct cmodel_s {
    vec3_t      mins, maxs;
    cLeaf_t     leaf;           // submodels don't reference the main tree
} cmodel_t;

typedef struct {
    cplane_t    *plane;
    int         surfaceFlags;
    int         shaderNum;
} cbrushside_t;

typedef struct {
    int         shaderNum;      // the shader that determined the contents
    int         contents;
    vec3_t      bounds[2];
    int         numsides;
    cbrushside_t    *sides;
    int         checkcount;     // to avoid repeated testings
} cbrush_t;

#define BSP_VERSION         46
#define BSP_IDENT   (('P'<<24)+('S'<<16)+('B'<<8)+'I')

typedef struct {
    int         floodnum;
    int         floodvalid;
} cArea_t;

typedef struct {
    int         checkcount;             // to avoid repeated testings
    int         surfaceFlags;
    int         contents;
    struct patchCollide_s   *pc;
} cPatch_t;

typedef struct {
    char        name[MAX_QPATH];

    int         numShaders;
    dshader_t   *shaders;

    int         numBrushSides;
    cbrushside_t *brushsides;

    int         numPlanes;
    cplane_t    *planes;

    int         numNodes;
    cNode_t     *nodes;

    int         numLeafs;
    cLeaf_t     *leafs;

    int         numLeafBrushes;
    int         *leafbrushes;

    int         numLeafSurfaces;
    int         *leafsurfaces;

    int         numSubModels;
    cmodel_t    *cmodels;

    int         numBrushes;
    cbrush_t    *brushes;

    int         numClusters;
    int         clusterBytes;
    byte        *visibility;
    qboolean    vised;          // if false, visibility is just a single cluster of ffs

    int         numEntityChars;
    char        *entityString;

    int         numAreas;
    cArea_t     *areas;
    int         *areaPortals;   // [ numAreas*numAreas ] reference counts

    int         numSurfaces;
    cPatch_t    **surfaces;         // non-patches will be NULL

    int         floodvalid;
    int         checkcount;                 // incremented on each trace
} clipMap_t;

typedef struct {
    int         shaderNum;
    int         fogNum;
    int         surfaceType;

    int         firstVert;
    int         numVerts;

    int         firstIndex;
    int         numIndexes;

    int         lightmapNum;
    int         lightmapX, lightmapY;
    int         lightmapWidth, lightmapHeight;

    vec3_t      lightmapOrigin;
    vec3_t      lightmapVecs[3];    // for patches, [0] and [1] are lodbounds

    int         patchWidth;
    int         patchHeight;
} dsurface_t;

typedef enum {
    MST_BAD,
    MST_PLANAR,
    MST_PATCH,
    MST_TRIANGLE_SOUP,
    MST_FLARE
} mapSurfaceType_t;

