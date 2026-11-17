cmake_minimum_required(VERSION 3.16)
project(ManifestGL VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(MGL_BUILD_TESTS "Build ManifestGL bootstrap tests" ON)
option(MGL_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

if(DEFINED MGL_ENABLE_SPIRV_TOOLCHAIN AND NOT MGL_ENABLE_SPIRV_TOOLCHAIN)
    message(FATAL_ERROR "ManifestGL requires the glslang and SPIRV-Cross toolchain")
endif()

set(GLSLANG_TESTS OFF CACHE BOOL "" FORCE)
set(GLSLANG_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "" FORCE)
set(ENABLE_GLSLANG_JS OFF CACHE BOOL "" FORCE)
set(ENABLE_HLSL OFF CACHE BOOL "" FORCE)
set(ENABLE_OPT OFF CACHE BOOL "" FORCE)
set(ENABLE_PCH OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/glslang EXCLUDE_FROM_ALL)

set(SPIRV_CROSS_CLI OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_HLSL OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_MSL OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_CPP OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_REFLECT OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_UTIL OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_GLSL ON CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_C_API ON CACHE BOOL "" FORCE)
set(SPIRV_CROSS_STATIC ON CACHE BOOL "" FORCE)
set(SPIRV_CROSS_SHARED OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_SKIP_INSTALL ON CACHE BOOL "" FORCE)
set(SPIRV_CROSS_FORCE_PIC ON CACHE BOOL "" FORCE)
add_subdirectory(third_party/SPIRV-Cross EXCLUDE_FROM_ALL)

find_package(Threads REQUIRED)
set(MGL_X11_INCLUDE_DIR "" CACHE PATH "X11 include directory for cross builds")
set(MGL_X11_LIBRARY "" CACHE FILEPATH "X11 library for cross builds")
if(MGL_X11_INCLUDE_DIR AND MGL_X11_LIBRARY)
    set(MGL_X11_INCLUDE_DIRS ${MGL_X11_INCLUDE_DIR})
    set(MGL_X11_LIBRARIES ${MGL_X11_LIBRARY})
else()
    find_package(X11 REQUIRED)
    set(MGL_X11_INCLUDE_DIRS ${X11_INCLUDE_DIR})
    set(MGL_X11_LIBRARIES ${X11_LIBRARIES})
endif()

if(MGL_X11_LIBRARY)
    get_filename_component(MGL_X11_LIBDIR "${MGL_X11_LIBRARY}" DIRECTORY)
    find_library(MGL_XCB_LIBRARY NAMES xcb PATHS "${MGL_X11_LIBDIR}" NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
else()
    find_library(MGL_XCB_LIBRARY NAMES xcb)
endif()
if(NOT MGL_XCB_LIBRARY)
    message(FATAL_ERROR "libxcb is required for safe GLX drawable classification")
endif()
list(APPEND MGL_X11_LIBRARIES "${MGL_XCB_LIBRARY}")

set(MGL_RUNTIME_SOURCES
    src/abi/gl_exports.c
    src/abi/proc_address.c
    src/abi/unimplemented_exports.c
    src/native/dispatch.c
    src/native/loader.c
    src/core/capabilities.c
    src/core/context.c
    src/core/error.c
    src/core/render_diag.c
    src/core/runtime.c
    src/core/runtime_services.c
    src/debug/debug.c
    src/state/state.c
    src/state/state_exports.c
    src/state/internalformat_query.c
    src/state/modern_state.c
    src/compat/fixed_function.c
    src/compat/attribute_overloads.c
    src/compat/compat_overloads.c
    src/compat/packed_attributes.c
    src/compat/fixed_point.c
    src/compat/arb_program.c
    src/compat/arb_program_translate.c
    src/compat/evaluator.c
    src/compat/immediate.c
    src/compat/raster.c
    src/compat/selection.c
    src/compat/accum.c
    src/compat/legacy_raster_state.c
    src/list/display_list.c
    src/objects/object_store.c
    src/objects/buffer_exports.c
    src/objects/vao_exports.c
    src/objects/framebuffer_exports.c
    src/objects/query_exports.c
    src/objects/sampler_exports.c
    src/objects/sync_exports.c
    src/objects/nv_fence.c
    src/objects/transform_feedback.c
    src/texture/texture_store.c
    src/texture/depth_readback.c
    src/texture/pixel_layout.c
    src/texture/texture_exports.c
    src/texture/bcdec_impl.c
    src/shader/translator.c
    src/shader/shader_store.c
    src/shader/shader_exports.c
    src/shader/program_pipeline_exports.c
    src/shader/implicit_compat_stage.c
    src/shader/arb_shader_objects.c
    src/shader/program_uniform_exports.c
    src/shader/reflection_exports.c
    src/vertex/vertex_state.c
    src/vertex/draw.c
    src/presentation/android_ahb_presentation.c
    src/presentation/android_direct.c
    src/glx/drawable.c
    src/glx/glx_exports.c
)

add_library(mgl_glslang_bridge OBJECT src/compiler/spirv_glslang_compile.cpp)
set_target_properties(mgl_glslang_bridge PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(mgl_glslang_bridge PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(mgl_glslang_bridge PRIVATE glslang::glslang glslang::SPIRV glslang::glslang-default-resource-limits)
target_compile_options(mgl_glslang_bridge PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wno-shadow)
if(MGL_WARNINGS_AS_ERRORS)
    target_compile_options(mgl_glslang_bridge PRIVATE -Werror)
endif()
list(APPEND MGL_RUNTIME_SOURCES src/compiler/spirv_toolchain.c $<TARGET_OBJECTS:mgl_glslang_bridge>)

# Generated fixed-function GLSL exceeds the C99 minimum string-literal length.
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    set_source_files_properties(src/compat/fixed_function.c PROPERTIES COMPILE_OPTIONS "-Wno-overlength-strings")
endif()

set_source_files_properties(src/texture/bcdec_impl.c PROPERTIES COMPILE_OPTIONS "-w")

function(mgl_configure_runtime_target target)
    target_include_directories(${target}
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${MGL_X11_INCLUDE_DIRS}
    )
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion)
    if(MGL_WARNINGS_AS_ERRORS)
        target_compile_options(${target} PRIVATE -Werror)
    endif()
    target_link_libraries(${target} PUBLIC Threads::Threads ${CMAKE_DL_LIBS} ${MGL_X11_LIBRARIES} m)
    target_link_libraries(${target} PUBLIC glslang::glslang glslang::SPIRV glslang::glslang-default-resource-limits spirv-cross-c)
endfunction()

add_library(mgl SHARED ${MGL_RUNTIME_SOURCES})
target_compile_definitions(mgl PRIVATE MGL_BUILDING_LIBRARY=1)
mgl_configure_runtime_target(mgl)
if(ANDROID)
    target_link_libraries(mgl PRIVATE log)
endif()

# Hide linked compiler-archive symbols from the public libGL interface.
if(UNIX AND NOT APPLE)
    target_link_options(mgl PRIVATE "-Wl,--exclude-libs,ALL")
endif()

# Android payloads are staged from the current CMake target outputs.
if(ANDROID)
    set(MGL_ANDROID_PRODUCTS_DIR "${CMAKE_BINARY_DIR}/mgl-products")
    set_target_properties(mgl PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${MGL_ANDROID_PRODUCTS_DIR}")
endif()

# Android uses a fixed libGL payload filename; the ELF SONAME remains libGL.so.1.
if(ANDROID)
    set_target_properties(mgl PROPERTIES
        OUTPUT_NAME GL
        SUFFIX ".so.1.7.0"
        NO_SONAME TRUE
        C_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN YES
    )
    target_link_options(mgl PRIVATE "-Wl,-soname,libGL.so.1")
else()
    set_target_properties(mgl PROPERTIES
        OUTPUT_NAME GL
        VERSION 1.0.0
        SOVERSION 1
        C_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN YES
    )
endif()

add_library(mgl_egl SHARED src/egl/egl_interface.c)
target_compile_definitions(mgl_egl PRIVATE MGL_BUILDING_LIBRARY=1)
if(ANDROID)
    set_target_properties(mgl_egl PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${MGL_ANDROID_PRODUCTS_DIR}")
endif()
target_include_directories(mgl_egl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src ${MGL_X11_INCLUDE_DIRS})
target_link_libraries(mgl_egl PRIVATE mgl Threads::Threads ${CMAKE_DL_LIBS} ${MGL_X11_LIBRARIES})
if(MGL_WARNINGS_AS_ERRORS)
    if(MSVC)
        target_compile_options(mgl_egl PRIVATE /WX)
    else()
        target_compile_options(mgl_egl PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endif()
if(ANDROID)
    set_target_properties(mgl_egl PROPERTIES
        OUTPUT_NAME EGL
        SUFFIX ".so.1.0.0"
        NO_SONAME TRUE
        C_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN YES
    )
    target_link_options(mgl_egl PRIVATE "-Wl,-soname,libEGL.so.1")
    add_custom_command(TARGET mgl_egl POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E create_symlink $<TARGET_FILE_NAME:mgl_egl> ${MGL_ANDROID_PRODUCTS_DIR}/libEGL.so.1
        COMMAND ${CMAKE_COMMAND} -E create_symlink libEGL.so.1 ${MGL_ANDROID_PRODUCTS_DIR}/libEGL.so
        VERBATIM)
else()
    set_target_properties(mgl_egl PROPERTIES
        OUTPUT_NAME EGL
        VERSION 1.0.0
        SOVERSION 1
        C_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN YES
    )
endif()

if(MGL_BUILD_TESTS)
    enable_testing()

    # Test executables link the runtime implementation for shared-state coverage.
    add_library(mgl_test_runtime STATIC ${MGL_RUNTIME_SOURCES})
    mgl_configure_runtime_target(mgl_test_runtime)

    # Native GLES integration requires a surfaceless EGL/GLES provider.
    if(NOT CMAKE_CROSSCOMPILING)
        add_executable(mgl_gles_texture_framebuffer_probe
            tests/integration/gles_texture_framebuffer_probe.c)
        target_include_directories(mgl_gles_texture_framebuffer_probe PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(mgl_gles_texture_framebuffer_probe PRIVATE
            mgl_test_runtime ${MGL_X11_LIBRARIES} ${CMAKE_DL_LIBS})
        option(MGL_ENABLE_REAL_GLES_PROBE_CTEST
            "Run native GLES texture and framebuffer integration probe (requires surfaceless GLES3)" OFF)
        if(MGL_ENABLE_REAL_GLES_PROBE_CTEST)
            add_test(NAME gles_texture_framebuffer_probe COMMAND mgl_gles_texture_framebuffer_probe)
        endif()
    endif()

    add_executable(mgl_program_pipeline_test tests/unit/program_pipeline.c)
    target_include_directories(mgl_program_pipeline_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(mgl_program_pipeline_test PRIVATE mgl_test_runtime)
    add_test(NAME program_pipeline COMMAND mgl_program_pipeline_test)

    add_executable(mgl_basevertex_extension_selection_test tests/unit/basevertex_extension_selection.c)
    target_include_directories(mgl_basevertex_extension_selection_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(mgl_basevertex_extension_selection_test PRIVATE mgl_test_runtime)
    add_test(NAME basevertex_extension_selection COMMAND mgl_basevertex_extension_selection_test)

    add_executable(mgl_error_state_semantics_test tests/unit/error_state_semantics.c)
    target_include_directories(mgl_error_state_semantics_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(mgl_error_state_semantics_test PRIVATE mgl_test_runtime)
    add_test(NAME error_state_semantics COMMAND mgl_error_state_semantics_test)

    add_executable(mgl_bootstrap_smoke tests/unit/bootstrap_smoke.c)
    target_include_directories(mgl_bootstrap_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(mgl_bootstrap_smoke PRIVATE mgl)
    add_test(NAME bootstrap_smoke COMMAND mgl_bootstrap_smoke)

    find_package(Python3 QUIET COMPONENTS Interpreter)
    function(mgl_add_internal_test target test_name source)
        add_executable(${target} ${source})
        target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(${target} PRIVATE mgl_test_runtime)
        add_test(NAME ${test_name} COMMAND ${target})
    endfunction()

    function(mgl_add_trace_fixture target source)
        add_executable(${target} ${source})
        target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(${target} PRIVATE mgl_test_runtime)
    endfunction()

    mgl_add_internal_test(mgl_point_parameter_conformance_mgl point_parameter_conformance_mgl tests/conformance/point_parameter_manifestgl.c)
    target_include_directories(mgl_point_parameter_conformance_mgl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)

    mgl_add_internal_test(mgl_active_texture_conformance_mgl active_texture_conformance_mgl tests/conformance/active_texture_manifestgl.c)
    target_include_directories(mgl_active_texture_conformance_mgl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    if(NOT CMAKE_CROSSCOMPILING)
        find_library(MGL_REFERENCE_GL_LIBRARY NAMES GL GL.so.1 libGL.so.1)
        find_program(MGL_REFERENCE_XVFB_RUN xvfb-run)
    endif()
    if(NOT CMAKE_CROSSCOMPILING AND Python3_Interpreter_FOUND AND MGL_REFERENCE_GL_LIBRARY AND MGL_REFERENCE_XVFB_RUN)
        add_executable(mgl_point_parameter_mesa_reference tests/conformance/point_parameter_mesa_reference.c)
        target_include_directories(mgl_point_parameter_mesa_reference PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
        target_link_libraries(mgl_point_parameter_mesa_reference PRIVATE ${MGL_REFERENCE_GL_LIBRARY} ${MGL_X11_LIBRARIES})
        add_test(NAME point_parameter_mesa_conformance
            COMMAND ${MGL_REFERENCE_XVFB_RUN} -a ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance/compare_conformance_traces.py
                    $<TARGET_FILE:mgl_point_parameter_mesa_reference>
                    $<TARGET_FILE:mgl_point_parameter_conformance_mgl>)
    endif()
    if(NOT CMAKE_CROSSCOMPILING AND Python3_Interpreter_FOUND AND MGL_REFERENCE_GL_LIBRARY AND MGL_REFERENCE_XVFB_RUN)
        add_executable(mgl_active_texture_mesa_reference tests/conformance/active_texture_mesa_reference.c)
        target_include_directories(mgl_active_texture_mesa_reference PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
        target_link_libraries(mgl_active_texture_mesa_reference PRIVATE ${MGL_REFERENCE_GL_LIBRARY} ${MGL_X11_LIBRARIES})
        add_test(NAME active_texture_mesa_conformance
            COMMAND ${MGL_REFERENCE_XVFB_RUN} -a ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance/compare_conformance_traces.py
                    $<TARGET_FILE:mgl_active_texture_mesa_reference>
                    $<TARGET_FILE:mgl_active_texture_conformance_mgl>)
    endif()

    mgl_add_internal_test(mgl_core_state_test core_state tests/unit/core_state.c)
    mgl_add_internal_test(mgl_legacy_robust_exports_test legacy_robust_exports tests/unit/legacy_robust_exports.c)
    mgl_add_internal_test(mgl_nv_fence_test nv_fence tests/unit/nv_fence.c)
    mgl_add_internal_test(mgl_error_diagnostics_test error_diagnostics tests/unit/error_diagnostics.c)
    mgl_add_internal_test(mgl_native_dispatch_error_polling_test native_error_polling tests/unit/native_error_polling.c)
    mgl_add_internal_test(mgl_shader_translation_test shader_translation tests/unit/shader_translation.c)
    mgl_add_internal_test(mgl_shader_es2_compat_test shader_es2_compat tests/unit/shader_es2_compat.c)
    mgl_add_internal_test(mgl_shader_object_semantics_test shader_object_semantics tests/unit/shader_object_semantics.c)
    mgl_add_internal_test(mgl_program_state_fast_path_test program_state_fast_path tests/unit/program_state_fast_path.c)
    mgl_add_internal_test(mgl_shader_query_semantics_test shader_query_semantics tests/unit/shader_query_semantics.c)
    mgl_add_internal_test(mgl_shader_implicit_stage_test shader_implicit_stage tests/unit/shader_implicit_stage.c)
    mgl_add_internal_test(mgl_shader_binary_test shader_binary tests/unit/shader_binary.c)
    mgl_add_internal_test(mgl_original_glsl_preprocess_test original_glsl_preprocess tests/unit/desktop_glsl_preprocess.c)
    mgl_add_internal_test(mgl_spirv_toolchain_test spirv_toolchain tests/unit/spirv_toolchain.c)
    mgl_add_internal_test(mgl_spirv_specialization_test spirv_specialization tests/unit/shader_spirv_specialization.c)
    target_include_directories(mgl_spirv_specialization_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    mgl_add_internal_test(mgl_vertex_draw_semantics_test vertex_draw_semantics tests/unit/vertex_draw_semantics.c)
    mgl_add_internal_test(mgl_interleaved_arrays_test interleaved_arrays tests/unit/interleaved_arrays.c)
    mgl_add_internal_test(mgl_compiled_vertex_array_test compiled_vertex_array tests/unit/compiled_vertex_array.c)
    mgl_add_internal_test(mgl_glx_context_queries_test glx_context_queries tests/unit/glx_context_queries.c)
    set_tests_properties(glx_context_queries PROPERTIES ENVIRONMENT "MGL_EGL_LIBRARY=$<TARGET_FILE:mgl_test_native_egl>;MGL_GLES_LIBRARY=$<TARGET_FILE:mgl_test_native_egl>")
    mgl_add_internal_test(mgl_glx_context_attribs_test glx_context_attribs tests/unit/glx_context_attribs.c)
    set_tests_properties(glx_context_attribs PROPERTIES ENVIRONMENT "MGL_EGL_LIBRARY=$<TARGET_FILE:mgl_test_native_egl>;MGL_GLES_LIBRARY=$<TARGET_FILE:mgl_test_native_egl>")
    mgl_add_internal_test(mgl_glx_query_surface_test glx_query_surface tests/unit/glx_query_surface.c)
    set_tests_properties(glx_query_surface PROPERTIES ENVIRONMENT "MGL_EGL_LIBRARY=$<TARGET_FILE:mgl_test_native_egl>;MGL_GLES_LIBRARY=$<TARGET_FILE:mgl_test_native_egl>")
    mgl_add_internal_test(mgl_glx_current_state_test glx_current_state tests/unit/glx_current_state.c)
    set_tests_properties(glx_current_state PROPERTIES ENVIRONMENT "MGL_EGL_LIBRARY=$<TARGET_FILE:mgl_test_native_egl>;MGL_GLES_LIBRARY=$<TARGET_FILE:mgl_test_native_egl>")
    mgl_add_internal_test(mgl_glx_event_selection_test glx_event_selection tests/unit/glx_event_selection.c)
    mgl_add_internal_test(mgl_fixed_function_state_test fixed_function_state tests/unit/fixed_function_state.c)
    mgl_add_internal_test(mgl_matrix_state_semantics_test matrix_state_semantics tests/unit/matrix_state_semantics.c)
    mgl_add_internal_test(mgl_texture_matrix_uniform_test texture_matrix_uniform tests/unit/texture_matrix_uniform.c)
    mgl_add_internal_test(mgl_matrix_load_multiply_semantics_test matrix_load_multiply_semantics tests/unit/matrix_load_multiply_semantics.c)
    mgl_add_internal_test(mgl_matrix_transform_semantics_test matrix_transform_semantics tests/unit/matrix_transform_semantics.c)
    mgl_add_internal_test(mgl_immediate_mode_test immediate_mode tests/unit/immediate_mode.c)
    mgl_add_internal_test(mgl_lighting_integer_semantics_test lighting_integer_semantics tests/unit/lighting_integer_semantics.c)
    mgl_add_internal_test(mgl_texture_semantics_test texture_semantics tests/unit/texture_semantics.c)
    mgl_add_internal_test(mgl_stencil_texture_compat_test stencil_texture_compat tests/unit/stencil_texture_compat.c)
    mgl_add_internal_test(mgl_texture_1d_compat_test texture_1d_compat tests/unit/texture_1d_compat.c)
    mgl_add_internal_test(mgl_texture_format_transfer_test texture_format_transfer tests/unit/texture_format_transfer.c)
    mgl_add_internal_test(mgl_texture_packed_upload_test texture_packed_upload tests/unit/texture_packed_upload.c)
    mgl_add_internal_test(mgl_texture_gles32_format_matrix_test texture_gles32_format_matrix tests/unit/texture_gles32_format_matrix.c)
    mgl_add_internal_test(mgl_renderbuffer_gles32_formats_test renderbuffer_gles32_formats tests/unit/renderbuffer_gles32_formats.c)
    mgl_add_trace_fixture(mgl_texture_1d_compat_conformance_mgl tests/conformance/texture_1d_compat_manifestgl.c)
    target_include_directories(mgl_texture_1d_compat_conformance_mgl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    if(NOT CMAKE_CROSSCOMPILING AND MGL_REFERENCE_GL_LIBRARY AND MGL_REFERENCE_XVFB_RUN)
        add_executable(mgl_texture_1d_compat_mesa_reference tests/conformance/texture_1d_compat_mesa_reference.c)
        target_include_directories(mgl_texture_1d_compat_mesa_reference PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance)
        target_link_libraries(mgl_texture_1d_compat_mesa_reference PRIVATE ${MGL_REFERENCE_GL_LIBRARY} ${MGL_X11_LIBRARIES})
        add_test(NAME texture_1d_compat_mesa_conformance COMMAND ${MGL_REFERENCE_XVFB_RUN} -a ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance/compare_conformance_traces.py
                    $<TARGET_FILE:mgl_texture_1d_compat_mesa_reference> $<TARGET_FILE:mgl_texture_1d_compat_conformance_mgl>)
    endif()
    mgl_add_internal_test(mgl_image_copy_barrier_native_test image_copy_barrier_native tests/unit/image_copy_barrier_native.c)
    mgl_add_internal_test(mgl_image_copy_barrier_conformance_mgl image_copy_barrier_conformance_mgl tests/conformance/image_copy_barrier_manifestgl.c)
    target_include_directories(mgl_image_copy_barrier_conformance_mgl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    if(NOT CMAKE_CROSSCOMPILING AND MGL_REFERENCE_GL_LIBRARY AND MGL_REFERENCE_XVFB_RUN)
        add_executable(mgl_image_copy_barrier_mesa_reference tests/conformance/image_copy_barrier_mesa_reference.c)
        target_include_directories(mgl_image_copy_barrier_mesa_reference PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance)
        target_link_libraries(mgl_image_copy_barrier_mesa_reference PRIVATE ${MGL_REFERENCE_GL_LIBRARY} ${MGL_X11_LIBRARIES})
        add_test(NAME image_copy_barrier_mesa_conformance COMMAND ${MGL_REFERENCE_XVFB_RUN} -a ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance/compare_conformance_traces.py
                    $<TARGET_FILE:mgl_image_copy_barrier_mesa_reference> $<TARGET_FILE:mgl_image_copy_barrier_conformance_mgl>)
    endif()
    mgl_add_internal_test(mgl_buffer_clear_transfer_native_test buffer_clear_transfer_native tests/unit/buffer_clear_transfer_native.c)
    mgl_add_trace_fixture(mgl_buffer_clear_transfer_conformance_mgl tests/conformance/buffer_clear_transfer_manifestgl.c)
    target_include_directories(mgl_buffer_clear_transfer_conformance_mgl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    if(NOT CMAKE_CROSSCOMPILING AND MGL_REFERENCE_GL_LIBRARY AND MGL_REFERENCE_XVFB_RUN)
        add_executable(mgl_buffer_clear_transfer_mesa_reference tests/conformance/buffer_clear_transfer_mesa_reference.c)
        target_include_directories(mgl_buffer_clear_transfer_mesa_reference PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance)
        target_link_libraries(mgl_buffer_clear_transfer_mesa_reference PRIVATE ${MGL_REFERENCE_GL_LIBRARY} ${MGL_X11_LIBRARIES})
        add_test(NAME buffer_clear_transfer_mesa_conformance COMMAND ${MGL_REFERENCE_XVFB_RUN} -a ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance/compare_conformance_traces.py
                    $<TARGET_FILE:mgl_buffer_clear_transfer_mesa_reference> $<TARGET_FILE:mgl_buffer_clear_transfer_conformance_mgl>)
    endif()
    mgl_add_internal_test(mgl_buffer_storage_native_test buffer_storage_native tests/unit/buffer_storage_native.c)
    mgl_add_trace_fixture(mgl_buffer_storage_conformance_mgl tests/conformance/buffer_storage_manifestgl.c)
    target_include_directories(mgl_buffer_storage_conformance_mgl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    if(NOT CMAKE_CROSSCOMPILING AND MGL_REFERENCE_GL_LIBRARY AND MGL_REFERENCE_XVFB_RUN)
        add_executable(mgl_buffer_storage_mesa_reference tests/conformance/buffer_storage_mesa_reference.c)
        target_include_directories(mgl_buffer_storage_mesa_reference PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance)
        target_link_libraries(mgl_buffer_storage_mesa_reference PRIVATE ${MGL_REFERENCE_GL_LIBRARY} ${MGL_X11_LIBRARIES})
        add_test(NAME buffer_storage_mesa_conformance COMMAND ${MGL_REFERENCE_XVFB_RUN} -a ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance/compare_conformance_traces.py
                    $<TARGET_FILE:mgl_buffer_storage_mesa_reference> $<TARGET_FILE:mgl_buffer_storage_conformance_mgl>)
    endif()
    mgl_add_internal_test(mgl_clear_texture_native_test clear_texture_native tests/unit/clear_texture_native.c)
    mgl_add_trace_fixture(mgl_clear_texture_conformance_mgl tests/conformance/clear_texture_manifestgl.c)
    target_include_directories(mgl_clear_texture_conformance_mgl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    if(NOT CMAKE_CROSSCOMPILING AND MGL_REFERENCE_GL_LIBRARY AND MGL_REFERENCE_XVFB_RUN)
        add_executable(mgl_clear_texture_mesa_reference tests/conformance/clear_texture_mesa_reference.c)
        target_include_directories(mgl_clear_texture_mesa_reference PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance)
        target_link_libraries(mgl_clear_texture_mesa_reference PRIVATE ${MGL_REFERENCE_GL_LIBRARY} ${MGL_X11_LIBRARIES})
        add_test(NAME clear_texture_mesa_conformance COMMAND ${MGL_REFERENCE_XVFB_RUN} -a ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance/compare_conformance_traces.py
                    $<TARGET_FILE:mgl_clear_texture_mesa_reference> $<TARGET_FILE:mgl_clear_texture_conformance_mgl>)
    endif()
    mgl_add_internal_test(mgl_framebuffer_attachment_native_test framebuffer_attachment_native tests/unit/framebuffer_attachment_native.c)
    mgl_add_internal_test(mgl_gl45_framebuffer_dsa_test gl45_framebuffer_dsa tests/unit/gl45_framebuffer_dsa.c)
    mgl_add_internal_test(mgl_gl45_buffer_query_dsa_test gl45_buffer_query_dsa tests/unit/gl45_buffer_query_dsa.c)
    mgl_add_trace_fixture(mgl_framebuffer_attachment_conformance_mgl tests/conformance/framebuffer_attachment_manifestgl.c)
    target_include_directories(mgl_framebuffer_attachment_conformance_mgl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    if(NOT CMAKE_CROSSCOMPILING AND MGL_REFERENCE_GL_LIBRARY AND MGL_REFERENCE_XVFB_RUN)
        add_executable(mgl_framebuffer_attachment_mesa_reference tests/conformance/framebuffer_attachment_mesa_reference.c)
        target_include_directories(mgl_framebuffer_attachment_mesa_reference PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance)
        target_link_libraries(mgl_framebuffer_attachment_mesa_reference PRIVATE ${MGL_REFERENCE_GL_LIBRARY} ${MGL_X11_LIBRARIES})
        add_test(NAME framebuffer_attachment_mesa_conformance COMMAND ${MGL_REFERENCE_XVFB_RUN} -a ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance/compare_conformance_traces.py
                    $<TARGET_FILE:mgl_framebuffer_attachment_mesa_reference> $<TARGET_FILE:mgl_framebuffer_attachment_conformance_mgl>)
    endif()
    if(MGL_ENABLE_REAL_GLES_PROBE_CTEST)
        mgl_add_trace_fixture(mgl_shader_object_aliases_gles_test tests/integration/shader_object_aliases_real_gles.c)
        target_include_directories(mgl_shader_object_aliases_gles_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    endif()
    if(MGL_ENABLE_REAL_GLES_PROBE_CTEST AND NOT CMAKE_CROSSCOMPILING AND MGL_REFERENCE_GL_LIBRARY AND MGL_REFERENCE_XVFB_RUN)
        add_executable(mgl_shader_object_aliases_mesa_reference tests/conformance/shader_object_aliases_mesa_reference.c)
        target_include_directories(mgl_shader_object_aliases_mesa_reference PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/tests)
        target_link_libraries(mgl_shader_object_aliases_mesa_reference PRIVATE ${MGL_REFERENCE_GL_LIBRARY} ${MGL_X11_LIBRARIES})
        add_test(NAME shader_object_aliases_mesa_conformance COMMAND ${MGL_REFERENCE_XVFB_RUN} -a ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance/compare_conformance_traces.py
                    $<TARGET_FILE:mgl_shader_object_aliases_mesa_reference> $<TARGET_FILE:mgl_shader_object_aliases_gles_test>)
    endif()
    mgl_add_internal_test(mgl_legacy_entrypoint_aliases_conformance_mgl legacy_entrypoint_aliases_conformance_mgl tests/conformance/legacy_entrypoint_aliases_manifestgl.c)
    target_include_directories(mgl_legacy_entrypoint_aliases_conformance_mgl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    if(NOT CMAKE_CROSSCOMPILING AND MGL_REFERENCE_GL_LIBRARY AND MGL_REFERENCE_XVFB_RUN)
        add_executable(mgl_legacy_entrypoint_aliases_mesa_reference tests/conformance/legacy_entrypoint_aliases_mesa_reference.c)
        target_include_directories(mgl_legacy_entrypoint_aliases_mesa_reference PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance)
        target_link_libraries(mgl_legacy_entrypoint_aliases_mesa_reference PRIVATE ${MGL_REFERENCE_GL_LIBRARY} ${MGL_X11_LIBRARIES})
        add_test(NAME legacy_entrypoint_aliases_mesa_conformance COMMAND ${MGL_REFERENCE_XVFB_RUN} -a ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance/compare_conformance_traces.py
                    $<TARGET_FILE:mgl_legacy_entrypoint_aliases_mesa_reference> $<TARGET_FILE:mgl_legacy_entrypoint_aliases_conformance_mgl>)
    endif()
    mgl_add_internal_test(mgl_fixed_function_texture_test fixed_function_texture tests/unit/fixed_function_texture.c)
    mgl_add_internal_test(mgl_alpha_compat_test alpha_compat tests/unit/alpha_compat.c)
    mgl_add_internal_test(mgl_fog_texenv_test fog_texenv tests/unit/fog_texenv.c)
    mgl_add_internal_test(mgl_fog_parameter_semantics_test fog_parameter_semantics tests/unit/fog_parameter_semantics.c)
    mgl_add_internal_test(mgl_color_index_state_test color_index_state tests/unit/color_index_state.c)
    mgl_add_internal_test(mgl_fog_coord_test fog_coord tests/unit/fog_coord.c)
    mgl_add_internal_test(mgl_lighting_material_test lighting_material tests/unit/lighting_material.c)
    mgl_add_internal_test(mgl_texgen_clip_test texgen_clip tests/unit/texgen_clip.c)
    mgl_add_internal_test(mgl_single_precision_compat_test single_precision_compat tests/unit/single_precision_compat.c)
    mgl_add_internal_test(mgl_fixed_point_compat_test fixed_point_compat tests/unit/fixed_point_compat.c)
    mgl_add_internal_test(mgl_fixed_point_parameters_test fixed_point_parameters tests/unit/fixed_point_parameters.c)
    mgl_add_internal_test(mgl_fixed_point_queries_test fixed_point_queries tests/unit/fixed_point_queries.c)
    mgl_add_internal_test(mgl_normalized_color_compat_test normalized_color_compat tests/unit/normalized_color_compat.c)
    mgl_add_internal_test(mgl_gl33_compat_entrypoints_test gl33_compat_entrypoints tests/unit/gl33_compat_entrypoints.c)
    mgl_add_internal_test(mgl_evaluators_test evaluators tests/unit/evaluators.c)
    mgl_add_internal_test(mgl_raster_compat_test raster_compat tests/unit/raster_compat.c)
    mgl_add_internal_test(mgl_color_readback_formats_test color_readback_formats tests/unit/color_readback_formats.c)
    mgl_add_internal_test(mgl_high_precision_color_readback_test high_precision_color_readback tests/unit/high_precision_color_readback.c)
    mgl_add_internal_test(mgl_read_attachment_coverage_test read_attachment_coverage tests/unit/read_attachment_coverage.c)
    mgl_add_internal_test(mgl_pixel_map_windowpos_test pixel_map_windowpos tests/unit/pixel_map_windowpos.c)
    mgl_add_internal_test(mgl_display_list_late_binding_test display_list_late_binding tests/unit/display_list_late_binding.c)
    mgl_add_internal_test(mgl_display_list_calls_test display_list_calls tests/unit/display_list_calls.c)
    mgl_add_internal_test(mgl_display_list_transaction_test display_list_transaction tests/unit/display_list_transaction.c)
    mgl_add_internal_test(mgl_display_list_geometry_test display_list_geometry tests/unit/display_list_geometry.c)
    mgl_add_internal_test(mgl_display_list_texture_test display_list_texture tests/unit/display_list_texture.c)
    mgl_add_internal_test(mgl_display_list_draw_snapshot_test display_list_draw_snapshot tests/unit/display_list_draw_snapshot.c)
    mgl_add_internal_test(mgl_attrib_stack_test attrib_stack tests/unit/attrib_stack.c)
    mgl_add_internal_test(mgl_stencil_blend_state_test stencil_blend_state tests/unit/stencil_blend_state.c)
    mgl_add_internal_test(mgl_compressed_texture_test compressed_texture tests/unit/compressed_texture.c)
    mgl_add_internal_test(mgl_compressed_texture_rgtc_test compressed_texture_rgtc tests/unit/compressed_texture_rgtc.c)
    mgl_add_internal_test(mgl_compressed_storage_transcode_test compressed_storage_transcode tests/unit/compressed_storage_transcode.c)
    mgl_add_internal_test(mgl_compressed_readback_test compressed_readback tests/unit/compressed_readback.c)
    mgl_add_internal_test(mgl_copy_tex_test copy_tex tests/unit/copy_tex.c)
    mgl_add_internal_test(mgl_angle_texture_passthrough_test angle_texture_passthrough tests/unit/angle_texture_passthrough.c)
    mgl_add_internal_test(mgl_gles32_texture_format_table_test gles32_texture_format_table tests/unit/gles32_texture_format_table.c)
    mgl_add_internal_test(mgl_depth_readback_test depth_readback tests/unit/depth_readback.c)
    mgl_add_internal_test(mgl_selection_mode_test selection_mode tests/unit/selection_mode.c)
    mgl_add_internal_test(mgl_query_objects_test query_objects tests/unit/query_objects.c)
    mgl_add_internal_test(mgl_query_surface_test query_surface tests/unit/query_surface.c)
    mgl_add_internal_test(mgl_sampler_objects_test sampler_objects tests/unit/sampler_objects.c)
    mgl_add_internal_test(mgl_sync_objects_test sync_objects tests/unit/sync_objects.c)
    mgl_add_internal_test(mgl_transform_feedback_test transform_feedback tests/unit/transform_feedback.c)
    mgl_add_internal_test(mgl_tf_replay_subroutine_stage_test tf_replay_subroutine_stage tests/unit/tf_replay_subroutine_stage.c)
    mgl_add_internal_test(mgl_dsa_buffers_test dsa_buffers tests/unit/dsa_buffers.c)
    mgl_add_internal_test(mgl_dsa_textures_test dsa_textures tests/unit/dsa_textures.c)
    mgl_add_internal_test(mgl_dsa_texture_readback_test dsa_texture_readback tests/unit/dsa_texture_readback.c)
    mgl_add_internal_test(mgl_texture_scalar_readback_test texture_scalar_readback tests/unit/texture_scalar_readback.c)
    mgl_add_internal_test(mgl_texture_readback_format_matrix_test texture_readback_format_matrix tests/unit/texture_readback_format_matrix.c)
    mgl_add_internal_test(mgl_dsa_texture_mutation_test dsa_texture_mutation tests/unit/dsa_texture_mutation.c)
    mgl_add_internal_test(mgl_dsa_framebuffers_test dsa_framebuffers tests/unit/dsa_framebuffers.c)
    mgl_add_internal_test(mgl_arb_programs_test arb_programs tests/unit/arb_programs.c)
    mgl_add_internal_test(mgl_arb_program_parameter_semantics_test arb_program_parameter_semantics tests/unit/arb_program_parameter_semantics.c)
    mgl_add_internal_test(mgl_arb_program_string_semantics_test arb_program_string_semantics tests/unit/arb_program_string_semantics.c)
    mgl_add_internal_test(mgl_arb_shader_objects_test arb_shader_objects tests/unit/arb_shader_objects.c)
    mgl_add_internal_test(mgl_extended_draws_test extended_draws tests/unit/extended_draws.c)
    mgl_add_internal_test(mgl_multisample_textures_test multisample_textures tests/unit/multisample_textures.c)
    mgl_add_internal_test(mgl_texture_3d_test texture_3d tests/unit/texture_3d.c)
    mgl_add_internal_test(mgl_texture_2d_array_test texture_2d_array tests/unit/texture_2d_array.c)
    mgl_add_internal_test(mgl_texture_buffer_test texture_buffer tests/unit/texture_buffer.c)
    mgl_add_internal_test(mgl_texture_buffer_range_test texture_buffer_range tests/unit/texture_buffer_range.c)
    mgl_add_internal_test(mgl_texture_storage_test texture_storage tests/unit/texture_storage.c)
    mgl_add_internal_test(mgl_texture_storage_multisample_test texture_storage_multisample tests/unit/texture_storage_multisample.c)
    mgl_add_internal_test(mgl_texture_buffer_runtime_test texture_buffer_runtime tests/unit/texture_buffer_runtime.c)
    mgl_add_internal_test(mgl_cube_texture_test cube_texture tests/unit/cube_texture.c)
    mgl_add_internal_test(mgl_debug_output_test debug_output tests/unit/debug_output.c)
    mgl_add_internal_test(mgl_compute_execution_test compute_execution tests/unit/compute_execution.c)
    mgl_add_internal_test(mgl_image_binding_test image_binding tests/unit/image_binding.c)
    mgl_add_internal_test(mgl_gl44_texture_multibind_test gl44_texture_multibind tests/unit/gl44_texture_multibind.c)
    mgl_add_internal_test(mgl_gl45_create_vertex_arrays_test gl45_create_vertex_arrays tests/unit/gl45_create_vertex_arrays.c)
    mgl_add_internal_test(mgl_gl43_vertex_binding_test gl43_vertex_binding tests/unit/gl43_vertex_binding.c)
    mgl_add_internal_test(mgl_program_reflection_test program_reflection tests/unit/program_reflection.c)
    mgl_add_internal_test(mgl_atomic_counter_reflection_test atomic_counter_reflection tests/unit/atomic_counter_reflection.c)
    mgl_add_internal_test(mgl_fragment_output_bindings_test fragment_output_bindings tests/unit/fragment_output_bindings.c)
    mgl_add_internal_test(mgl_blend_extended_test blend_extended tests/unit/blend_extended.c)
    mgl_add_internal_test(mgl_uniform_families_test uniform_families tests/unit/uniform_families.c)
    mgl_add_internal_test(mgl_robust_uniform_readback_test robust_uniform_readback tests/unit/robust_uniform_readback.c)
    mgl_add_internal_test(mgl_program_uniform_families_test program_uniform_families tests/unit/program_uniform_families.c)
    mgl_add_internal_test(mgl_integer_vertex_attribs_test integer_vertex_attribs tests/unit/integer_vertex_attribs.c)
    mgl_add_internal_test(mgl_program_binary_test program_binary tests/unit/program_binary.c)
    mgl_add_internal_test(mgl_capability_reporting_test capability_reporting tests/unit/capability_reporting.c)
    mgl_add_internal_test(mgl_native_dispatch_gpu_identity_test native_gpu_identity tests/unit/native_gpu_identity.c)
    mgl_add_internal_test(mgl_ext_dsa_matrix_test ext_dsa_matrix tests/unit/ext_dsa_matrix.c)
    mgl_add_internal_test(mgl_ext_dsa_client_state_test ext_dsa_client_state tests/unit/ext_dsa_client_state.c)
    mgl_add_internal_test(mgl_ext_dsa_multitex_state_test ext_dsa_multitex_state tests/unit/ext_dsa_multitex_state.c)
    mgl_add_internal_test(mgl_ext_dsa_multitex_queries_test ext_dsa_multitex_queries tests/unit/ext_dsa_multitex_queries.c)
    mgl_add_internal_test(mgl_ext_dsa_multitex_mutation_test ext_dsa_multitex_mutation tests/unit/ext_dsa_multitex_mutation.c)
    mgl_add_internal_test(mgl_ext_dsa_multitex_copy_test ext_dsa_multitex_copy tests/unit/ext_dsa_multitex_copy.c)
    mgl_add_internal_test(mgl_ext_dsa_texture_copy_test ext_dsa_texture_copy tests/unit/ext_dsa_texture_copy.c)
    mgl_add_internal_test(mgl_ext_dsa_texture_mutation_test ext_dsa_texture_mutation tests/unit/ext_dsa_texture_mutation.c)
    mgl_add_internal_test(mgl_ext_dsa_texture_compressed_test ext_dsa_texture_compressed tests/unit/ext_dsa_texture_compressed.c)
    mgl_add_internal_test(mgl_ext_dsa_texture_queries_test ext_dsa_texture_queries tests/unit/ext_dsa_texture_queries.c)
    mgl_add_internal_test(mgl_ext_dsa_texture_parameters_test ext_dsa_texture_parameters tests/unit/ext_dsa_texture_parameters.c)
    mgl_add_internal_test(mgl_ext_dsa_texture_integer_parameters_test ext_dsa_texture_integer_parameters tests/unit/ext_dsa_texture_integer_parameters.c)
    mgl_add_internal_test(mgl_ext_dsa_multitex_buffer_test ext_dsa_multitex_buffer tests/unit/ext_dsa_multitex_buffer.c)
    mgl_add_internal_test(mgl_ext_dsa_mipmap_test ext_dsa_mipmap tests/unit/ext_dsa_mipmap.c)
    mgl_add_internal_test(mgl_ext_dsa_texture_buffer_test ext_dsa_texture_buffer tests/unit/ext_dsa_texture_buffer.c)
    mgl_add_internal_test(mgl_ext_dsa_program_uniform_test ext_dsa_program_uniform tests/unit/ext_dsa_program_uniform.c)
    mgl_add_internal_test(mgl_indexed_state_test indexed_state tests/unit/indexed_state.c)
    mgl_add_internal_test(mgl_clear_buffer_test clear_buffer tests/unit/clear_buffer.c)
    mgl_add_internal_test(mgl_clear_state_semantics_test clear_state_semantics tests/unit/clear_state_semantics.c)
    mgl_add_internal_test(mgl_framebuffer_buffer_state_test framebuffer_buffer_state tests/unit/framebuffer_buffer_state.c)
    mgl_add_internal_test(mgl_raster_state_extended_test raster_state_extended tests/unit/raster_state_extended.c)
    mgl_add_internal_test(mgl_begin_end_state_legality_test begin_end_state_legality tests/unit/begin_end_state_legality.c)
    mgl_add_internal_test(mgl_flush_finish_semantics_test flush_finish_semantics tests/unit/flush_finish_semantics.c)
    mgl_add_internal_test(mgl_object_is_semantics_test object_is_semantics tests/unit/object_is_semantics.c)
    mgl_add_internal_test(mgl_object_binding_semantics_test object_binding_semantics tests/unit/object_binding_semantics.c)
    mgl_add_internal_test(mgl_object_lifecycle_semantics_test object_lifecycle_semantics tests/unit/object_lifecycle_semantics.c)
    mgl_add_internal_test(mgl_query_selector_semantics_test query_selector_semantics tests/unit/query_selector_semantics.c)
    mgl_add_internal_test(mgl_hint_state_test hint_state tests/unit/hint_state.c)
    mgl_add_internal_test(mgl_texture_default_params_test texture_default_params tests/unit/texture_default_params.c)
    mgl_add_internal_test(mgl_texture_residency_test texture_residency tests/unit/texture_residency.c)
    mgl_add_internal_test(mgl_texture_border_params_test texture_border_params tests/unit/texture_border_params.c)
    mgl_add_internal_test(mgl_parity_buffer_pixel_test parity_buffer_pixel tests/unit/parity_buffer_pixel.c)
    mgl_add_internal_test(mgl_buffer_object_semantics_test buffer_object_semantics tests/unit/buffer_object_semantics.c)
    mgl_add_internal_test(mgl_pixel_store_layout_test pixel_store_layout tests/unit/pixel_store_layout.c)
    mgl_add_internal_test(mgl_integer_color_readback_test integer_color_readback tests/unit/integer_color_readback.c)
    mgl_add_internal_test(mgl_depth_stencil_packed_readback_test depth_stencil_packed_readback tests/unit/depth_stencil_packed_readback.c)
    mgl_add_internal_test(mgl_readback_immutable_pack_buffer_test readback_immutable_pack_buffer tests/unit/readback_immutable_pack_buffer.c)
    mgl_add_internal_test(mgl_indexed_buffer_multibind_test indexed_buffer_multibind tests/unit/indexed_buffer_multibind.c)
    mgl_add_internal_test(mgl_query_conversions_test query_conversions tests/unit/query_conversions.c)
    mgl_add_internal_test(mgl_compat_alias_parity_test compat_alias_parity tests/unit/compat_alias_parity.c)
    mgl_add_internal_test(mgl_legacy_attribute_overloads_test legacy_attribute_overloads tests/unit/legacy_attribute_overloads.c)
    mgl_add_internal_test(mgl_scratch_upload_reuse_test scratch_upload_reuse tests/unit/scratch_upload_reuse.c)
    mgl_add_internal_test(mgl_direct_passthrough_test direct_passthrough tests/unit/direct_passthrough.c)
    mgl_add_internal_test(mgl_dsa_vertex_array_attrib_test dsa_vertex_array_attrib tests/unit/dsa_vertex_array_attrib.c)
    mgl_add_internal_test(mgl_uniform_api_parity_test uniform_api_parity tests/unit/uniform_api_parity.c)
    mgl_add_internal_test(mgl_bind_multitexture_test bind_multitexture tests/unit/bind_multitexture.c)
    mgl_add_internal_test(mgl_programmable_texture_limits_test programmable_texture_limits tests/unit/programmable_texture_limits.c)
    mgl_add_internal_test(mgl_native_active_texture_cache_test native_active_texture_cache tests/unit/native_active_texture_cache.c)
    mgl_add_internal_test(mgl_native_buffer_binding_cache_test native_buffer_binding_cache tests/unit/native_buffer_binding_cache.c)
    mgl_add_internal_test(mgl_angle_client_arrays_enable_test angle_client_arrays_enable tests/unit/angle_client_arrays_enable.c)
    mgl_add_internal_test(mgl_angle_lazy_buffer_shadow_test angle_lazy_buffer_shadow tests/unit/angle_lazy_buffer_shadow.c)
    mgl_add_internal_test(mgl_angle_buffer_native_forward_test angle_buffer_native_forward tests/unit/angle_buffer_native_forward.c)

    add_library(mgl_test_native_egl SHARED tests/support/native_egl_provider.c)
    target_include_directories(mgl_test_native_egl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src)
    set_target_properties(mgl_test_native_egl PROPERTIES PREFIX "" OUTPUT_NAME "mgl_test_native_egl")

    add_executable(mgl_egl_lifecycle_test tests/unit/egl_lifecycle.c)
    target_include_directories(mgl_egl_lifecycle_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(mgl_egl_lifecycle_test PRIVATE mgl_egl mgl)
    add_test(NAME egl_lifecycle COMMAND mgl_egl_lifecycle_test)
    set_tests_properties(egl_lifecycle PROPERTIES ENVIRONMENT "MGL_EGL_LIBRARY=$<TARGET_FILE:mgl_test_native_egl>;MGL_GLES_LIBRARY=$<TARGET_FILE:mgl_test_native_egl>")

    add_executable(mgl_egl_wait_sync_test tests/unit/egl_wait_sync.c)
    target_include_directories(mgl_egl_wait_sync_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(mgl_egl_wait_sync_test PRIVATE mgl_egl Threads::Threads ${CMAKE_DL_LIBS})
    add_test(NAME egl_wait_sync COMMAND mgl_egl_wait_sync_test $<TARGET_FILE:mgl_test_native_egl>)
    set_tests_properties(egl_wait_sync PROPERTIES ENVIRONMENT "MGL_EGL_LIBRARY=$<TARGET_FILE:mgl_test_native_egl>;MGL_GLES_LIBRARY=$<TARGET_FILE:mgl_test_native_egl>")

    add_executable(mgl_egl_fence_sync_test tests/unit/egl_fence_sync.c)
    target_include_directories(mgl_egl_fence_sync_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(mgl_egl_fence_sync_test PRIVATE mgl_egl ${CMAKE_DL_LIBS})
    add_test(NAME egl_fence_sync COMMAND mgl_egl_fence_sync_test $<TARGET_FILE:mgl_test_native_egl>)
    set_tests_properties(egl_fence_sync PROPERTIES ENVIRONMENT "MGL_EGL_LIBRARY=$<TARGET_FILE:mgl_test_native_egl>;MGL_GLES_LIBRARY=$<TARGET_FILE:mgl_test_native_egl>")

    add_executable(mgl_egl_context_attributes_test tests/unit/egl_context_attributes.c)
    target_include_directories(mgl_egl_context_attributes_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(mgl_egl_context_attributes_test PRIVATE mgl_egl mgl)
    add_test(NAME egl_context_attributes COMMAND mgl_egl_context_attributes_test)
    set_tests_properties(egl_context_attributes PROPERTIES ENVIRONMENT "MGL_EGL_LIBRARY=$<TARGET_FILE:mgl_test_native_egl>;MGL_GLES_LIBRARY=$<TARGET_FILE:mgl_test_native_egl>")

    find_program(MGL_XVFB_RUN_EXECUTABLE xvfb-run)
    if(MGL_XVFB_RUN_EXECUTABLE)
        # GLX screen queries require a live X display.
        add_executable(mgl_glx_context_copy_test tests/unit/glx_context_copy.c)
    target_include_directories(mgl_glx_context_copy_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src ${MGL_X11_INCLUDE_DIRS})
    target_link_libraries(mgl_glx_context_copy_test PRIVATE mgl_test_runtime ${MGL_X11_LIBRARIES})
    add_test(NAME glx_context_copy COMMAND mgl_glx_context_copy_test)
    set_tests_properties(glx_context_copy PROPERTIES ENVIRONMENT "MGL_EGL_LIBRARY=$<TARGET_FILE:mgl_test_native_egl>;MGL_GLES_LIBRARY=$<TARGET_FILE:mgl_test_native_egl>")

    if(MGL_XVFB_RUN_EXECUTABLE)
        add_executable(mgl_glx_error_delivery_test tests/unit/glx_error_delivery.c)
        target_include_directories(mgl_glx_error_delivery_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${MGL_X11_INCLUDE_DIRS})
        target_link_libraries(mgl_glx_error_delivery_test PRIVATE mgl_test_runtime ${MGL_X11_LIBRARIES})
        add_test(NAME glx_error_delivery COMMAND ${MGL_XVFB_RUN_EXECUTABLE} -a $<TARGET_FILE:mgl_glx_error_delivery_test>)
    endif()

    add_executable(mgl_glx_screen_queries_test tests/unit/glx_screen_queries.c)
        target_include_directories(mgl_glx_screen_queries_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${MGL_X11_INCLUDE_DIRS})
        target_link_libraries(mgl_glx_screen_queries_test PRIVATE mgl_test_runtime ${MGL_X11_LIBRARIES})
        add_test(NAME glx_screen_queries COMMAND ${MGL_XVFB_RUN_EXECUTABLE} -a $<TARGET_FILE:mgl_glx_screen_queries_test>)

        add_executable(mgl_glx_wait_sync_test tests/unit/glx_wait_sync.c)
        target_include_directories(mgl_glx_wait_sync_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src ${MGL_X11_INCLUDE_DIRS})
        target_link_libraries(mgl_glx_wait_sync_test PRIVATE mgl_test_runtime ${MGL_X11_LIBRARIES} ${CMAKE_DL_LIBS})
        add_test(NAME glx_wait_sync COMMAND ${MGL_XVFB_RUN_EXECUTABLE} -a $<TARGET_FILE:mgl_glx_wait_sync_test>)
        set_tests_properties(glx_wait_sync PROPERTIES ENVIRONMENT "MGL_EGL_LIBRARY=$<TARGET_FILE:mgl_test_native_egl>;MGL_GLES_LIBRARY=$<TARGET_FILE:mgl_test_native_egl>")

        add_executable(mgl_glx_legacy_pixmap_test tests/unit/glx_legacy_pixmap.c)
        target_include_directories(mgl_glx_legacy_pixmap_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src ${MGL_X11_INCLUDE_DIRS})
        target_link_libraries(mgl_glx_legacy_pixmap_test PRIVATE mgl_test_runtime ${MGL_X11_LIBRARIES})
        add_test(NAME glx_legacy_pixmap COMMAND ${MGL_XVFB_RUN_EXECUTABLE} -a $<TARGET_FILE:mgl_glx_legacy_pixmap_test>)

        add_executable(mgl_glx_init_probe_test tests/unit/glx_init_probe.c)
        target_include_directories(mgl_glx_init_probe_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src ${MGL_X11_INCLUDE_DIRS})
        target_link_libraries(mgl_glx_init_probe_test PRIVATE mgl_test_runtime ${MGL_X11_LIBRARIES})
        add_test(NAME glx_init_probe COMMAND ${MGL_XVFB_RUN_EXECUTABLE} -a $<TARGET_FILE:mgl_glx_init_probe_test>)
        set_tests_properties(glx_init_probe PROPERTIES ENVIRONMENT "MGL_EGL_LIBRARY=$<TARGET_FILE:mgl_test_native_egl>;MGL_GLES_LIBRARY=$<TARGET_FILE:mgl_test_native_egl>")

        add_executable(mgl_glx_drawable_consistency_test tests/unit/glx_drawable_consistency.c)
        target_include_directories(mgl_glx_drawable_consistency_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src ${MGL_X11_INCLUDE_DIRS})
        target_link_libraries(mgl_glx_drawable_consistency_test PRIVATE mgl_test_runtime ${MGL_X11_LIBRARIES})
        add_test(NAME glx_drawable_consistency COMMAND ${MGL_XVFB_RUN_EXECUTABLE} -a $<TARGET_FILE:mgl_glx_drawable_consistency_test>)
        set_tests_properties(glx_drawable_consistency PROPERTIES ENVIRONMENT "MGL_EGL_LIBRARY=$<TARGET_FILE:mgl_test_native_egl>;MGL_GLES_LIBRARY=$<TARGET_FILE:mgl_test_native_egl>")

        add_executable(mgl_glx_swap_control_test tests/unit/glx_swap_control.c)
        target_include_directories(mgl_glx_swap_control_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src ${MGL_X11_INCLUDE_DIRS})
        target_link_libraries(mgl_glx_swap_control_test PRIVATE mgl_test_runtime ${MGL_X11_LIBRARIES} ${CMAKE_DL_LIBS})
        add_test(NAME glx_swap_control COMMAND ${MGL_XVFB_RUN_EXECUTABLE} -a $<TARGET_FILE:mgl_glx_swap_control_test>)
        set_tests_properties(glx_swap_control PROPERTIES ENVIRONMENT "MGL_EGL_LIBRARY=$<TARGET_FILE:mgl_test_native_egl>;MGL_GLES_LIBRARY=$<TARGET_FILE:mgl_test_native_egl>")

        add_executable(mgl_glx_use_x_font_test tests/unit/glx_use_x_font.c)
        target_include_directories(mgl_glx_use_x_font_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src ${MGL_X11_INCLUDE_DIRS})
        target_link_libraries(mgl_glx_use_x_font_test PRIVATE mgl_test_runtime ${MGL_X11_LIBRARIES} ${CMAKE_DL_LIBS})
        add_test(NAME glx_use_x_font COMMAND ${MGL_XVFB_RUN_EXECUTABLE} -a $<TARGET_FILE:mgl_glx_use_x_font_test>)
        set_tests_properties(glx_use_x_font PROPERTIES ENVIRONMENT "MGL_EGL_LIBRARY=$<TARGET_FILE:mgl_test_native_egl>;MGL_GLES_LIBRARY=$<TARGET_FILE:mgl_test_native_egl>")

        add_executable(mgl_egl_platform_extensions_test tests/unit/egl_platform_extensions.c)
        target_include_directories(mgl_egl_platform_extensions_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${MGL_X11_INCLUDE_DIRS})
        target_link_libraries(mgl_egl_platform_extensions_test PRIVATE mgl_egl ${MGL_X11_LIBRARIES})
        add_test(NAME egl_platform_extensions COMMAND ${MGL_XVFB_RUN_EXECUTABLE} -a $<TARGET_FILE:mgl_egl_platform_extensions_test>)
        set_tests_properties(egl_platform_extensions PROPERTIES
            ENVIRONMENT "MGL_EGL_LIBRARY=$<TARGET_FILE:mgl_test_native_egl>;MGL_GLES_LIBRARY=$<TARGET_FILE:mgl_test_native_egl>"
            RUN_SERIAL TRUE)

        add_executable(mgl_egl_client_contract_test tests/unit/egl_client_contract.c)
        target_include_directories(mgl_egl_client_contract_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${MGL_X11_INCLUDE_DIRS})
        target_link_libraries(mgl_egl_client_contract_test PRIVATE mgl_egl ${MGL_X11_LIBRARIES} ${CMAKE_DL_LIBS})
        add_test(NAME egl_client_contract COMMAND ${MGL_XVFB_RUN_EXECUTABLE} -a $<TARGET_FILE:mgl_egl_client_contract_test>)
        set_tests_properties(egl_client_contract PROPERTIES ENVIRONMENT "MGL_EGL_LIBRARY=$<TARGET_FILE:mgl_test_native_egl>;MGL_GLES_LIBRARY=$<TARGET_FILE:mgl_test_native_egl>")

        add_executable(mgl_egl_surface_attributes_test tests/unit/egl_surface_attributes.c)
        target_include_directories(mgl_egl_surface_attributes_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${MGL_X11_INCLUDE_DIRS})
        target_link_libraries(mgl_egl_surface_attributes_test PRIVATE mgl_egl ${MGL_X11_LIBRARIES})
        add_test(NAME egl_surface_attributes COMMAND ${MGL_XVFB_RUN_EXECUTABLE} -a $<TARGET_FILE:mgl_egl_surface_attributes_test>)
        set_tests_properties(egl_surface_attributes PROPERTIES ENVIRONMENT "MGL_EGL_LIBRARY=$<TARGET_FILE:mgl_test_native_egl>;MGL_GLES_LIBRARY=$<TARGET_FILE:mgl_test_native_egl>")

        add_executable(mgl_egl_direct_present_test tests/unit/egl_direct_present.c)
        target_include_directories(mgl_egl_direct_present_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${MGL_X11_INCLUDE_DIRS})
        target_link_libraries(mgl_egl_direct_present_test PRIVATE mgl_egl ${MGL_X11_LIBRARIES} ${CMAKE_DL_LIBS})
        add_test(NAME egl_direct_present COMMAND ${MGL_XVFB_RUN_EXECUTABLE} -a $<TARGET_FILE:mgl_egl_direct_present_test>)
        set_tests_properties(egl_direct_present PROPERTIES ENVIRONMENT "MGL_EGL_LIBRARY=$<TARGET_FILE:mgl_test_native_egl>;MGL_GLES_LIBRARY=$<TARGET_FILE:mgl_test_native_egl>;WINE_ANDROID_DIRECT_PRESENT_LIB=$<TARGET_FILE:mgl_test_direct_present_bridge>;WINE_ANDROID_GL_SWAP_INTERVAL=3")
    endif()

    if(UNIX AND NOT APPLE)
        add_library(mgl_test_native_android_vulkan_mock SHARED tests/support/native_android_vulkan_mock.c)
        add_library(mgl_test_wrapper_vulkan_mock SHARED tests/support/wrapper_vulkan_mock.c)
        add_library(mgl_test_native_android_angle_mock SHARED tests/support/native_android_angle_mock.c)
        add_executable(mgl_android_vulkan_preload_test tests/unit/android_vulkan_preload.c src/native/loader.c)
        target_include_directories(mgl_android_vulkan_preload_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(mgl_android_vulkan_preload_test PRIVATE ${CMAKE_DL_LIBS})
        add_test(NAME android_vulkan_preload COMMAND mgl_android_vulkan_preload_test
            $<TARGET_FILE:mgl_test_native_android_vulkan_mock>
            $<TARGET_FILE:mgl_test_wrapper_vulkan_mock>
            $<TARGET_FILE:mgl_test_native_android_angle_mock>
            $<TARGET_FILE:mgl_egl>)
        add_test(NAME android_vulkan_preemption_warning COMMAND mgl_android_vulkan_preload_test
            $<TARGET_FILE:mgl_test_native_android_vulkan_mock>
            $<TARGET_FILE:mgl_test_wrapper_vulkan_mock>
            $<TARGET_FILE:mgl_test_native_android_angle_mock>
            $<TARGET_FILE:mgl_egl> --global-wrapper)
    endif()

    add_executable(mgl_egl_native_self_filter_test tests/unit/egl_native_self_filter.c src/native/loader.c)
    target_include_directories(mgl_egl_native_self_filter_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(mgl_egl_native_self_filter_test PRIVATE ${CMAKE_DL_LIBS})
    add_test(NAME egl_native_self_filter COMMAND mgl_egl_native_self_filter_test $<TARGET_FILE:mgl_egl> $<TARGET_FILE:mgl_test_native_egl>)

    add_library(mgl_test_android_ahb SHARED tests/unit/ahb_presentation_android_stub.c)
    set_target_properties(mgl_test_android_ahb PROPERTIES OUTPUT_NAME android)
    add_executable(mgl_ahb_presentation_android_test tests/unit/ahb_presentation_android.c)
    target_include_directories(mgl_ahb_presentation_android_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(mgl_ahb_presentation_android_test PRIVATE mgl_test_runtime ${CMAKE_DL_LIBS})
    add_test(NAME ahb_presentation_android COMMAND mgl_ahb_presentation_android_test)
    set_tests_properties(ahb_presentation_android PROPERTIES
        ENVIRONMENT "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:mgl_test_android_ahb>")

    add_library(mgl_test_direct_present_bridge SHARED tests/support/direct_present_bridge.c)
    set_target_properties(mgl_test_direct_present_bridge PROPERTIES PREFIX "" OUTPUT_NAME "mgl_test_direct_present_bridge")
    add_executable(mgl_presentation_direct_test tests/unit/presentation_direct.c)
    target_include_directories(mgl_presentation_direct_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(mgl_presentation_direct_test PRIVATE mgl_test_runtime ${CMAKE_DL_LIBS})
    add_test(NAME presentation_direct COMMAND mgl_presentation_direct_test)
    set_tests_properties(presentation_direct PROPERTIES ENVIRONMENT "WINE_ANDROID_DIRECT_PRESENT_LIB=$<TARGET_FILE:mgl_test_direct_present_bridge>;MGL_EGL_LIBRARY=$<TARGET_FILE:mgl_test_native_egl>;MGL_GLES_LIBRARY=$<TARGET_FILE:mgl_test_native_egl>")
endif()

install(TARGETS mgl mgl_egl LIBRARY DESTINATION lib)
install(DIRECTORY include/mgl DESTINATION include)

if(TARGET mgl_test_runtime)
    add_executable(mgl_accum_without_buffer_test tests/unit/accum_without_buffer.c)
    target_include_directories(mgl_accum_without_buffer_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(mgl_accum_without_buffer_test PRIVATE mgl_test_runtime)
    add_test(NAME accum_without_buffer COMMAND mgl_accum_without_buffer_test)
endif()

if(TARGET mgl_test_runtime)
    add_executable(mgl_feedback_mode_test tests/unit/feedback_mode.c)
    target_include_directories(mgl_feedback_mode_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(mgl_feedback_mode_test PRIVATE mgl_test_runtime)
    add_test(NAME feedback_mode COMMAND mgl_feedback_mode_test)
endif()

if(TARGET mgl_test_runtime)
    add_executable(mgl_legacy_raster_state_test tests/unit/legacy_raster_state.c)
    target_include_directories(mgl_legacy_raster_state_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(mgl_legacy_raster_state_test PRIVATE mgl_test_runtime)
    add_test(NAME legacy_raster_state COMMAND mgl_legacy_raster_state_test)
endif()
