#include "mgl/gl.h"
#include "core/context.h"
#include "objects/object_store.h"
#include "objects/query.h"
#include "compat/selection.h"

#include <stdio.h>
#include <string.h>

struct capture_state {
    GLuint next_native;
    GLenum begin_target;
    GLuint begin_id;
    GLenum end_target;
    unsigned begin_calls;
    unsigned end_calls;
    unsigned result_calls;
    unsigned generation_calls;
    unsigned deletion_calls;
    GLuint sample_result;
    unsigned timer_begin_calls;
    unsigned timer_end_calls;
    unsigned counter_calls;
    unsigned result64_calls;
    unsigned timer_bits_calls;
    GLuint timer_available;
    GLuint64 timer_result;
    GLuint64 counter_result;
    GLboolean timer_extension;
    GLint disjoint;
} cap;

static void fake_gen_queries(GLsizei n, GLuint *ids) {
    GLsizei i;
    ++cap.generation_calls;
    for (i = 0; i < n; ++i)
        ids[i] = ++cap.next_native;
}

static void fake_delete_queries(GLsizei n, const GLuint *ids) {
    (void)n;
    (void)ids;
    ++cap.deletion_calls;
}

static void fake_begin_query(GLenum target, GLuint id) {
    cap.begin_target = target;
    cap.begin_id = id;
    ++cap.begin_calls;
    if (target == GL_TIME_ELAPSED)
        ++cap.timer_begin_calls;
}

static void fake_end_query(GLenum target) {
    cap.end_target = target;
    ++cap.end_calls;
    if (target == GL_TIME_ELAPSED)
        ++cap.timer_end_calls;
}

static const GLubyte *fake_get_string(GLenum target) {
    if (target == GL_EXTENSIONS && cap.timer_extension)
        return (const GLubyte *)"GL_EXT_disjoint_timer_query";
    return (const GLubyte *)"";
}

static void fake_get_integer(GLenum target, GLint *value) {
    if (target == GL_GPU_DISJOINT_EXT)
        *value = cap.disjoint;
    else
        *value = 0;
}

static void fake_query_counter(GLuint id, GLenum target) {
    (void)id;
    if (target == GL_TIMESTAMP)
        ++cap.counter_calls;
}

static void fake_timer_bits(GLenum target, GLenum pname, GLint *params) {
    if (pname == GL_QUERY_COUNTER_BITS && (target == GL_TIMESTAMP || target == GL_TIME_ELAPSED)) {
        *params = 48;
        ++cap.timer_bits_calls;
    }
}

static void fake_get_query_object64(GLuint id, GLenum pname, GLuint64 *value) {
    (void)id;
    if (pname == GL_QUERY_RESULT) {
        *value = cap.counter_calls != 0u ? cap.counter_result : cap.timer_result;
        ++cap.result64_calls;
    }
}

static void fake_get_query_object(GLuint id, GLenum pname, GLuint *value) {
    (void)id;
    ++cap.result_calls;
    if (pname == GL_QUERY_RESULT_AVAILABLE)
        *value = cap.timer_begin_calls != 0u ? cap.timer_available : GL_TRUE;
    else if (pname == GL_QUERY_RESULT)
        *value = cap.sample_result;
    else
        *value = 0u;
}

