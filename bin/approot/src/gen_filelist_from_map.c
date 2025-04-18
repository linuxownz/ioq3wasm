#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <signal.h>
#include "qfiles.h"
#include "assert.h"

#define LittleLong
#define LittleFloat

union {
    int  *i;
    void *v;
} buf;

clipMap_t cm;

byte *cmod_base = NULL;

void CM_LoadMap2( char *name, FILE *file, int size );

// gcc generate_file_list_from_q3_map.c -o generate_file_list_from_q3_map

void usage() {
    printf("Usage: prog q3map\n");
    exit(1);
}

void CMod_LoadBrushes( lump_t *l ) {
    dbrush_t    *in;
    cbrush_t    *out;
    int         i, count;

    in = (void *)(cmod_base + l->fileofs);
    if (l->filelen % sizeof(*in)) {
        printf ("MOD_LoadBmodel: funny lump size");
        exit(1);
    }
    count = l->filelen / sizeof(*in);

    cm.brushes = malloc( ( BOX_BRUSHES + count ) * sizeof( *cm.brushes ));
    cm.numBrushes = count;

    out = cm.brushes;

    for ( i=0 ; i<count ; i++, out++, in++ ) {
        out->sides = cm.brushsides + LittleLong(in->firstSide);
        out->numsides = LittleLong(in->numSides);

        out->shaderNum = LittleLong( in->shaderNum );
        if ( out->shaderNum < 0 || out->shaderNum >= cm.numShaders ) {
            printf( "%s: bad shaderNum: %i", __func__, out->shaderNum );
            exit(1);
        }
        out->contents = cm.shaders[out->shaderNum].contentFlags;

        //CM_BoundBrush( out );
    }

}
void CMod_LoadBrushSides (lump_t *l)
{
    int             i;
    cbrushside_t    *out;
    dbrushside_t    *in;
    int             count;
    int             num;

    in = (void *)(cmod_base + l->fileofs);
    if ( l->filelen % sizeof(*in) ) {
        printf ("MOD_LoadBmodel: funny lump size");
        exit(1);
    }
    count = l->filelen / sizeof(*in);

    cm.brushsides = malloc( ( BOX_SIDES + count ) * sizeof( *cm.brushsides ));
    cm.numBrushSides = count;

    out = cm.brushsides;

    for ( i=0 ; i<count ; i++, in++, out++) {
        num = LittleLong( in->planeNum );
        out->plane = &cm.planes[num];
        out->shaderNum = LittleLong( in->shaderNum );
        if ( out->shaderNum < 0 || out->shaderNum >= cm.numShaders ) {
            printf( "%s: bad shaderNum: %i", __func__, out->shaderNum );
            exit(1);
        }
        out->surfaceFlags = cm.shaders[out->shaderNum].surfaceFlags;
    }
}

void CMod_LoadEntityString( lump_t *l ) {
    cm.entityString = malloc( l->filelen );
    cm.numEntityChars = l->filelen;
    memcpy (cm.entityString, cmod_base + l->fileofs, l->filelen);

    printf("%s\n", cm.entityString);
}

