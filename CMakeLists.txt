cmake_minimum_required(VERSION 3.14)
project(bcp CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)
include_directories(src)

set(CADICAL_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/external/cadical/include" CACHE PATH
        "Directory containing cadical.hpp")
set(CADICAL_LIB "${CMAKE_SOURCE_DIR}/external/cadical/libcadical.a" CACHE FILEPATH
        "Path to the CaDiCaL static library")
if (NOT EXISTS "${CADICAL_INCLUDE_DIR}/cadical.hpp")
    message(FATAL_ERROR "CaDiCaL header not found: ${CADICAL_INCLUDE_DIR}/cadical.hpp")
endif ()
if (NOT EXISTS "${CADICAL_LIB}")
    message(FATAL_ERROR "CaDiCaL library not found: ${CADICAL_LIB}")
endif ()
include_directories(${CADICAL_INCLUDE_DIR})

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

if (BUILD_TESTING)
    include(FetchContent)
    FetchContent_Declare(
            googletest
            URL https://github.com/google/googletest/archive/063de7e9578f82b369302001269680b4b1553359.zip
            URL_HASH SHA256=f933817755e14daf4afa36230b994c5e9ab5a0476dbd659b1d70163e876b4b91
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(googletest)

    add_executable(bcp_tests
            test/test_upper_bound.cpp
            test/test_small_graphs.cpp
            test/test_one_var_greater.cpp
            test/test_one_var_less.cpp
            test/test_two_vars_greater.cpp
            test/test_two_vars_less.cpp
            test/test_staircase_aux_nocache.cpp
            test/test_staircase_aux_cache.cpp
            test/test_staircase_no_aux.cpp
            ${CORE_SOURCES}
            ${METHOD_SOURCES}
            ${ENCODER_SOURCES}
    )

    target_compile_definitions(bcp_tests PRIVATE
            BCP_DATASET_DIR="${CMAKE_SOURCE_DIR}/dataset"
    )

    target_link_libraries(bcp_tests
            PRIVATE
            ${CADICAL_LIB}
            GTest::gtest_main
    )

    include(GoogleTest)
    gtest_discover_tests(bcp_tests)
endif ()
