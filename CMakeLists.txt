set(CMAKE_POSITION_INDEPENDENT_CODE ON)
# --- 1. 憿寧�箸�蔭 ---
# �MAke��++��霈曄蔭
cmake_minimum_required(VERSION 3.22)
project(DramsysAxiSimulation CXX)
option(SC_FORCE_SHARED "Build SystemC/DRAMSys dependencies as shared libraries" ON)
if(SC_FORCE_SHARED)
    set(BUILD_SHARED_LIBS ON CACHE BOOL "Build shared libraries" FORCE)
endif()

# Mirror booksim2 style: allow external script-based build by default
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(build_type "rel")
elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(build_type "dbg")
elseif(CMAKE_BUILD_TYPE STREQUAL "DebugWithASan")
    set(build_type "dbg_asan")
else()
    set(build_type "rel")
endif()

option(DRAMSYS_NATIVE_INTEGRATION "Build DRAMSys/bridge natively in this CMake (OFF=script like booksim2)" OFF)

if(NOT DRAMSYS_NATIVE_INTEGRATION)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../build_dramsys.sh")
        add_custom_target(build_dramsys ALL
            COMMAND bash ../build_dramsys.sh ${build_type}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            VERBATIM)
        message(STATUS "DRAMSys external build enabled (booksim2 style): build_dramsys -> ../build_dramsys.sh ${build_type}")
    else()
        message(WARNING "External script ../build_dramsys.sh not found; switching to native integration.")
        set(DRAMSYS_NATIVE_INTEGRATION ON CACHE BOOL "Build DRAMSys/bridge natively in this CMake" FORCE)
    endif()
endif()

if(DRAMSYS_NATIVE_INTEGRATION)

# CMake policies / compatibility tweaks for dependencies
if(POLICY CMP0167)
    # Enable legacy FindBoost module for subprojects relying on it
    cmake_policy(SET CMP0167 OLD)
endif()

# Ensure subprojects that call their own cmake_minimum_required also inherit policy defaults
set(CMAKE_POLICY_DEFAULT_CMP0167 OLD)
set(CMAKE_POLICY_DEFAULT_CMP0144 NEW)