static int fail(int n, const char *msg) {
    fprintf(stderr, "query failure %d: %s\n", n, msg);
    return n;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    GLuint q = 0u, created = 0u, arb = 0u;
    GLuint result = 0u;
    GLuint64 result64 = 0u;
    GLint iv = 0;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));
    native.glesGenQueries = fake_gen_queries;
    native.glesDeleteQueries = fake_delete_queries;
    native.glesBeginQuery = fake_begin_query;
    native.glesEndQuery = fake_end_query;
    native.glesGetQueryObjectuiv = fake_get_query_object;
    native.glesGetString = fake_get_string;
    native.glesGetIntegerv = fake_get_integer;
    native.glesGetQueryivEXT = fake_timer_bits;
    native.glesQueryCounterEXT = fake_query_counter;
    native.glesGetQueryObjectui64vEXT = fake_get_query_object64;
    ctx.native_dispatch = &native;
    cap.sample_result = 1u;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_compat_state_init(&ctx.compat);
    mgl_pixel_transfer_init(&ctx.pixel_transfer);
    mgl_selection_init(&ctx.selection);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    ctx.next_query_name = 1u;
    mgl_context_set_current_for_test(&ctx);
    {
        GLint max_streams = -1;
        glGetIntegerv(GL_MAX_VERTEX_STREAMS, &max_streams);
        if (max_streams != 1 || glGetError() != GL_NO_ERROR)
            return fail(99, "single GLES geometry stream limit");
    }

    glGenQueries(1, &q);
    if (q == 0u || glIsQuery(q) != GL_FALSE || mgl_query_find(&ctx, q) == NULL)
        return fail(2, "generated name should be reserved, not instantiated");
    glGetQueryObjectuiv(q, GL_QUERY_RESULT, &result);
    if (glGetError() != GL_INVALID_OPERATION)
        return fail(47, "reserved query is not a query object");
    glBeginQuery(GL_SAMPLES_PASSED, q);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || glIsQuery(q) != GL_TRUE ||
        cap.begin_calls != 1u || cap.begin_target != GL_ANY_SAMPLES_PASSED_CONSERVATIVE ||
        cap.begin_id == 0u)
        return fail(3, "native query begin/mapping");
    glGetQueryiv(GL_SAMPLES_PASSED, GL_CURRENT_QUERY, &iv);
    if (iv != (GLint)q)
        return fail(4, "current query");
    glEndQuery(GL_SAMPLES_PASSED);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR || cap.end_calls != 1u ||
        cap.end_target != GL_ANY_SAMPLES_PASSED_CONSERVATIVE)
        return fail(5, "native query end");
    glGetQueryObjectuiv(q, GL_QUERY_RESULT_AVAILABLE, &result);
    if (result != GL_TRUE)
        return fail(6, "native result availability");
    glGetQueryObjectuiv(q, GL_QUERY_RESULT, &result);
    if (result != 4096u)
        return fail(7, "sample count compatibility result");
    glGetQueryObjectivARB(q, GL_QUERY_RESULT, &iv);
    if (iv != 4096 || glGetError() != GL_NO_ERROR)
        return fail(21, "sample count ARB result");
    cap.sample_result = 0u;
    glBeginQuery(GL_SAMPLES_PASSED, q);
    glEndQuery(GL_SAMPLES_PASSED);
    glGetQueryObjectuiv(q, GL_QUERY_RESULT, &result);
    if (result != 0u)
        return fail(22, "occluded sample count remains zero");
    cap.sample_result = 1u;
    glBeginQuery(GL_SAMPLES_PASSED, q);
    glEndQuery(GL_SAMPLES_PASSED);
    glGetQueryObjectui64v(q, GL_QUERY_RESULT, &result64);
    if (result64 != 4096u)
        return fail(23, "64-bit sample count projection");

    glCreateQueries(GL_TIME_ELAPSED, 1, &created);
    if (created == 0u || glIsQuery(created) != GL_TRUE)
        return fail(8, "CreateQueries object");
    glBeginQuery(GL_TIME_ELAPSED, created);
    if (glGetError() != GL_INVALID_OPERATION || mgl_query_find(&ctx, created)->active)
        return fail(9, "unavailable timer must not activate query");
    glGetQueryiv(GL_TIME_ELAPSED, GL_QUERY_COUNTER_BITS, &iv);
    if (iv != 0 || glGetError() != GL_NO_ERROR)
        return fail(10, "unavailable timer bits");
    glGetQueryiv(GL_TIMESTAMP, GL_QUERY_COUNTER_BITS, &iv);
    if (iv != 0 || glGetError() != GL_NO_ERROR)
        return fail(11, "unavailable timestamp bits");

    glGenQueriesARB(1, &arb);
    glQueryCounter(arb, GL_TIMESTAMP);
    if (glGetError() != GL_INVALID_OPERATION || glIsQuery(arb))
        return fail(40, "unavailable timestamp must not create query");
    glEndQuery(GL_TIME_ELAPSED);
    if (glGetError() != GL_INVALID_OPERATION)
        return fail(41, "rejected timer did not become active");
    {
        GLint disjoint = 17;
        GLint64 timestamp = 17;
        glGetIntegerv(GL_GPU_DISJOINT_EXT, &disjoint);
        if (glGetError() != GL_INVALID_OPERATION || disjoint != 17)
            return fail(42, "unavailable GPU disjoint must not return a false result");
        glGetInteger64v(GL_TIMESTAMP, &timestamp);
        if (glGetError() != GL_INVALID_OPERATION || timestamp != 17)
            return fail(46, "unavailable GPU timestamp must not reach GLES");
    }

    {
        GLuint timer = 0u, timestamp = 0u;
        unsigned results_before;
        GLint disjoint = -1;
        cap.timer_extension = GL_TRUE;
        {
            unsigned counters_before = cap.counter_calls;
            glQueryCounter(q, GL_TIMESTAMP);
            if (glGetError() != GL_INVALID_OPERATION ||
                cap.counter_calls != counters_before ||
                mgl_query_find(&ctx, q)->target != GL_SAMPLES_PASSED)
                return fail(52, "timestamp must preserve query target");
        }
        cap.timer_available = GL_FALSE;
        cap.timer_result = 0x123456789ABCDEF0ull;
        cap.counter_result = 0xFEDCBA9876543210ull;
        glGenQueries(1, &timer);
        glBeginQuery(GL_TIME_ELAPSED, timer);
        glEndQuery(GL_TIME_ELAPSED);
        if (cap.timer_begin_calls != 1u || cap.timer_end_calls != 1u ||
            cap.begin_target != GL_TIME_ELAPSED || cap.end_target != GL_TIME_ELAPSED)
            return fail(29, "native elapsed timer begin/end");
        glGetQueryObjectuiv(timer, GL_QUERY_RESULT_AVAILABLE, &result);
        if (result != GL_FALSE)
            return fail(30, "native pending result");
        results_before = cap.result64_calls;
        result64 = 99u;
        glGetQueryObjectui64v(timer, GL_QUERY_RESULT_NO_WAIT, &result64);
        if (result64 != 99u || cap.result64_calls != results_before)
            return fail(31, "native no-wait result");
        cap.timer_available = GL_TRUE;
        glGetQueryObjectui64v(timer, GL_QUERY_RESULT, &result64);
        if (result64 != cap.timer_result || cap.result64_calls != results_before + 1u)
            return fail(32, "native elapsed 64-bit result");
        glGetQueryiv(GL_TIME_ELAPSED, GL_QUERY_COUNTER_BITS, &iv);
        if (iv != 48 || cap.timer_bits_calls != 1u)
            return fail(33, "native elapsed counter bits");
        glGenQueries(1, &timestamp);
        glQueryCounter(timestamp, GL_TIMESTAMP);
        if (cap.counter_calls != 1u)
            return fail(34, "native timestamp query");
        glGetQueryObjectui64v(timestamp, GL_QUERY_RESULT, &result64);
        if (result64 != cap.counter_result)
            return fail(35, "native timestamp 64-bit result");
        glGetQueryiv(GL_TIMESTAMP, GL_QUERY_COUNTER_BITS, &iv);
        if (iv != 48 || cap.timer_bits_calls != 2u)
            return fail(36, "native timestamp bits");
        cap.disjoint = 1;
        glGetIntegerv(GL_GPU_DISJOINT_EXT, &disjoint);
        if (disjoint != 1 || glGetError() != GL_NO_ERROR)
            return fail(37, "GPU disjoint query");
        glDeleteQueries(1, &timer);
        glDeleteQueries(1, &timestamp);
        cap.timer_extension = GL_FALSE;
        cap.disjoint = 0;
        cap.timer_begin_calls = 0u;
        cap.timer_end_calls = 0u;
        cap.counter_calls = 0u;
        glGenQueries(1, &timer);
        glBeginQuery(GL_TIME_ELAPSED, timer);
        if (glGetError() != GL_INVALID_OPERATION || glIsQuery(timer) || cap.timer_begin_calls != 0u)
            return fail(38, "timer without extension must fail");
        glQueryCounter(timer, GL_TIMESTAMP);
        if (glGetError() != GL_INVALID_OPERATION || glIsQuery(timer) || cap.counter_calls != 0u)
            return fail(43, "timestamp without extension must fail");
        cap.timer_extension = GL_TRUE;
        native.glesGetQueryObjectui64vEXT = NULL;
        glBeginQuery(GL_TIME_ELAPSED, timer);
        if (glGetError() != GL_INVALID_OPERATION || glIsQuery(timer) || cap.timer_begin_calls != 0u)
            return fail(39, "timer without 64-bit entry point must fail");
        glQueryCounter(timer, GL_TIMESTAMP);
        if (glGetError() != GL_INVALID_OPERATION || glIsQuery(timer) || cap.counter_calls != 0u)
            return fail(44, "timestamp without 64-bit entry point must fail");
        glGetQueryiv(GL_TIMESTAMP, GL_QUERY_COUNTER_BITS, &iv);
        if (glGetError() != GL_NO_ERROR || iv != 0)
            return fail(45, "timestamp without 64-bit entry point reports zero bits");
        native.glesGetQueryObjectui64vEXT = fake_get_query_object64;
        cap.timer_extension = GL_FALSE;
    }

    glBeginQuery(GL_ANY_SAMPLES_PASSED, q);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION)
        return fail(12, "query target immutability");
    {
        GLuint overlap;
        unsigned calls = cap.begin_calls;
        glGenQueries(1, &overlap);
        glBeginQuery(GL_SAMPLES_PASSED, q);
        glBeginQuery(GL_ANY_SAMPLES_PASSED, overlap);
        if (glGetError() != GL_INVALID_OPERATION || cap.begin_calls != calls + 1u ||
            glIsQuery(overlap))
            return fail(27, "occlusion targets must not overlap");
        glEndQuery(GL_SAMPLES_PASSED);
        if (ctx.active_occlusion_query != NULL)
            return fail(28, "query active pointer release");
        glDeleteQueries(1, &overlap);
    }

    {
        GLuint boolean_query;
        unsigned begins = cap.begin_calls, deleted = cap.deletion_calls;
        glGenQueries(1, &boolean_query);
        glBeginQuery(GL_ANY_SAMPLES_PASSED, boolean_query);
        glEndQuery(GL_ANY_SAMPLES_PASSED);
        glBeginQuery(GL_ANY_SAMPLES_PASSED_CONSERVATIVE, boolean_query);
        if (glGetError() != GL_INVALID_OPERATION || cap.begin_calls != begins + 1u ||
            cap.deletion_calls != deleted)
            return fail(48, "boolean query target immutability");
        glGetQueryObjectuiv(boolean_query, GL_QUERY_TARGET, &result);
        if (result != GL_ANY_SAMPLES_PASSED || glGetError() != GL_NO_ERROR)
            return fail(49, "boolean query target preserved");
        glBeginQuery(GL_ANY_SAMPLES_PASSED, boolean_query);
        glEndQuery(GL_ANY_SAMPLES_PASSED);
        if (glGetError() != GL_NO_ERROR)
            return fail(53, "matching boolean query reuse");
        glDeleteQueries(1, &boolean_query);
    }
    {
        GLuint active_query;
        unsigned ends = cap.end_calls, deleted = cap.deletion_calls;
        glGenQueries(1, &active_query);
        glBeginQuery(GL_ANY_SAMPLES_PASSED, active_query);
        glDeleteQueries(1, &active_query);
        if (glGetError() != GL_NO_ERROR || glIsQuery(active_query) || cap.end_calls != ends + 1u ||
            cap.deletion_calls != deleted + 1u || ctx.active_occlusion_query != NULL)
            return fail(50, "active query deletion");
        glEndQuery(GL_ANY_SAMPLES_PASSED);
        if (glGetError() != GL_INVALID_OPERATION)
            return fail(51, "deleted query must not remain active");
    }
    glDeleteQueries(1, &q);
    if (glIsQuery(q) != GL_FALSE)
        return fail(13, "delete query");
    glDeleteQueries(1, &created);
    glDeleteQueriesARB(1, &arb);

    /* Illegal query operations must preserve both interface and GLES state. */
    {
        GLuint live = 0u, untouched = 777u;
        GLuint output_u = 998u;
        GLint output_i = -998;
        GLint64 output_i64 = -998;
        GLuint64 output_u64 = 998u;
        unsigned generations, deletions, begins, ends, reads;
        glGenQueries(1, &live);
        glBeginQuery(GL_SAMPLES_PASSED, live);
        glEndQuery(GL_SAMPLES_PASSED);
        if (!glIsQuery(live) || glGetError() != GL_NO_ERROR)
            return fail(14, "query fixture");
        generations = cap.generation_calls;
        deletions = cap.deletion_calls;
        begins = cap.begin_calls;
        ends = cap.end_calls;
        reads = cap.result_calls;
        glBegin(GL_POINTS);
        glGenQueries(1, &untouched);
        glGenQueriesARB(1, &untouched);
        glCreateQueries(GL_SAMPLES_PASSED, 1, &untouched);
        glDeleteQueries(1, &live);
        glDeleteQueriesARB(1, &live);
        glBeginQuery(GL_SAMPLES_PASSED, live);
        glBeginQueryARB(GL_SAMPLES_PASSED, live);
        glEndQuery(GL_SAMPLES_PASSED);
        glEndQueryARB(GL_SAMPLES_PASSED);
        glQueryCounter(live, GL_TIMESTAMP);
        glGetQueryiv(GL_SAMPLES_PASSED, GL_CURRENT_QUERY, &output_i);
        glGetQueryObjectiv(live, GL_QUERY_RESULT, &output_i);
        glGetQueryObjectuiv(live, GL_QUERY_RESULT, &output_u);
        glGetQueryObjecti64v(live, GL_QUERY_RESULT, &output_i64);
        glGetQueryObjectui64v(live, GL_QUERY_RESULT, &output_u64);
        (void)glIsQuery(live);
        glEnd();
        if (glGetError() != GL_INVALID_OPERATION || untouched != 777u || output_i != -998 ||
            output_u != 998u || output_i64 != -998 || output_u64 != 998u ||
            cap.generation_calls != generations || cap.deletion_calls != deletions ||
            cap.begin_calls != begins || cap.end_calls != ends || cap.result_calls != reads ||
            glIsQuery(live) != GL_TRUE || glGetError() != GL_NO_ERROR)
            return fail(15, "illegal Begin/End mutated query state or reached GLES");
        glGetQueryObjectuiv(live, GL_QUERY_RESULT, &output_u);
        if (output_u != 4096u || glGetError() != GL_NO_ERROR)
            return fail(16, "legal query use after rejected operations");
        glDeleteQueries(1, &live);
        if (cap.deletion_calls != deletions + 1u || glIsQuery(live) || glGetError() != GL_NO_ERROR)
            return fail(17, "legal query deletion after rejected operations");
    }

    {
        GLuint indexed;
        GLint value = -1;
        glGenQueries(1, &indexed);
        glBeginQueryIndexed(GL_PRIMITIVES_GENERATED, 1, indexed);
        if (glGetError() != GL_INVALID_VALUE || glIsQuery(indexed))
            return fail(18, "missing indexed native must not create a query");
        glGetQueryIndexediv(GL_PRIMITIVES_GENERATED, 0, GL_CURRENT_QUERY, &value);
        if (value != 0 || glGetError() != GL_NO_ERROR)
            return fail(19, "indexed query zero current state");
        glBeginQueryIndexed(GL_SAMPLES_PASSED, 1, indexed);
        if (glGetError() != GL_INVALID_VALUE)
            return fail(20, "non-stream index validation");
    }

    {
        GLuint binary = 0u;
        GLuint unbacked = 0u;
        struct mgl_native_dispatch *saved_native = ctx.native_dispatch;
        glGenQueries(1, &binary);
        glBeginQuery(GL_ANY_SAMPLES_PASSED, binary);
        glEndQuery(GL_ANY_SAMPLES_PASSED);
        glGetQueryObjectuiv(binary, GL_QUERY_RESULT, &result);
        if (result != 1u)
            return fail(24, "boolean query must remain boolean");
        glDeleteQueries(1, &binary);
        ctx.native_dispatch = NULL;
        glGenQueries(1, &unbacked);
        glBeginQuery(GL_SAMPLES_PASSED, unbacked);
        glEndQuery(GL_SAMPLES_PASSED);
        glGetQueryObjectuiv(unbacked, GL_QUERY_RESULT, &result);
        if (result != 0u || glGetError() != GL_NO_ERROR)
            return fail(25, "unbacked query does not synthesize visibility");
        glDeleteQueries(1, &unbacked);
        ctx.native_dispatch = saved_native;
    }

    {
        GLuint ungenerated = 90001u, generated = 0u;
        ctx.profile_mask = GL_CONTEXT_CORE_PROFILE_BIT;
        glBeginQuery(GL_ANY_SAMPLES_PASSED, ungenerated);
        if (glGetError() != GL_INVALID_OPERATION || glIsQuery(ungenerated))
            return fail(52, "core profile ungenerated query rejected");
        glGenQueries(1, &generated);
        glBeginQuery(GL_ANY_SAMPLES_PASSED, generated);
        if (glGetError() != GL_NO_ERROR || !glIsQuery(generated))
            return fail(53, "core profile reserved query accepted");
        glEndQuery(GL_ANY_SAMPLES_PASSED);
        glDeleteQueries(1, &generated);
        glQueryCounter(ungenerated, GL_TIMESTAMP);
        if (glGetError() != GL_INVALID_OPERATION)
            return fail(54, "core profile ungenerated timestamp rejected");
        ctx.profile_mask = GL_CONTEXT_COMPATIBILITY_PROFILE_BIT;
        glBeginQuery(GL_ANY_SAMPLES_PASSED, ungenerated);
        if (glGetError() != GL_NO_ERROR || !glIsQuery(ungenerated))
            return fail(55, "compatibility profile unnamed query accepted");
        glEndQuery(GL_ANY_SAMPLES_PASSED);
        glDeleteQueries(1, &ungenerated);
    }
    glGetQueryiv(GL_PRIMITIVES_GENERATED, GL_QUERY_COUNTER_BITS, &iv);
    if (iv != 0 || glGetError() != GL_NO_ERROR)
        return fail(56, "unavailable primitive counter reports zero bits");
    glGetQueryiv(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, GL_QUERY_COUNTER_BITS, &iv);
    if (iv != 32 || glGetError() != GL_NO_ERROR)
        return fail(57, "native transform feedback counter reported");
    {
        GLuint primitive;
        glGenQueries(1, &primitive);
        glBeginQuery(GL_PRIMITIVES_GENERATED, primitive);
        if (glGetError() != GL_INVALID_OPERATION || glIsQuery(primitive))
            return fail(58, "unavailable primitive counter rejected");
        glDeleteQueries(1, &primitive);
    }
    mgl_context_set_current_for_test(NULL);
    mgl_query_free_all(&ctx);
    mgl_share_group_release(ctx.share_group);
    puts("mGL query object unit test: PASS");
    return 0;
}
