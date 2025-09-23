add_library(DRAMUtils INTERFACE)
target_include_directories(DRAMUtils INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/DRAMUtils")

target_compile_features(DRAMUtils INTERFACE cxx_std_17)
set_target_properties(DRAMUtils PROPERTIES CXX_EXTENSIONS OFF)
set_target_properties(DRAMUtils PROPERTIES CXX_STANDARD_REQUIRED ON)

target_link_libraries(DRAMUtils
    INTERFACE
        nlohmann_json::nlohmann_json
)

add_library(DRAMUtils::DRAMUtils ALIAS DRAMUtils)