# Prefer using in-repo Boost if present (project root/boost or DRAMSys/lib/boost)
# This helps both this project and subprojects find headers/libs without system installs
set(_BOOST_CANDIDATES)
get_filename_component(_BOOST_CAND_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../boost" ABSOLUTE)
list(APPEND _BOOST_CANDIDATES "${_BOOST_CAND_ROOT}")
get_filename_component(_BOOST_CAND_DRAMSYS "${DRAMSYS_PATH}/lib/boost" ABSOLUTE)
list(APPEND _BOOST_CANDIDATES "${_BOOST_CAND_DRAMSYS}")

foreach(_cand IN LISTS _BOOST_CANDIDATES)
    if(EXISTS "${_cand}" AND EXISTS "${_cand}/boost")
        set(BOOST_ROOT "${_cand}" CACHE PATH "Boost root directory (in-repo)" FORCE)
        set(Boost_ROOT "${_cand}" CACHE PATH "" FORCE)
        set(Boost_INCLUDE_DIR "${_cand}" CACHE PATH "" FORCE)
        if(EXISTS "${_cand}/stage/lib")
            set(BOOST_LIBRARYDIR "${_cand}/stage/lib" CACHE PATH "" FORCE)
            set(Boost_LIBRARY_DIR "${_cand}/stage/lib" CACHE PATH "" FORCE)
        elseif(EXISTS "${_cand}/lib")
            set(BOOST_LIBRARYDIR "${_cand}/lib" CACHE PATH "" FORCE)
            set(Boost_LIBRARY_DIR "${_cand}/lib" CACHE PATH "" FORCE)
        endif()
        # Also export environment hints for nested subprojects using FindBoost
        set(ENV{BOOST_ROOT} "${_cand}")
        if(DEFINED BOOST_LIBRARYDIR)
            set(ENV{BOOST_LIBRARYDIR} "${BOOST_LIBRARYDIR}")
        endif()
        message(STATUS "Boost hint: using in-repo Boost at ${_cand}")
        break()
    endif()
endforeach()

# Prefer classic FindBoost over BoostConfig (more common with prebuilt Boost)
set(Boost_NO_BOOST_CMAKE ON CACHE BOOL "Prefer FindBoost module over BoostConfig" FORCE)
# Prefer multi-threaded libs; allow static or shared depending on what's available
set(Boost_USE_MULTITHREADED ON CACHE BOOL "" FORCE)
# You can flip this if you only built static libs with b2
set(Boost_USE_STATIC_LIBS OFF CACHE BOOL "Prefer dynamic Boost libs by default" FORCE)

# Optionally pre-discover Boost to pass hints to subprojects (quietly)
find_package(Boost 1.70 QUIET COMPONENTS system date_time filesystem)
if(NOT Boost_FOUND)
    message(STATUS "Boost 1.70+ not found yet (components: system, date_time, filesystem). Set Boost_ROOT/BOOST_ROOT to your local Boost or install Boost.")
endif()

# 霈拙�憿寧銋�曉�賑���?Find*.cmake 璅∪�
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
# 蝖桐�摮★�桐��賜誧�踹�賑�芸�銋� Find 璅∪�嚗� Cache 撘箏�雿��
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" CACHE STRING "Module search path (includes ${CMAKE_CURRENT_SOURCE_DIR}/cmake)" FORCE)
find_package(fmt QUIET)
find_package(spdlog QUIET)
find_package(yaml-cpp QUIET)

# Avoid install/export checks for bundled subprojects during this build
set(CMAKE_SKIP_INSTALL_RULES ON CACHE BOOL "Skip install() rules for subprojects" FORCE)

# 蝏�雿輻 C++20嚗噶鈭�銝�撌亦�嚗sl_model嚗��湛��踹� SystemC API �蝚血銝�
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# --- 2. 蝻���蝞∠� ---
# 瘙�蝻�摰��★嚗����冽�靘�蝷箔�憌
# -fPIC: ��雿蔭�隞��嚗笆鈭鈭怠��臬���?# -DSC_INCLUDE_DYNAMIC_PROCESSES: SystemC�冽�蝔��
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -DSC_INCLUDE_DYNAMIC_PROCESSES -D_GNU_SOURCE")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-parameter")

# �寞�遣蝐餃�瘛餃��孵���霂�敹
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Build type: Debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -ggdb")
elseif(CMAKE_BUILD_TYPE STREQUAL "DebugWithASan")
    message(STATUS "Build type: DebugWithASan")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -ggdb -fsanitize=address")
    link_libraries(asan)
endif()

# --- 3. 靘�憿寡楝敺�蝵?---
# �冽�閬�桀����萎耨�寡�鈭楝敺
set(SYSTEMC_PATH "../systemc-2.3.4" CACHE PATH "Path to SystemC root directory")
# ���砌�摨撣衣� DRAMSys 皞�雿蔭
set(DRAMSYS_PATH "${CMAKE_CURRENT_SOURCE_DIR}/src/DRAMSys" CACHE PATH "Path to DRAMSys root directory")
# 蝻箇��� DRAMSYS_PATH/lib/SystemC-Components嚗鋡怠隞方�閬�嚗����典隞方��曉�隡
# scc-sysc 銝�雿蛹靘�憿嫣蝙�剁��詨��撌脩宏�?
# �?SystemC 皞�撌脫�?DRAMSys 銝����悟 DRAMSys �遣撟嗆�靘?SystemC �格�
if(EXISTS "${DRAMSYS_PATH}/lib/systemc-2.3.4")
    get_filename_component(_ABS_SYSTEMC_PRE "${DRAMSYS_PATH}/lib/systemc-2.3.4" ABSOLUTE)
    set(DRAMSYS_USE_FETCH_CONTENT_SYSTEMC ON CACHE BOOL "" FORCE)
    set(FETCHCONTENT_SOURCE_DIR_SYSTEMCLANGUAGE "${_ABS_SYSTEMC_PRE}" CACHE PATH "" FORCE)
