#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>

#define MAX_QPATH           64      // max length of a quake game pathname

typedef int qboolean;
typedef unsigned char       byte;

typedef struct {
    int         ident;
    int         version;

    char        name[MAX_QPATH];    // model name

    int         flags;

    int         numFrames;
    int         numTags;
    int         numSurfaces;

    int         numSkins;

    int         ofsFrames;          // offset for first frame
    int         ofsTags;            // numFrames * numTags
    int         ofsSurfaces;        // first surface, others follow

    int         ofsEnd;             // end of file
} md3Header_t;

typedef float vec_t;
typedef vec_t vec3_t[3];

typedef struct md3Frame_s {
    vec3_t      bounds[2];
    vec3_t      localOrigin;
    float       radius;
    char        name[16];
} md3Frame_t;


typedef struct {
    int     ident;              //

    char    name[MAX_QPATH];    // polyset name

    int     flags;
    int     numFrames;          // all surfaces in a model should have the same

    int     numShaders;         // all surfaces in a model should have the same
    int     numVerts;

    int     numTriangles;
    int     ofsTriangles;

    int     ofsShaders;         // offset from start of md3Surface_t
    int     ofsSt;              // texture coords are common for all frames
    int     ofsXyzNormals;      // numVerts * numFrames

    int     ofsEnd;             // next surface follows
} md3Surface_t;

typedef struct
{
    float           bounds[2][3];
    float           localOrigin[3];
    float           radius;
} mdvFrame_t;

typedef struct
{
    float           origin[3];
    float           axis[3][3];
} mdvTag_t;

typedef struct
{
    char            name[MAX_QPATH];    // tag name
} mdvTagName_t;

typedef signed short int16_t;
typedef struct
{
    vec3_t          xyz;
    int16_t         normal[4];
    int16_t         tangent[4];
} mdvVertex_t;

typedef struct
{
    float           st[2];
} mdvSt_t;


typedef enum {
    SF_BAD,
    SF_SKIP,                // ignore
    SF_FACE,
    SF_GRID,
    SF_TRIANGLES,
    SF_POLY,
    SF_MDV,
    SF_MDR,
    SF_IQM,
    SF_FLARE,
    SF_ENTITY,              // beams, rails, lightning, etc that can be determined by entity
    SF_VAO_MDVMESH,
    SF_VAO_IQM,

    SF_NUM_SURFACE_TYPES,
    SF_MAX = 0x7fffffff         // ensures that sizeof( surfaceType_t ) == sizeof( int )
} surfaceType_t;

typedef unsigned short glIndex_t;

typedef struct mdvSurface_s
{
    surfaceType_t   surfaceType;

    char            name[MAX_QPATH];    // polyset name

    int             numShaderIndexes;
    int             *shaderIndexes;

    int             numVerts;
    mdvVertex_t    *verts;
    mdvSt_t        *st;

    int             numIndexes;
    glIndex_t      *indexes;

    struct mdvModel_s *model;
} mdvSurface_t;

typedef unsigned int uint32_t;


typedef struct vaoAttrib_s
{
    uint32_t enabled;
    uint32_t count;
    uint32_t type;
    uint32_t normalized;
    uint32_t stride;
    uint32_t offset;
} vaoAttrib_t;

#define VAO_MAX_ATTRIBS 16

typedef struct vao_s
{
    char            name[MAX_QPATH];

    uint32_t        vao;

    uint32_t        vertexesVBO;
    int             vertexesSize;   // amount of memory data allocated for all vertices in bytes
    vaoAttrib_t     attribs[VAO_MAX_ATTRIBS];

    uint32_t        frameSize;      // bytes to skip per frame when doing vertex animation

    uint32_t        indexesIBO;
    int             indexesSize;    // amount of memory data allocated for all triangles in bytes
} vao_t;

typedef struct srfVaoMdvMesh_s
{
    surfaceType_t   surfaceType;

    struct mdvModel_s *mdvModel;
    struct mdvSurface_s *mdvSurface;

    // backEnd stats
    int             numIndexes;
    int             numVerts;

    // static render data
    vao_t          *vao;
} srfVaoMdvMesh_t;

typedef struct mdvModel_s
{
    int             numFrames;
    mdvFrame_t     *frames;

    int             numTags;
    mdvTag_t       *tags;
    mdvTagName_t   *tagNames;

    int             numSurfaces;
    mdvSurface_t   *surfaces;

    int             numVaoSurfaces;
    srfVaoMdvMesh_t  *vaoSurfaces;

    int             numSkins;
} mdvModel_t;

