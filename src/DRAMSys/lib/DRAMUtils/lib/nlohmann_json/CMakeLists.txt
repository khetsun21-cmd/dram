########################################
###          nlohmann_json           ###
########################################
FetchContent_Declare(nlohmann_json
    URL "https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz"
    OVERRIDE_FIND_PACKAGE
)

FetchContent_MakeAvailable(nlohmann_json)
