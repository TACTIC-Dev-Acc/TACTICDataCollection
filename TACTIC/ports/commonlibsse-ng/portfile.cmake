vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO alandtse/CommonLibVR
    REF 9ea71c54883e65fcafeda3040ccc2bc02a5c9cc6
    SHA512 93dc6cb371fb494bfaeb024ddbc54ff3396bbe0db332de920b95cd9d6dacfdb6d322f7ff0d156bae89a04a685e1648fd9d3457f9ad44dab2607c9de55211d84e
    HEAD_REF ng
)

vcpkg_configure_cmake(
    SOURCE_PATH "${SOURCE_PATH}"
    PREFER_NINJA
    OPTIONS
        -DSKSE_SUPPORT_XBYAK=ON
        -DBUILD_TESTS=OFF
        -DENABLE_SKYRIM_VR=OFF
)

vcpkg_install_cmake()

vcpkg_cmake_config_fixup(
    PACKAGE_NAME CommonLibSSE
    CONFIG_PATH lib/cmake
)

# Install the helper module that upstream forgets to install.
file(INSTALL
    "${SOURCE_PATH}/cmake/CommonLibSSE.cmake"
    DESTINATION "${CURRENT_PACKAGES_DIR}/share/CommonLibSSE"
)

# Overwrite the broken upstream package config.
set(CONFIG_FILE
    "${CURRENT_PACKAGES_DIR}/share/CommonLibSSE/CommonLibSSEConfig.cmake"
)

file(WRITE "${CONFIG_FILE}" [=[
include(CMakeFindDependencyMacro)

find_dependency(spdlog CONFIG REQUIRED)
find_dependency(directxtk CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/CommonLibSSE-targets.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/CommonLibSSE.cmake")
]=])

vcpkg_copy_pdbs()

file(GLOB CMAKE_CONFIGS
    "${CURRENT_PACKAGES_DIR}/share/CommonLibSSE/CommonLibSSE/*.cmake"
)
file(INSTALL
    ${CMAKE_CONFIGS}
    DESTINATION "${CURRENT_PACKAGES_DIR}/share/CommonLibSSE"
)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/share/CommonLibSSE/CommonLibSSE")

file(
    INSTALL "${SOURCE_PATH}/LICENSE"
    DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}"
    RENAME copyright
)