typedef enum {
    MOD_BAD,
    MOD_BRUSH,
    MOD_MESH,
    MOD_MDR,
    MOD_IQM
} modtype_t;


typedef struct {
    vec3_t      bounds[2];      // for culling
    int         firstSurface;
    int         numSurfaces;
} bmodel_t;

#define MD3_MAX_LODS        3
typedef struct model_s {
    char        name[MAX_QPATH];
    modtype_t   type;
    int         index;      // model = tr.models[model->index]

    int         dataSize;   // just for listing purposes
    bmodel_t    *bmodel;        // only if type == MOD_BRUSH
    mdvModel_t  *mdv[MD3_MAX_LODS]; // only if type == MOD_MESH
    void    *modelData;         // only if type == (MOD_MDR | MOD_IQM)

    int          numLods;
} model_t;


typedef struct {
    char            name[MAX_QPATH];
    int             shaderIndex;    // for in-game use
} md3Shader_t;

typedef struct {
    int         indexes[3];
} md3Triangle_t;

typedef struct {
    float       st[2];
} md3St_t;

typedef struct {
    short       xyz[3];
    short       normal;
} md3XyzNormal_t;


typedef struct md3Tag_s {
    char        name[MAX_QPATH];    // tag name
    vec3_t      origin;
    vec3_t      axis[3];
} md3Tag_t;

char *Q_strlwr( char *s1 ) {
    char    *s;

    s = s1;
    while ( *s ) {
        *s = tolower(*s);
        s++;
    }
    return s1;
}