void CMod_LoadLeafBrushes (lump_t *l)
{
    int         i;
    int         *out;
    int         *in;
    int         count;

    in = (void *)(cmod_base + l->fileofs);
    if (l->filelen % sizeof(*in)) {
        printf ("MOD_LoadBmodel: funny lump size");
        exit(1);
    }
    count = l->filelen / sizeof(*in);

    cm.leafbrushes = malloc( (count + BOX_BRUSHES) * sizeof( *cm.leafbrushes ));
    cm.numLeafBrushes = count;

    out = cm.leafbrushes;

    for ( i=0 ; i<count ; i++, in++, out++) {
        *out = LittleLong (*in);
    }
}
void CMod_LoadLeafs (lump_t *l)
{
    int         i;
    cLeaf_t     *out;
    dleaf_t     *in;
    int         count;

    in = (void *)(cmod_base + l->fileofs);
    if (l->filelen % sizeof(*in)) {
        printf ("MOD_LoadBmodel: funny lump size");
        exit(1);
    }

    count = l->filelen / sizeof(*in);

    if (count < 1) {
        printf ("Map with no leafs");
        exit(1);
    }

    cm.leafs = malloc( ( BOX_LEAFS + count ) * sizeof( *cm.leafs ) );
    cm.numLeafs = count;

    out = cm.leafs;
    for ( i=0 ; i<count ; i++, in++, out++)
    {
        out->cluster = LittleLong (in->cluster);
        out->area = LittleLong (in->area);
        out->firstLeafBrush = LittleLong (in->firstLeafBrush);
        out->numLeafBrushes = LittleLong (in->numLeafBrushes);
        out->firstLeafSurface = LittleLong (in->firstLeafSurface);
        out->numLeafSurfaces = LittleLong (in->numLeafSurfaces);

        if (out->cluster >= cm.numClusters)
            cm.numClusters = out->cluster + 1;
        if (out->area >= cm.numAreas)
            cm.numAreas = out->area + 1;
    }

    cm.areas = malloc( cm.numAreas * sizeof( *cm.areas ) );
    cm.areaPortals = malloc( cm.numAreas * cm.numAreas * sizeof( *cm.areaPortals ) );
}
void CMod_LoadLeafSurfaces( lump_t *l )
{
    int         i;
    int         *out;
    int         *in;
    int         count;

    in = (void *)(cmod_base + l->fileofs);
    if (l->filelen % sizeof(*in)) {
        printf ("MOD_LoadBmodel: funny lump size");
        exit(1);
    }
    count = l->filelen / sizeof(*in);

    cm.leafsurfaces = malloc( count * sizeof( *cm.leafsurfaces ));
    cm.numLeafSurfaces = count;

    out = cm.leafsurfaces;

    for ( i=0 ; i<count ; i++, in++, out++) {
        *out = LittleLong (*in);
    }
}
void CMod_LoadNodes( lump_t *l ) {
    dnode_t     *in;
    int         child;
    cNode_t     *out;
    int         i, j, count;

    in = (void *)(cmod_base + l->fileofs);
    if (l->filelen % sizeof(*in)) {
        printf ("%s: funny lump size", __func__);
        exit(1);
    }
    count = l->filelen / sizeof(*in);

    if (count < 1) {
        printf ("Map has no nodes");
        exit(1);
    }
    cm.nodes = malloc( count * sizeof( *cm.nodes ) );
    cm.numNodes = count;

    out = cm.nodes;

    for (i=0 ; i<count ; i++, out++, in++)
    {
        out->plane = cm.planes + LittleLong( in->planeNum );
        for (j=0 ; j<2 ; j++)
        {
            child = LittleLong (in->children[j]);
            out->children[j] = child;
        }
    }

}
#define MAX_PATCH_VERTS     1024
void CMod_LoadPatches( lump_t *surfs, lump_t *verts ) {
    drawVert_t  *dv, *dv_p;
    dsurface_t  *in;
    int         count;
    int         i, j;
    int         c;
    cPatch_t    *patch;
    //vec3_t      points[MAX_PATCH_VERTS];
    int         width, height;
    int         shaderNum;

    in = (void *)(cmod_base + surfs->fileofs);
    if (surfs->filelen % sizeof(*in)) {
        printf ("%s: funny lump size", __func__);
        exit(1);
    }
    cm.numSurfaces = count = surfs->filelen / sizeof(*in);
    cm.surfaces = malloc( cm.numSurfaces * sizeof( cm.surfaces[0] ) );

    dv = (void *)(cmod_base + verts->fileofs);
    if (verts->filelen % sizeof(*dv)) {
        printf ("MOD_LoadBmodel: funny lump size");
        exit(1);
    }

    // scan through all the surfaces, but only load patches,
    // not planar faces
    for ( i = 0 ; i < count ; i++, in++ ) {
        if ( LittleLong( in->surfaceType ) != MST_PATCH ) {
            continue;       // ignore other surfaces
        }
        // FIXME: check for non-colliding patches

        cm.surfaces[ i ] = patch = malloc( sizeof( *patch ) );

        // load the full drawverts onto the stack
        width = LittleLong( in->patchWidth );
        height = LittleLong( in->patchHeight );
        c = width * height;
        if ( c > MAX_PATCH_VERTS ) {
            printf( "ParseMesh: MAX_PATCH_VERTS" );
            exit(1);
        }

        dv_p = dv + LittleLong( in->firstVert );
        for ( j = 0 ; j < c ; j++, dv_p++ ) {
            //points[j][0] = LittleFloat( dv_p->xyz[0] );
            //points[j][1] = LittleFloat( dv_p->xyz[1] );
            //points[j][2] = LittleFloat( dv_p->xyz[2] );
        }

        shaderNum = LittleLong( in->shaderNum );
        patch->contents = cm.shaders[shaderNum].contentFlags;
        patch->surfaceFlags = cm.shaders[shaderNum].surfaceFlags;

        // create the internal facet structure
        //patch->pc = CM_GeneratePatchCollide( width, height, points );
    }
}
void CMod_LoadPlanes (lump_t *l)
{
    int         i, j;
    cplane_t    *out;
    dplane_t    *in;
    int         count;
    int         bits;

    in = (void *)(cmod_base + l->fileofs);
    if (l->filelen % sizeof(*in)) {
        printf ("MOD_LoadBmodel: funny lump size");
        exit(1);
    }
    count = l->filelen / sizeof(*in);

    if (count < 1) {
        printf ("Map with no planes");
        exit(1);
    }
    cm.planes = malloc( ( BOX_PLANES + count ) * sizeof( *cm.planes ));
    cm.numPlanes = count;

    out = cm.planes;

    for ( i=0 ; i<count ; i++, in++, out++)
    {
        bits = 0;
        for (j=0 ; j<3 ; j++)
        {
            out->normal[j] = LittleFloat (in->normal[j]);
            if (out->normal[j] < 0)
                bits |= 1<<j;
        }

        out->dist = LittleFloat (in->dist);
        out->type = 1 ; //PlaneTypeForNormal( out->normal );
        out->signbits = bits;
    }
}

