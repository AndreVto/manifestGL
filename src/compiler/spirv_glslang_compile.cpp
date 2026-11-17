#include "spirv_glslang_compile.h"

#include <glslang/Public/ShaderLang.h>
#include <glslang/Public/ResourceLimits.h>
#include <SPIRV/GlslangToSpv.h>
#include <glslang/MachineIndependent/localintermediate.h>

#include <cstdlib>
#include <cstring>
#include <vector>
#include <set>

static char *copy_log(const char *message) {
    if (message == nullptr) message = "glslang compile failure";
    const size_t size = std::strlen(message) + 1u;
    char *result = static_cast<char *>(std::malloc(size));
    if (result != nullptr) std::memcpy(result, message, size);
    return result;
}

extern "C" int mgl_glslang_compile_spirv(GLenum shader_type, const char *source, unsigned version,
                                       unsigned int **words, size_t *count, char **compiler_log) {
    if (words != nullptr) *words = nullptr;
    if (count != nullptr) *count = 0u;
    if (compiler_log != nullptr) *compiler_log = nullptr;
    if (source == nullptr || words == nullptr || count == nullptr) return 0;

    EShLanguage stage;
    switch (shader_type) {
        case GL_VERTEX_SHADER: stage = EShLangVertex; break;
        case GL_FRAGMENT_SHADER: stage = EShLangFragment; break;
        case GL_GEOMETRY_SHADER: stage = EShLangGeometry; break;
        case GL_TESS_CONTROL_SHADER: stage = EShLangTessControl; break;
        case GL_TESS_EVALUATION_SHADER: stage = EShLangTessEvaluation; break;
        case GL_COMPUTE_SHADER: stage = EShLangCompute; break;
        default: return 0;
    }

    using namespace glslang;
    glslang::TShader shader(stage);
    shader.setStrings(&source, 1);
    shader.setEnvInput(EShSourceGlsl, stage, EShClientVulkan, static_cast<int>(version));
    shader.setEnvClient(EShClientOpenGL, EShTargetOpenGL_450);
    shader.setEnvTarget(EShTargetSpv, EShTargetSpv_1_5);
    shader.setAutoMapLocations(true);
    shader.setAutoMapBindings(true);

    if (!shader.parse(GetDefaultResources(), static_cast<int>(version), true, EShMsgDefault)) {
        if (compiler_log != nullptr) *compiler_log = copy_log(shader.getInfoLog());
        return 0;
    }
    glslang::TProgram program;
    program.addShader(&shader);
    if (!program.link(EShMsgDefault)) {
        if (compiler_log != nullptr) *compiler_log = copy_log(program.getInfoLog());
        return 0;
    }
    auto *intermediate = program.getIntermediate(stage);
    if (intermediate == nullptr) return 0;
    std::vector<unsigned int> spirv;
    glslang::GlslangToSpv(*intermediate, spirv);
    if (spirv.empty()) return 0;
    unsigned int *buffer = static_cast<unsigned int *>(std::malloc(spirv.size() * sizeof(unsigned int)));
    if (buffer == nullptr) return 0;
    std::memcpy(buffer, spirv.data(), spirv.size() * sizeof(unsigned int));
    *words = buffer;
    *count = spirv.size();
    return 1;
}


