//
#ifndef __TR_SHADER_INFO_H__
#define __TR_SHADER_INFO_H__
typedef struct shader_info_s {
    int stages_target;
    int bundle_target;
    int image_target;
    int sky_outerbox;
    int sky_innerbox;
    shader_t *shader;
    image_t **image;
}shader_info_t;

#endif