void CMod_LoadShaders( lump_t *l ) {
    dshader_t *in, *out;
    int         i, count;

    in = (void *)(cmod_base + l->fileofs);

    if (l->filelen % sizeof(*in)) {
        printf ("%s: funny lump size l->filelen:%d sizeof(*in):%ld\n", __func__, l->filelen, sizeof(*in));
        exit(1);
    }

    count = l->filelen / sizeof(*in);

    if (count < 1) {
        printf ("Map with no shaders");
        exit(1);
    }

    cm.shaders    = malloc( count * sizeof( *cm.shaders ) );
    cm.numShaders = count;

    memcpy( cm.shaders, in, count * sizeof( *cm.shaders ) );

    out = cm.shaders;
    for ( i=0 ; i<count ; i++, in++, out++ ) {
        out->contentFlags = LittleLong( out->contentFlags );
        out->surfaceFlags = LittleLong( out->surfaceFlags );
    }

    dshader_t * sh = cm.shaders;
    for ( int i = 0 ; i < cm.numShaders ; i++ ) {
        printf("%s\n", sh->shader);
        sh++;
    }
}
void CMod_LoadSubmodels( lump_t *l ) {
    dmodel_t    *in;
    cmodel_t    *out;
    int         i, j, count;
    int         *indexes;

    in = (void *)(cmod_base + l->fileofs);
    if (l->filelen % sizeof(*in)) {
        printf ("%s: funny lump size", __func__);
    }
    count = l->filelen / sizeof(*in);

    if (count < 1) {
        printf ("Map with no models");
        exit(1);
    }

    cm.cmodels = malloc( count * sizeof( *cm.cmodels ) );
    cm.numSubModels = count;

    if ( count > MAX_SUBMODELS ) {
        printf( "MAX_SUBMODELS exceeded" );
        exit(1);
    }

    for ( i=0 ; i<count ; i++, in++)
    {
        out = &cm.cmodels[i];

        for (j=0 ; j<3 ; j++)
        {   // spread the mins / maxs by a pixel
            out->mins[j] = LittleFloat (in->mins[j]) - 1;
            out->maxs[j] = LittleFloat (in->maxs[j]) + 1;
        }

        if ( i == 0 ) {
            continue;   // world model doesn't need other info
        }

        // make a "leaf" just to hold the model's brushes and surfaces
        out->leaf.numLeafBrushes = LittleLong( in->numBrushes );
        indexes = malloc( out->leaf.numLeafBrushes * 4 );
        out->leaf.firstLeafBrush = indexes - cm.leafbrushes;
        for ( j = 0 ; j < out->leaf.numLeafBrushes ; j++ ) {
            indexes[j] = LittleLong( in->firstBrush ) + j;
        }

        out->leaf.numLeafSurfaces = LittleLong( in->numSurfaces );
        indexes = malloc( out->leaf.numLeafSurfaces * 4 );
        out->leaf.firstLeafSurface = indexes - cm.leafsurfaces;
        for ( j = 0 ; j < out->leaf.numLeafSurfaces ; j++ ) {
            indexes[j] = LittleLong( in->firstSurface ) + j;
        }
    }
}

