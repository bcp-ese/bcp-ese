cmake_minimum_required(VERSION 3.14)
project(bcp CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)
include_directories(external/cadical/include)
include_directories(src)

set(CADICAL_LIB ${CMAKE_SOURCE_DIR}/external/cadical/libcadical.a)

include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/heads/main.zip
)

FetchContent_MakeAvailable(googletest)

set(CORE_SOURCES
        src/sat_solver/SatSolver.cpp
        src/bcp_solver/utility.cpp
        src/bcp_solver/bcp_solver.cpp
        src/sat_solver/Cadical.cpp
        src/sat_solver/Cadical.h
)

set(METHOD_SOURCES
        src/bcp_solver/method/TwoVarsGreaterMethod.cpp
        src/bcp_solver/method/TwoVarsGreaterMethod.h
        src/bcp_solver/method/TwoVarsLessMethod.cpp
        src/bcp_solver/method/TwoVarsLessMethod.h
        src/bcp_solver/method/OneVarGreaterMethod.cpp
        src/bcp_solver/method/OneVarGreaterMethod.h
        src/bcp_solver/method/OneVarLessMethod.cpp
        src/bcp_solver/method/OneVarLessMethod.h
        src/bcp_solver/method/StaircaseWithAuxiliaryVarsMethod.cpp
        src/bcp_solver/method/StaircaseWithAuxiliaryVarsMethod.h
        src/bcp_solver/method/StaircaseWithoutAuxiliaryVarsMethod.cpp
        src/bcp_solver/method/StaircaseWithoutAuxiliaryVarsMethod.h
)

set(ENCODER_SOURCES
        src/card_encoder/clset.hh
        src/card_encoder/common.hh
        src/card_encoder/ptypes.hh
        src/card_encoder/seqcounter.hh
        src/card_encoder/utils.hh
)

add_executable(bcp
        src/main.cpp
        ${CORE_SOURCES}
        ${METHOD_SOURCES}
        ${ENCODER_SOURCES}
)

target_link_libraries(bcp PRIVATE ${CADICAL_LIB})

enable_testing()

add_executable(bcp_tests
        test/test_one_var_greater.cpp
        test/test_one_var_less.cpp
        test/test_two_vars_greater.cpp
        test/test_two_vars_less.cpp
        test/test_staircase_aux_nocache.cpp
        test/test_staircase_aux_cache.cpp
        test/test_staircase_no_aux.cpp
        # (header-only helper, no need to list)
        ${CORE_SOURCES}
        ${METHOD_SOURCES}
        ${ENCODER_SOURCES}
)

target_link_libraries(bcp_tests
        PRIVATE
        ${CADICAL_LIB}
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(bcp_tests)

add_custom_command(
        TARGET bcp_tests
        POST_BUILD
        COMMAND ctest --output-on-failure
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "------------------ RUNNING UNIT TESTS ------------------"
        VERBATIM
)