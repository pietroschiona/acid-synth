# CMakeLists.txt — acid_synth_plugin
cmake_minimum_required(VERSION 3.16)
project(acid_synth VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Build DSP kernel as static library
add_library(acid_dsp
    src/osc.c
    src/filter.c
    src/delay.c
    src/wavefolder.c
    src/reverb.c
    src/sequencer.c
    src/lfo.c
    src/synth.c
    src/scale.c
)

target_include_directories(acid_dsp PUBLIC src)

# Build tests
function(add_dsp_test NAME SOURCE)
    add_executable(${NAME} tests/${SOURCE})
    target_link_libraries(${NAME} acid_dsp)
    if(NOT MSVC)
        target_link_libraries(${NAME} m)
    endif()
    target_include_directories(${NAME} PRIVATE tests)
endfunction()

add_dsp_test(test_osc   test_osc.c)
add_dsp_test(test_filter  test_filter.c)
add_dsp_test(test_delay   test_delay.c)
add_dsp_test(test_wavefolder test_wavefolder.c)
add_dsp_test(test_reverb  test_reverb.c)
add_dsp_test(test_synth   test_synth.c)

# VST3 plugin (requires VST3 SDK — will be added later)
# add_library(acid_synth_vst3 SHARED src/vst3_wrapper.cpp)
# target_link_libraries(acid_synth_vst3 acid_dsp)