#define VIS_HEADER  8
void CMod_LoadVisibility( lump_t *l ) {
    int     len;
    byte    *buf;

    len = l->filelen;
    if ( !len ) {
        cm.clusterBytes = ( cm.numClusters + 31 ) & ~31;
        cm.visibility = malloc( cm.clusterBytes );
        memset( cm.visibility, 255, cm.clusterBytes );
        return;
    }
    buf = cmod_base + l->fileofs;

    cm.vised = qtrue;
    cm.visibility = malloc( len );
        cm.numClusters  = LittleLong( ((int *)buf)[0] ); // TODO possible alignment fault
        cm.clusterBytes = LittleLong( ((int *)buf)[1] ); // TODO possible alignment fault
    memcpy (cm.visibility, buf + VIS_HEADER, len - VIS_HEADER );
}

int main ( int argc, char **argv ) {
    char *name = argv[1];

    if ( name == NULL ) {
        usage();
    }

    FILE *file = fopen(name, "r");

    if ( NULL == file ) {
        perror("fopen");
        return 1;
    }

    // printf("processing map '%s'\n", name);

    struct stat statbuf;
    fstat(fileno(file), &statbuf);
    int size = statbuf.st_size;

    buf.v = malloc(size);
    fread(buf.v, size, 1, file);

    if ( ! buf.i ) {
        printf ("Couldn't load %s", name);
        exit(1);
    }

    dheader_t header = *(dheader_t *)buf.v;
    for ( int i = 0 ; i < sizeof(dheader_t) / 4 ; i++ ) {
        ((int *)&header)[i] = LittleLong ( ((int *)&header)[i]);
    }

    if ( header.version != BSP_VERSION ) {
        printf ("CM_LoadMap: %s has wrong version number (%i should be %i)", name, header.version, BSP_VERSION );
        exit(1);
    }

    if ( header.ident != BSP_IDENT ) {
        printf ("CM_LoadMap: %s has wrong ident (%i should be %i)", name, header.ident, BSP_IDENT );
        exit(1);
    }

    cmod_base = (byte *)buf.i;

    CMod_LoadShaders( &header.lumps[LUMP_SHADERS] );
    //CMod_LoadLeafs (&header.lumps[LUMP_LEAFS]);
    //CMod_LoadLeafBrushes (&header.lumps[LUMP_LEAFBRUSHES]);
    //CMod_LoadLeafSurfaces (&header.lumps[LUMP_LEAFSURFACES]);
    //CMod_LoadPlanes (&header.lumps[LUMP_PLANES]);
    //CMod_LoadBrushSides (&header.lumps[LUMP_BRUSHSIDES]);
    //CMod_LoadBrushes (&header.lumps[LUMP_BRUSHES]);
    //CMod_LoadSubmodels (&header.lumps[LUMP_MODELS]);
    //CMod_LoadNodes (&header.lumps[LUMP_NODES]);
    CMod_LoadEntityString (&header.lumps[LUMP_ENTITIES]);
    //CMod_LoadVisibility( &header.lumps[LUMP_VISIBILITY] );
    CMod_LoadPatches( &header.lumps[LUMP_SURFACES], &header.lumps[LUMP_DRAWVERTS] );
}