endif()

# --- 4. 靘�/FetchContent �★ ---
# 蝻箇��喲�典� FetchContent嚗��交隞�撌脰撣行�鈭��孵�皞�嚗��笆�批��臬笆摨��
set(DRAMSYS_USE_FETCH_CONTENT OFF CACHE BOOL "Enable FetchContent for DRAMSys")

# 憒� DRAMSys/lib/sqlite3 摮嚗�撘�舐 DRAMSys ���/�遣 SQLite3 �楝敺��踹�蝟餌�蝥批�鋆�韏
if(EXISTS "${DRAMSYS_PATH}/lib/sqlite3")
    set(DRAMSYS_USE_FETCH_CONTENT_SQLITE3 ON CACHE BOOL "Fetch SQLite3 for DRAMSys" FORCE)
else()
    set(DRAMSYS_USE_FETCH_CONTENT_SQLITE3 OFF CACHE BOOL "Fetch SQLite3 for DRAMSys" FORCE)
endif()

set(DRAMSYS_USE_FETCH_CONTENT_INTERNAL OFF CACHE BOOL "Fetch DRAMUtils/DRAMPower for DRAMSys")
set(DRAMSYS_USE_FETCH_CONTENT_SYSTEMC OFF CACHE BOOL "Fetch SystemC for DRAMSys")

# �臬撠?DRAMSys 雿蛹摮★�格�撱綽�add_subdirectory嚗�暺恕�喲隞乩��蝙�券�蝻�摨
set(DRAMSYS_USE_SUBPROJECT ON CACHE BOOL "Build DRAMSys from source via add_subdirectory (default ON)")

# �舫��湔��憸�霂� libdramsys 頝臬�
set(DRAMSYS_LIB "" CACHE FILEPATH "Path to prebuilt libdramsys (optional)")

## Arteris tlm2-interfaces
# 銝甇文��蝻�嚗銝箔���嗅仍�辣嚗 DRAMSys/SCC 蝡舀�靘��摰�
set(ARTERIS_PATH "${DRAMSYS_PATH}/lib/tlm2-interfaces" CACHE PATH "Path to tlm2-interfaces headers (default DRAMSys/lib/tlm2-interfaces)")

# 憒�銝� DRAMSys �瑕� SystemC嚗�撠�雿輻�砍 SystemC 撟嗆�靘?SystemC::systemc �格�
if(NOT DRAMSYS_USE_FETCH_CONTENT_SYSTEMC)
    find_library(systemc_lib
        NAMES systemc libsystemc.so libsystemc.a
        PATHS "${SYSTEMC_PATH}/build/src" "${SYSTEMC_PATH}/lib" "${SYSTEMC_PATH}/lib-linux64"
        NO_DEFAULT_PATH)
    if(NOT systemc_lib)
        message(FATAL_ERROR "SystemC library not found in path: ${SYSTEMC_PATH}")
    endif()
    if(NOT TARGET SystemC::systemc)
        add_library(SystemC::systemc UNKNOWN IMPORTED)
        set_target_properties(SystemC::systemc PROPERTIES
            IMPORTED_LOCATION "${systemc_lib}"
            INTERFACE_INCLUDE_DIRECTORIES "${SYSTEMC_PATH}/include;${SYSTEMC_PATH}/src"
        )
    endif()
endif()