extern "C" int mgl_glslang_fragment_output_spirv(
    const char *source, unsigned version, size_t binding_count,
    const struct mgl_fragment_output_location *bindings, unsigned int **words,
    size_t *count, int *rewrite,
    struct mgl_glslang_fragment_output **result_outputs, size_t *output_count,
    char **compiler_log) {
    if (words != nullptr) *words = nullptr;
    if (count != nullptr) *count = 0u;
    if (rewrite != nullptr) *rewrite = 0;
    if (compiler_log != nullptr) *compiler_log = nullptr;
    if (result_outputs != nullptr) *result_outputs = nullptr;
    if (output_count != nullptr) *output_count = 0u;
    if (source == nullptr || words == nullptr || count == nullptr || rewrite == nullptr ||
        result_outputs == nullptr || output_count == nullptr ||
        (binding_count != 0u && bindings == nullptr)) return 0;

    using namespace glslang;
    constexpr EShLanguage stage = EShLangFragment;
    TShader shader(stage);
    shader.setStrings(&source, 1);
    shader.setEnvInput(EShSourceGlsl, stage, EShClientVulkan, static_cast<int>(version));
    shader.setEnvClient(EShClientOpenGL, EShTargetOpenGL_450);
    shader.setEnvTarget(EShTargetSpv, EShTargetSpv_1_5);
    shader.setAutoMapLocations(true);
    shader.setAutoMapBindings(true);
    if (!shader.parse(GetDefaultResources(), static_cast<int>(version), true, EShMsgDefault)) {
        if (compiler_log != nullptr) *compiler_log = copy_log(shader.getInfoLog());
        return 0;
    }

    TIntermediate *intermediate = shader.getIntermediate();
    if (intermediate == nullptr) return 0;
    TIntermAggregate *linker = intermediate->findLinkerObjects();
    if (linker == nullptr) return 1;
    std::vector<TIntermSymbol *> outputs;
    std::set<unsigned> used[2];
    for (TIntermNode *node : linker->getSequence()) {
        TIntermSymbol *symbol = node->getAsSymbolNode();
        if (symbol == nullptr) continue;
        const TQualifier &qualifier = symbol->getQualifier();
        if (qualifier.storage != EvqVaryingOut || qualifier.builtIn != EbvNone) continue;
        outputs.push_back(symbol);
    }

    for (TIntermSymbol *symbol : outputs) {
        TQualifier &qualifier = symbol->getQualifier();
        if (!qualifier.hasLocation() && intermediate->inIoAccessed(symbol->getName())) {
            for (size_t j = 0u; j < binding_count; ++j) {
                if (bindings[j].name != nullptr &&
                    symbol->getName() == bindings[j].name) {
                    qualifier.layoutLocation = (bindings[j].location & 0xfffu);
                    qualifier.layoutIndex = static_cast<unsigned char>(bindings[j].index);
                    *rewrite = 1;
                    break;
                }
            }
        }
        if (!qualifier.hasLocation()) continue;
        const int slots = TIntermediate::computeTypeLocationSize(symbol->getType(), stage);
        if (slots <= 0 || (qualifier.hasIndex() && qualifier.getIndex() > 1u)) return 0;
        const unsigned index = qualifier.hasIndex() ? qualifier.getIndex() : 0u;
        for (int offset = 0; offset < slots; ++offset)
            used[index].insert(qualifier.layoutLocation + static_cast<unsigned>(offset));
    }

    if (outputs.size() > 1u) {
        for (TIntermSymbol *symbol : outputs) {
            TQualifier &qualifier = symbol->getQualifier();
            if (qualifier.hasLocation()) continue;
            const int slots = TIntermediate::computeTypeLocationSize(symbol->getType(), stage);
            if (slots <= 0 || slots > static_cast<int>(MGL_MAX_DRAW_BUFFERS)) return 0;
            unsigned start = 0u;
            for (; start + static_cast<unsigned>(slots) <= MGL_MAX_DRAW_BUFFERS; ++start) {
                bool free = true;
                const unsigned index = qualifier.hasIndex() ? qualifier.getIndex() : 0u;
                if (index > 1u) return 0;
                for (int offset = 0; offset < slots; ++offset)
                    if (used[index].count(start + static_cast<unsigned>(offset)) != 0u)
                        free = false;
                if (free) {
                    qualifier.layoutLocation = (start & 0xfffu);
                    for (int offset = 0; offset < slots; ++offset)
                        used[index].insert(start + static_cast<unsigned>(offset));
                    *rewrite = 1;
                    break;
                }
            }
            if (!qualifier.hasLocation()) return 0;
        }
    }
    if (!*rewrite) return 1;

    TProgram program;
    program.addShader(&shader);
    if (!program.link(EShMsgDefault)) {
        if (compiler_log != nullptr) *compiler_log = copy_log(program.getInfoLog());
        return 0;
    }
    intermediate = program.getIntermediate(stage);
    if (intermediate == nullptr) return 0;
    std::vector<unsigned int> spirv;
    GlslangToSpv(*intermediate, spirv);
    if (spirv.empty()) return 0;
    unsigned int *buffer = static_cast<unsigned int *>(
        std::malloc(spirv.size() * sizeof(unsigned int)));
    if (buffer == nullptr) return 0;
    std::memcpy(buffer, spirv.data(), spirv.size() * sizeof(unsigned int));
    auto *assigned = static_cast<mgl_glslang_fragment_output *>(
        std::calloc(outputs.size(), sizeof(mgl_glslang_fragment_output)));
    if (assigned == nullptr) {
        std::free(buffer);
        return 0;
    }
    for (size_t i = 0u; i < outputs.size(); ++i) {
        const TIntermSymbol *output = outputs[i];
        const std::string name(output->getName().c_str());
        assigned[i].name = copy_log(name.c_str());
        if (assigned[i].name == nullptr) {
            for (size_t j = 0u; j < i; ++j) std::free(assigned[j].name);
            std::free(assigned);
            std::free(buffer);
            return 0;
        }
        assigned[i].location = output->getQualifier().layoutLocation;
        assigned[i].index = output->getQualifier().hasIndex()
                                ? output->getQualifier().getIndex() : 0u;
    }
    *result_outputs = assigned;
    *output_count = outputs.size();
    *words = buffer;
    *count = spirv.size();
    return 1;
}
