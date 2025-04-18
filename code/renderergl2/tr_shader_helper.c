#include "tr_local.h"
#include "../qcommon/qcommon_client.h"

qboolean memnonnull ( const char * start, int size) {
    for ( int i = 0 ; i < size ; i++ ) {
        if ( start[i] ) {
            return qtrue;
        }
    }

    return qfalse;
}

void DumpTexMod(texModInfo_t *texMod) {
    showf(texMod->matrix[0][0]);
    showf(texMod->scale[0]);
    showf(texMod->scroll[0]);
    showf(texMod->translate[0]);
}

void DumpImage(image_t *image) {

    if ( image ) {
        showp(image);
        show(image->imgName);
        showp(image->bundle);
        showi(image->texnum);
    }
}

void DumpAllImages() {
    Com_Printf("Dumping all images\n");

    for ( int i = 0 ; i < tr.numImages ; i++ ) {
        image_t * image = tr.images[i];
        DumpImage(image);
    }
}

static image_t *FindImage( const char *name ) {
    for ( int i = 0 ; i < tr.numImages ; i++ ) {
        image_t *image = tr.images[i];

        if ( strcmp(image->imgName, name) == 0 ) {
            Com_Printf("Found image %s\n", name);
            DumpImage(image);
            return image;
            break;
        }
    }
}

static image_t *FindImageByBundleAddr(void *p) {
    Com_Printf("Looking for %p\n", p);

    for ( int i = 0 ; i < tr.numImages ; i++ ) {
        image_t *image = tr.images[i];

        showp(image->bundle);
        if ( image->bundle == p ) {
            return image;
        }
    }

    return NULL;
}

static void DumpBundle( textureBundle_t *bundle ) {
    Com_Printf("%s\n", __func__);

    showp(&bundle->image[0]);
    showp(bundle->image[0]);
    showi(bundle->isLightmap);
    showi(bundle->isVideoMap);
    //showi(bundle->tcGen);
    //showf(bundle->tcGenVectors[0]);
    //showf(bundle->tcGenVectors[1]);
    //showf(bundle->tcGenVectors[2]);

    showi(bundle->numImageAnimations);
    for ( int i = 0 ; i < MAX_IMAGE_ANIMATIONS ; i++ ) {
        image_t *image = bundle->image[i];
        DumpImage(image);
    }

    showi(bundle->numTexMods);
    for ( int i = 0 ; i < bundle->numTexMods ; i++ ) {
        texModInfo_t *texMod = bundle->texMods;
        //DumpTexMod(texMod);
    }

    Com_Printf("\n");
}

extern int numImageLoadsInFlight;
qboolean ValidateNumUnfoggedPasses(shader_t *shader) {
    if ( ! shader ) {
        return qfalse;
    }

    int count = 0;
    for ( int i = 0 ; i < MAX_SHADER_STAGES ; i++ ) {
        shaderStage_t *st = &shader->stages[i];

        if ( ! st ) {
            continue;
        }

        if ( ! st->active ) {
            continue;
        }

        if ( st->bundle[0].image[0] ) {
            count++;
        }
    }

    if ( count != shader->numUnfoggedPasses ) {
        Com_Printf("shader %s numunfoggedpasses:%d count:%d\n", shader->name, shader->numUnfoggedPasses, count);
        if ( numImageLoadsInFlight ) {
            Com_Printf("num image loads in flight:%d\n", numImageLoadsInFlight);
        }
    }

    if ( count == 0 ) {
        if ( numImageLoadsInFlight == 0 ) {
            Com_Printf("nilif:0 no images\n");
        }
    }

    return count == shader->numUnfoggedPasses;
}

void DumpStage ( shaderStage_t *stage ) {
    Com_Printf("%s\n", __func__);
    if ( ! memnonnull((char *)stage, sizeof(shaderStage_t))) {
        return;
    }

    showi(stage->active);
    showi(stage->alphaGen);
    showi(stage->adjustColorsForFog);
    showi(stage->isDetail);
    show(stage->glslShaderGroup->name);
    showi(stage->type);
    showi(stage->stateBits);

    for ( int i = 0 ; i < NUM_TEXTURE_BUNDLES ; i++ ) {
        if ( memnonnull((const char *)&stage->bundle[i], sizeof(stage->bundle[i]))) {
            DumpBundle(&stage->bundle[i]);
        }
    }
}

static void DumpDeform(deformStage_t *deform) {
    showf(deform->bulgeWidth);
    showf(deform->bulgeHeight);
    showf(deform->bulgeSpeed);
}


void DumpShader(shader_t *shader) {
    assert(shader);
    Com_Printf("Shader:\n");

    show(shader->name);
    showi(shader->index);
    showi(shader->defaultShader);
    showi(shader->finished);
    showf(shader->sort);
    showi(shader->sortedIndex);
    showi(shader->explicitlyDefined);
    showp(shader->optimalStageIteratorFunc);
    showi(shader->isSky);
    showi(shader->isPortal);
    showi(shader->vertexAttribs);
    showf(shader->timeOffset);
    showi(shader->contentFlags);
    showi(shader->numUnfoggedPasses);

    ValidateNumUnfoggedPasses(shader);
    for ( int i = 0 ; i < shader->numUnfoggedPasses ; i++ ) {
        shaderStage_t *stage = &shader->stages[i];
        DumpStage(stage);
        Com_Printf("\n");
    }

    for ( int i = 0 ; i < shader->numDeforms ; i++ ) {
        deformStage_t *deform = &shader->deforms[i];
        //DumpDeform(deform);
        //Com_Printf("\n");
    }

    qboolean result = ValidateNumUnfoggedPasses(shader);

    Com_Printf("shader num unfogged passes %s number of non null bundle images\n", result ? "\e[32m==\e[0m" : "\e[31m!=\e[0m");

    Com_Printf("\n");
LINE;
}

void DumpCGSMediaShader(sfxHandle_t s) {
    Com_Printf("%s %d\n", __func__, s);
    DumpShader(tr.shaders[s]);
}

void ValidateShader(shader_t *shader) {
    DumpShader(shader);
}

void ValidateAllShaders(){
    Com_Printf("%s\n--------------\n numshaders: %d\n", __func__, tr.numShaders);
    for ( int i = 0 ; i < tr.numShaders ; i++ ) {
        ValidateShader(tr.shaders[i]);
    }
}

void DumpShaders(void) {
    Com_Printf("\e[H\e[2J\e[3J");
    Com_Printf("Dumping gfx/2d/bigchars shader\n");
    for ( int i = 0 ; i < tr.numShaders ; i++ ) {
        shader_t * shader = tr.shaders[i];
        if ( strcmp(shader->name, "gfx/2d/bigchars") == 0 ) {
            DumpShader(tr.shaders[i]);
        }
    }
}

qboolean ValidateImage ( image_t * image ) {
    assert(image);

    assert(image->width  > 0 && image->width  < 1024);
    assert(image->height > 0 && image->height < 1024);

    assert(image->texnum >= 0 && image->texnum < MAX_DRAWIMAGES);

    assert(image->uploadWidth  > 0 && image->uploadWidth  < 1024 );
    assert(image->uploadHeight > 0 && image->uploadHeight < 1024 );

    assert(image->index >= 0 && image->index < MAX_DRAWIMAGES);
}

void DumpTess(shaderCommands_t *tess) {
    DumpShader(tess->shader);
}

void DumpHungShaders() {
    Com_Printf("\n%s\n", __func__);
    for ( int i = 0 ; i < tr.numShaders ; i++ ) {
        shader_t * shader = tr.shaders[i];

        exit(0);
    }
}
