#include "mgl/gl.h"
#include "core/context.h"
#include "objects/object_store.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; } } while (0)

int main(void) {
    struct mgl_context ctx;
    GLdouble curve[] = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};
    GLdouble doubles[8];
    GLfloat floats[8];
    GLint integers[8];
    GLfloat pixel_values[] = {0.25f, 0.75f};
    GLuint uint_values[3] = {99u, 99u, 99u};
    GLushort short_values[3] = {99u, 99u, 99u};
    GLubyte stipple[128], output[300];
    GLushort halves[] = {0x3c00u, 0xc000u, 0x3800u, 0x4000u};
    struct mgl_buffer *pbo;
    size_t i;

    memset(&ctx, 0, sizeof(ctx));
    mgl_error_init(&ctx.errors);
    mgl_evaluator_init(&ctx.evaluator);
    mgl_pixel_transfer_init(&ctx.pixel_transfer);
    ctx.native_max_vertex_attribs = 8;
    ctx.pack_alignment = 4;
    ctx.unpack_alignment = 4;
    mgl_context_set_current_for_test(&ctx);

    glMap1d(GL_MAP1_VERTEX_3, 0.0, 1.0, 3, 2, curve);
    CHECK(glGetError() == GL_NO_ERROR);
    for (i = 0; i < 8u; ++i)
        doubles[i] = 90.0;
    glGetnMapdv(GL_MAP1_VERTEX_3, GL_COEFF, 47, doubles);
    CHECK(glGetError() == GL_INVALID_OPERATION && doubles[0] == 90.0);
    glGetnMapdv(GL_MAP1_VERTEX_3, GL_COEFF, 48, doubles);
    CHECK(glGetError() == GL_NO_ERROR && doubles[0] == 1.0 && doubles[5] == 6.0 && doubles[6] == 90.0);
    glGetnMapdv(GL_MAP1_VERTEX_3, GL_DOMAIN, 16, doubles);
    CHECK(glGetError() == GL_NO_ERROR && doubles[0] == 0.0 && doubles[1] == 1.0);
    glGetnMapdv(GL_MAP1_VERTEX_3, GL_DOMAIN, 15, doubles);
    CHECK(glGetError() == GL_INVALID_OPERATION);
    glGetnMapdv(0u, GL_DOMAIN, 64, doubles);
    CHECK(glGetError() == GL_INVALID_ENUM);
    glGetnMapfv(GL_MAP1_VERTEX_3, GL_COEFF, 23, floats);
    CHECK(glGetError() == GL_INVALID_OPERATION);
    glGetnMapfv(GL_MAP1_VERTEX_3, GL_COEFF, 24, floats);
    CHECK(glGetError() == GL_NO_ERROR && floats[5] == 6.0f);
    glGetnMapiv(GL_MAP1_VERTEX_3, GL_ORDER, 3, integers);
    CHECK(glGetError() == GL_INVALID_OPERATION);
    glGetnMapiv(GL_MAP1_VERTEX_3, GL_ORDER, 4, integers);
    CHECK(glGetError() == GL_NO_ERROR && integers[0] == 2);
    glGetnMapiv(GL_MAP1_VERTEX_3, GL_ORDER, -1, integers);
    CHECK(glGetError() == GL_INVALID_VALUE);

    glPixelMapfv(GL_PIXEL_MAP_R_TO_R, 2, pixel_values);
    CHECK(glGetError() == GL_NO_ERROR);
    floats[0] = -1.0f;
    glGetnPixelMapfv(GL_PIXEL_MAP_R_TO_R, 7, floats);
    CHECK(glGetError() == GL_INVALID_OPERATION && floats[0] == -1.0f);
    glGetnPixelMapfv(GL_PIXEL_MAP_R_TO_R, 8, floats);
    CHECK(glGetError() == GL_NO_ERROR && floats[0] == 0.25f && floats[1] == 0.75f);
    glGetnPixelMapuiv(GL_PIXEL_MAP_R_TO_R, 7, uint_values);
    CHECK(glGetError() == GL_INVALID_OPERATION && uint_values[0] == 99u);
    glGetnPixelMapuiv(GL_PIXEL_MAP_R_TO_R, 8, uint_values);
    CHECK(glGetError() == GL_NO_ERROR && uint_values[0] != 99u);
    glGetnPixelMapusv(GL_PIXEL_MAP_R_TO_R, 3, short_values);
    CHECK(glGetError() == GL_INVALID_OPERATION && short_values[0] == 99u);
    glGetnPixelMapusv(GL_PIXEL_MAP_R_TO_R, 4, short_values);
    CHECK(glGetError() == GL_NO_ERROR && short_values[0] != 99u);
    glGetnPixelMapfv(0u, 32, floats);
    CHECK(glGetError() == GL_INVALID_ENUM);

    memset(stipple, 0xaau, sizeof(stipple));
    memset(output, 0x5au, sizeof(output));
    glPolygonStipple(stipple);
    CHECK(glGetError() == GL_NO_ERROR);
    glGetnPolygonStipple(127, output);
    CHECK(glGetError() == GL_INVALID_OPERATION && output[0] == 0x5au);
    glGetnPolygonStipple(128, output);
    CHECK(glGetError() == GL_NO_ERROR && output[0] == 0xaau && output[127] == 0xaau && output[128] == 0x5au);
    glPixelStorei(GL_PACK_ROW_LENGTH, 40);
    glPixelStorei(GL_PACK_SKIP_ROWS, 1);
    CHECK(glGetError() == GL_NO_ERROR);
    memset(output, 0x5au, sizeof(output));
    glGetnPolygonStipple(259, output);
    CHECK(glGetError() == GL_INVALID_OPERATION && output[8] == 0x5au);
    glGetnPolygonStipple(260, output);
    CHECK(glGetError() == GL_NO_ERROR && output[8] == 0xaau && output[259] == 0xaau);
    glPixelStorei(GL_PACK_SKIP_ROWS, 0);
    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    CHECK(glGetError() == GL_NO_ERROR);

    glVertexAttribs2hvNV(2u, 2, halves);
    CHECK(glGetError() == GL_NO_ERROR && ctx.generic_current[2][0] == 1.0f &&
          ctx.generic_current[2][1] == -2.0f && ctx.generic_current[3][0] == 0.5f &&
          ctx.generic_current[3][1] == 2.0f);
    glVertexAttribs1hvNV(5u, 1, halves);
    CHECK(glGetError() == GL_NO_ERROR && ctx.generic_current[5][0] == 1.0f);
    glVertexAttribs3hvNV(5u, 1, halves);
    CHECK(glGetError() == GL_NO_ERROR && ctx.generic_current[5][2] == 0.5f);
    glVertexAttribs4hvNV(4u, 1, halves);
    CHECK(glGetError() == GL_NO_ERROR && ctx.generic_current[4][3] == 2.0f);
    glVertexAttribs2hvNV(7u, 2, halves);
    CHECK(glGetError() == GL_INVALID_VALUE && ctx.generic_current[7][0] == 0.0f);
    glVertexAttribs2hvNV(0u, -1, halves);
    CHECK(glGetError() == GL_INVALID_VALUE);
    glVertexAttribs2hvNV(0u, 0, NULL);
    CHECK(glGetError() == GL_NO_ERROR);

    ctx.share_group = mgl_share_group_create();
    CHECK(ctx.share_group != NULL);
    pbo = mgl_buffer_ensure(&ctx, 17u, 0);
    CHECK(pbo != NULL);
    pbo->size = 300u;
    pbo->shadow = calloc(1u, pbo->size);
    CHECK(pbo->shadow != NULL);
    ctx.pixel_pack_buffer = pbo->name;
    glGetnPixelMapfv(GL_PIXEL_MAP_R_TO_R, 1, (GLfloat *)(uintptr_t)4u);
    CHECK(glGetError() == GL_NO_ERROR);
    memcpy(floats, pbo->shadow + 4u, 2u * sizeof(GLfloat));
    CHECK(floats[0] == 0.25f && floats[1] == 0.75f);
    glGetnPixelMapfv(GL_PIXEL_MAP_R_TO_R, 32, (GLfloat *)(uintptr_t)296u);
    CHECK(glGetError() == GL_INVALID_OPERATION);
    glGetnPolygonStipple(0, (GLubyte *)(uintptr_t)128u);
    CHECK(glGetError() == GL_NO_ERROR && pbo->shadow[128] == 0xaau &&
          pbo->shadow[255] == 0xaau);
    pbo->mapped = GL_TRUE;
    glGetnPixelMapuiv(GL_PIXEL_MAP_R_TO_R, 32, (GLuint *)(uintptr_t)4u);
    CHECK(glGetError() == GL_INVALID_OPERATION);
    pbo->mapped = GL_FALSE;
    ctx.pixel_pack_buffer = 0u;
    mgl_share_group_release(ctx.share_group);
    mgl_evaluator_destroy(&ctx.evaluator);
    mgl_context_set_current_for_test(NULL);
    return 0;
}