# --- 5. 摰��函�隞輻��格� (�嫣蛹�遣�曹澈摨? ---
# 撠遛��祈澈�遣銝箔�銝芸鈭怠�嚗��臬�扯��辣
## 閫�� DRAMSys �格�嚗�/摮敶?憸�霂�
if(NOT DEFINED DRAMSYS_TARGET OR DRAMSYS_TARGET STREQUAL "")
    # A) �＆��鈭�蝻�摨?
    if(DRAMSYS_LIB)
        if(EXISTS "${DRAMSYS_LIB}")
            add_library(DRAMSys::libdramsys UNKNOWN IMPORTED)
            set_target_properties(DRAMSys::libdramsys PROPERTIES IMPORTED_LOCATION "${DRAMSYS_LIB}")
            set(DRAMSYS_TARGET DRAMSys::libdramsys)
            message(STATUS "Using prebuilt DRAMSys library (explicit): ${DRAMSYS_LIB}")
        else()
            message(FATAL_ERROR "DRAMSYS_LIB is set but file does not exist: ${DRAMSYS_LIB}")
        endif()
    endif()

    # B) 撠�撌脣�鋆� CMake �?
    if(NOT DRAMSYS_TARGET)
        find_package(DRAMSys QUIET CONFIG)
        if(TARGET DRAMSys::libdramsys)
            set(DRAMSYS_TARGET DRAMSys::libdramsys)
            message(STATUS "Found DRAMSys package: using DRAMSys::libdramsys")
        endif()
    endif()

    # C) �舫�雿蛹摮★�格�撱綽�隞�撘�?DRAMSYS_USE_SUBPROJECT嚗?
    if(NOT DRAMSYS_TARGET AND DRAMSYS_USE_SUBPROJECT AND EXISTS "${DRAMSYS_PATH}/CMakeLists.txt")
        message(STATUS "Adding DRAMSys as subdirectory from: ${DRAMSYS_PATH}")

        # 瘜冽�嚗��甇文�撘 sqlite3嚗ystemC-Components/third_party 撌脰撣血僎�勗 CMake 蝏�憭�

        if(EXISTS "${DRAMSYS_PATH}/lib/DRAMUtils")
            get_filename_component(_ABS_DRAMUTILS "${DRAMSYS_PATH}/lib/DRAMUtils" ABSOLUTE)
            set(DRAMSYS_USE_FETCH_CONTENT_INTERNAL ON CACHE BOOL "" FORCE)
            # 瘜冽�嚗etchContent �����典之�?
            set(FETCHCONTENT_SOURCE_DIR_DRAMUTILS "${_ABS_DRAMUTILS}" CACHE PATH "" FORCE)
            message(STATUS "Using local DRAMUtils source: ${FETCHCONTENT_SOURCE_DIR_DRAMUTILS}")
        endif()
        if(EXISTS "${DRAMSYS_PATH}/lib/DRAMPower")
            get_filename_component(_ABS_DRAMPOWER "${DRAMSYS_PATH}/lib/DRAMPower" ABSOLUTE)
            set(DRAMSYS_USE_FETCH_CONTENT_INTERNAL ON CACHE BOOL "" FORCE)
            set(FETCHCONTENT_SOURCE_DIR_DRAMPOWER "${_ABS_DRAMPOWER}" CACHE PATH "" FORCE)
            message(STATUS "Using local DRAMPower source: ${FETCHCONTENT_SOURCE_DIR_DRAMPOWER}")
        endif()

        if(EXISTS "${DRAMSYS_PATH}/lib/nlohmann_json")
            get_filename_component(_ABS_NLOHMANN "${DRAMSYS_PATH}/lib/nlohmann_json" ABSOLUTE)
            set(DRAMSYS_USE_FETCH_CONTENT_NLOHMANN_JSON ON CACHE BOOL "" FORCE)
            set(FETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON "${_ABS_NLOHMANN}" CACHE PATH "" FORCE)
            message(STATUS "Using local nlohmann_json source: ${FETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON}")
        endif()

        if(EXISTS "${DRAMSYS_PATH}/lib/systemc-2.3.4")
            get_filename_component(_ABS_SYSTEMC "${DRAMSYS_PATH}/lib/systemc-2.3.4" ABSOLUTE)
            set(DRAMSYS_USE_FETCH_CONTENT_SYSTEMC ON CACHE BOOL "" FORCE)
            set(FETCHCONTENT_SOURCE_DIR_SYSTEMCLANGUAGE "${_ABS_SYSTEMC}" CACHE PATH "" FORCE)
            message(STATUS "Using local SystemC source: ${FETCHCONTENT_SOURCE_DIR_SYSTEMCLANGUAGE}")
        endif()

        # Prefer local SQLite amalgamation if present to avoid network downloads
        if(EXISTS "${DRAMSYS_PATH}/lib/sqlite3")
            # Common layouts: lib/sqlite3/sqlite-amalgamation or sqlite-amalgamation-xxxxx
            set(_SQLITE_CANDIDATES
                "${DRAMSYS_PATH}/lib/sqlite3/sqlite-amalgamation"
            )
            file(GLOB _SQLITE_GLOB "${DRAMSYS_PATH}/lib/sqlite3/sqlite-amalgamation*")
            list(APPEND _SQLITE_CANDIDATES ${_SQLITE_GLOB})
            foreach(_cand ${_SQLITE_CANDIDATES})
                if(EXISTS "${_cand}/sqlite3.c")
                    get_filename_component(_ABS_SQLITE "${_cand}" ABSOLUTE)
                    set(DRAMSYS_USE_FETCH_CONTENT_SQLITE3 ON CACHE BOOL "" FORCE)
                    set(FETCHCONTENT_SOURCE_DIR_SQLITE3 "${_ABS_SQLITE}" CACHE PATH "" FORCE)
                    # Optional: avoid any network fetches once local is provided
                    set(FETCHCONTENT_FULLY_DISCONNECTED ON CACHE BOOL "" FORCE)
                    message(STATUS "Using local SQLite3 amalgamation: ${FETCHCONTENT_SOURCE_DIR_SQLITE3}")
                    break()
                endif()
            endforeach()
        endif()

        # �乩遙雿�靘�雿輻鈭?FetchContent嚗����餃��?
        if(DRAMSYS_USE_FETCH_CONTENT_INTERNAL OR DRAMSYS_USE_FETCH_CONTENT_SYSTEMC OR DRAMSYS_USE_FETCH_CONTENT_NLOHMANN_JSON OR DRAMSYS_USE_FETCH_CONTENT_SQLITE3)
            set(DRAMSYS_USE_FETCH_CONTENT ON CACHE BOOL "" FORCE)
        endif()

        # 撠��喃��� DRAMSys嚗雿���瑕�賭誘銵��亦�霈曄蔭嚗?
        set(DRAMSYS_USE_FETCH_CONTENT ${DRAMSYS_USE_FETCH_CONTENT} CACHE BOOL "" FORCE)
        set(DRAMSYS_USE_FETCH_CONTENT_INTERNAL ${DRAMSYS_USE_FETCH_CONTENT_INTERNAL} CACHE BOOL "" FORCE)
        set(DRAMSYS_USE_FETCH_CONTENT_NLOHMANN_JSON ${DRAMSYS_USE_FETCH_CONTENT_NLOHMANN_JSON} CACHE BOOL "" FORCE)
        set(DRAMSYS_USE_FETCH_CONTENT_SYSTEMC ${DRAMSYS_USE_FETCH_CONTENT_SYSTEMC} CACHE BOOL "" FORCE)

        # Ensure legacy SystemC CMake files request policies available in CMake 4.x
        set(_SC_POLICY_MIN_WAS_DEFINED FALSE)
        if(DEFINED CMAKE_POLICY_VERSION_MINIMUM)
            set(_SC_POLICY_MIN_WAS_DEFINED TRUE)
            set(_SC_PREV_POLICY_MIN "${CMAKE_POLICY_VERSION_MINIMUM}")
            if(CMAKE_POLICY_VERSION_MINIMUM VERSION_LESS "3.5")
                set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
            endif()
        else()
            set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
        endif()

        add_subdirectory(${DRAMSYS_PATH} ${CMAKE_CURRENT_BINARY_DIR}/dramsys_build)

        if(_SC_POLICY_MIN_WAS_DEFINED)
            set(CMAKE_POLICY_VERSION_MINIMUM "${_SC_PREV_POLICY_MIN}")
        else()
            unset(CMAKE_POLICY_VERSION_MINIMUM)
        endif()
        unset(_SC_POLICY_MIN_WAS_DEFINED)
        unset(_SC_PREV_POLICY_MIN)
        # �?DRAMSys �遣�箔� 'systemc' �格�����怠�嚗��遣�怠�隞亙摰嫣�皜?
        if(TARGET systemc AND NOT TARGET SystemC::systemc)
            add_library(SystemC::systemc ALIAS systemc)
        endif()
        if(TARGET DRAMSys::libdramsys)
            set(DRAMSYS_TARGET DRAMSys::libdramsys)
        endif()
    endif()