static qboolean R_LoadMD3(model_t * mod, const int lod, const void *buffer, const int bufferSize, const char *modName)
{
    int             f, i, j;

    md3Header_t    *md3Model;
    md3Frame_t     *md3Frame;
    md3Surface_t   *md3Surf;
    md3Shader_t    *md3Shader;
    md3Triangle_t  *md3Tri;
    md3St_t        *md3st;
    md3XyzNormal_t *md3xyz;
    md3Tag_t       *md3Tag;

    mdvModel_t     *mdvModel;
    mdvFrame_t     *frame;
    mdvSurface_t   *surf;//, *surface;
    int            *shaderIndex;
    glIndex_t      *tri;
    mdvVertex_t    *v;
    mdvSt_t        *st;
    mdvTag_t       *tag;
    mdvTagName_t   *tagName;

    int             version;
    int             size;

    if ( lod < 0 || lod >= MD3_MAX_LODS ) {
        printf("%s lod:%d invalid\n", __func__, lod);
    }

    md3Model = (md3Header_t *) buffer;

#define MD3_VERSION         15
    version = (md3Model->version);
    if(version != MD3_VERSION)
    {
        printf("%s: %s has wrong version (%i should be %i)\n", __func__, modName, version, MD3_VERSION);
        return 0;
    }

    mod->type = MOD_MESH;

    size = (md3Model->ofsEnd);
    mod->dataSize += size;
    mdvModel = mod->mdv[lod] = malloc(sizeof(mdvModel_t));

//  Com_Memcpy(mod->md3[lod], buffer, LittleLong(md3Model->ofsEnd));

#define LL

    LL(md3Model->ident);
    LL(md3Model->version);
    LL(md3Model->numFrames);
    LL(md3Model->numTags);
    LL(md3Model->numSurfaces);
    LL(md3Model->ofsFrames);
    LL(md3Model->ofsTags);
    LL(md3Model->ofsSurfaces);
    LL(md3Model->ofsEnd);

    if(md3Model->numFrames < 1)
    {
        printf("%s: %s has no frames\n", __func__, modName);
        return 0;
    }

    // swap all the frames
    mdvModel->numFrames = md3Model->numFrames;
    mdvModel->frames = frame = malloc(sizeof(*frame) * md3Model->numFrames);

    md3Frame = (md3Frame_t *) ((byte *) md3Model + md3Model->ofsFrames); // TODO possible alignment fault
    for(i = 0; i < md3Model->numFrames; i++, frame++, md3Frame++)
    {
        frame->radius = (md3Frame->radius);
        for(j = 0; j < 3; j++)
        {
            frame->bounds[0][j]   = (md3Frame->bounds[0][j]);
            frame->bounds[1][j]   = (md3Frame->bounds[1][j]);
            frame->localOrigin[j] = (md3Frame->localOrigin[j]);
        }
    }

    // swap all the tags
    mdvModel->numTags = md3Model->numTags;
    mdvModel->tags = tag = malloc(sizeof(*tag) * (md3Model->numTags * md3Model->numFrames));

    md3Tag = (md3Tag_t *) ((byte *) md3Model + md3Model->ofsTags); // TODO possible alignment fault
    for(i = 0; i < md3Model->numTags * md3Model->numFrames; i++, tag++, md3Tag++)
    {
        for(j = 0; j < 3; j++)
        {
            tag->origin[j]  = (md3Tag->origin[j]);
            tag->axis[0][j] = (md3Tag->axis[0][j]);
            tag->axis[1][j] = (md3Tag->axis[1][j]);
            tag->axis[2][j] = (md3Tag->axis[2][j]);
        }
    }

    mdvModel->tagNames = tagName = malloc(sizeof(*tagName) * (md3Model->numTags));

    md3Tag = (md3Tag_t *) ((byte *) md3Model + md3Model->ofsTags); // TODO possible alignment fault
    for(i = 0; i < md3Model->numTags; i++, tagName++, md3Tag++)
    {
        strncpy(tagName->name, md3Tag->name, sizeof(tagName->name));
    }

    // swap all the surfaces
    mdvModel->numSurfaces = md3Model->numSurfaces;
    mdvModel->surfaces = surf = malloc(sizeof(*surf) * md3Model->numSurfaces);

    md3Surf = (md3Surface_t *) ((byte *) md3Model + md3Model->ofsSurfaces); // TODO possible alignment fault
    for(i = 0; i < md3Model->numSurfaces; i++)
    {
        LL(md3Surf->ident);
        LL(md3Surf->flags);
        LL(md3Surf->numFrames);
        LL(md3Surf->numShaders);
        LL(md3Surf->numTriangles);
        LL(md3Surf->ofsTriangles);
        LL(md3Surf->numVerts);
        LL(md3Surf->ofsShaders);
        LL(md3Surf->ofsSt);
        LL(md3Surf->ofsXyzNormals);
        LL(md3Surf->ofsEnd);

#define SHADER_MAX_VERTEXES 1000
#define SHADER_MAX_INDEXES  (6*SHADER_MAX_VERTEXES)

        if(md3Surf->numVerts >= SHADER_MAX_VERTEXES)
        {
            printf("%s: %s has more than %i verts on %s (%i).\n", __func__, modName, SHADER_MAX_VERTEXES - 1, md3Surf->name[0] ? md3Surf->name : "a surface", md3Surf->numVerts );
            return 0;
        }
        if(md3Surf->numTriangles * 3 >= SHADER_MAX_INDEXES)
        {
            printf("%s: %s has more than %i triangles on %s (%i).\n", __func__, modName, ( SHADER_MAX_INDEXES / 3 ) - 1, md3Surf->name[0] ? md3Surf->name : "a surface", md3Surf->numTriangles );
            return 0;
        }

        // change to surface identifier
        surf->surfaceType = SF_MDV;

        // give pointer to model for Tess_SurfaceMDX
        surf->model = mdvModel;

        // copy surface name
        strncpy(surf->name, md3Surf->name, sizeof(surf->name));

        // lowercase the surface name so skin compares are faster
        Q_strlwr(surf->name);

        // strip off a trailing _1 or _2
        // this is a crutch for q3data being a mess
        j = strlen(surf->name);
        if(j > 2 && surf->name[j - 2] == '_')
        {
            surf->name[j - 2] = 0;
        }

        // register the shaders
        surf->numShaderIndexes = md3Surf->numShaders;
        surf->shaderIndexes = shaderIndex = malloc(sizeof(*shaderIndex) * md3Surf->numShaders);

        md3Shader = (md3Shader_t *) ((byte *) md3Surf + md3Surf->ofsShaders); // TODO possible alignment fault
        for(j = 0; j < md3Surf->numShaders; j++, shaderIndex++, md3Shader++)
        {
            printf("m%s\n", md3Shader->name + 1);
        }

        // find the next surface
        md3Surf = (md3Surface_t *) ((byte *) md3Surf + md3Surf->ofsEnd); // TODO possible alignment fault
        surf++;
    }


    return 1;
}


int main ( int argc, char **argv ) {

    if ( argc < 2 ) {
        printf ("Usage: %s /path/to/model.md3\n", argv[0]);
        exit(1);
    }

    FILE *fp = fopen ( argv[1], "rb" );

    if ( ! fp ) {
        printf("failed to open file %s %s\n", argv[1], strerror(errno));
        exit(1);
    }

    struct stat s;
    stat(argv[1], &s);

    int bufferSize = s.st_size;
    char *buffer = malloc(bufferSize);

    size_t red = fread(buffer, bufferSize, 1, fp);

    for ( int i = 0 ; i < 3 ; i++ ) {
        model_t mod;
        R_LoadMD3(&mod, i, buffer, bufferSize, "somemodel");
    }

    return 0;
}