add_library(axi_dramsys_bridge SHARED
    src/AxiDramsysSystem.cpp
set(_bridge_test_src "${CMAKE_CURRENT_SOURCE_DIR}/tests/bridge_test.cpp")
if(EXISTS "${_bridge_test_src}")
    add_executable(bridge_test "${_bridge_test_src}")
    target_link_libraries(bridge_test PRIVATE SystemC::systemc axi_dramsys_bridge)
else()
    message(STATUS "Test source ${_bridge_test_src} not found; skipping bridge_test target")
endif()

set(_lpddr4_test_src "${CMAKE_CURRENT_SOURCE_DIR}/tests/dramsys_lpddr4_test.cpp")
if(EXISTS "${_lpddr4_test_src}")
    add_executable(dramsys_lpddr4_test "${_lpddr4_test_src}")
    target_link_libraries(dramsys_lpddr4_test PRIVATE SystemC::systemc axi_dramsys_bridge ${DRAMSYS_TARGET})
    target_include_directories(dramsys_lpddr4_test PRIVATE
        "${DRAMSYS_PATH}/src"
        "${DRAMSYS_PATH}/include")
    target_compile_definitions(dramsys_lpddr4_test PRIVATE SRC_ROOT_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
else()
    message(STATUS "Test source ${_lpddr4_test_src} not found; skipping dramsys_lpddr4_test target")
endif()
        endif()
    endif()

    if(NOT DRAMSYS_TARGET)
        message(FATAL_ERROR "DRAMSys not found. Options: (1) set DRAMSYS_LIB to prebuilt libdramsys; (2) install DRAMSys CMake package; (3) enable DRAMSYS_USE_SUBPROJECT and satisfy dependencies.")
    endif()
endif()

add_library(axi_dramsys_bridge STATIC
    src/AxiToTlmBridge.cpp
)

# --- 6. 霈曄蔭��桀� ---
target_include_directories(axi_dramsys_bridge
    PUBLIC
        # �祇★�桀仍�辣嚗�靘�霂亙���蝙�剁�
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/tlm2-interfaces"
    PRIVATE
        # DRAMSys 摨仍�辣嚗�摨頨恍�閬�
        "${DRAMSYS_PATH}/src/libdramsys"
        "${DRAMSYS_PATH}/include"
)

# --- 7. �暹摨?---
target_link_libraries(axi_dramsys_bridge PRIVATE SystemC::systemc ${DRAMSYS_TARGET})

# --- 9. 瘚��舀銵�摨?---
add_executable(bridge_test tests/bridge_test.cpp)
target_link_libraries(bridge_test PRIVATE SystemC::systemc axi_dramsys_bridge)

# DRAMSys LPDDR4 integration test (128B AXI -> segmented DRAM beats)
add_executable(dramsys_lpddr4_test tests/dramsys_lpddr4_test.cpp)
target_link_libraries(dramsys_lpddr4_test PRIVATE SystemC::systemc axi_dramsys_bridge ${DRAMSYS_TARGET})
target_include_directories(dramsys_lpddr4_test PRIVATE
    "${DRAMSYS_PATH}/src"
    "${DRAMSYS_PATH}/include")
target_compile_definitions(dramsys_lpddr4_test PRIVATE SRC_ROOT_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

# --- 8. 摰��格� ---
# 撠����曹澈摨�鋆���ib�桀�銝?install(TARGETS axi_dramsys_bridge LIBRARY DESTINATION lib)
endif() # DRAMSYS_NATIVE_INTEGRATION
