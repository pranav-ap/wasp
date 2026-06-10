cmake_minimum_required(VERSION 3.25)
project(wasp VERSION 0.1.0 DESCRIPTION "Wasp Programming Language Compiler")

# ==========================================
# Global Settings
# ==========================================
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Default to Debug build if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
endif()

# Enable strict warnings to catch bugs early!
# add_compile_options(-Wall -Wextra -Wpedantic)

# Use the lightning-fast LLD linker if available
add_link_options(-fuse-ld=lld)

# ==========================================
# Build Optimizations
# ==========================================

# Use CCache if installed
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()

# Enable Link Time Optimization (LTO) for Release builds
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT error)
if(ipo_supported)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
endif()

# ==========================================
# Third-Party Libraries
# ==========================================

# CLI11
add_library(CLI11 INTERFACE)
target_include_directories(CLI11 INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# cpptrace
include(FetchContent)
FetchContent_Declare(
  cpptrace
  GIT_REPOSITORY https://github.com/jeremy-rifkin/cpptrace.git
  GIT_TAG        v0.7.5 # Stable release
)
set(CPPTRACE_GET_SYMBOLS_WITH_LIBBACKTRACE ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(cpptrace)

# fmt (Modern string formatting)
FetchContent_Declare(
  fmt
  GIT_REPOSITORY https://github.com/fmtlib/fmt.git
  GIT_TAG        11.0.2 # Latest stable release
)
FetchContent_MakeAvailable(fmt)

# ==========================================
# Wasp Modules
# ==========================================

add_subdirectory(libs/doctor)
add_subdirectory(libs/workspace)
add_subdirectory(libs/token)
add_subdirectory(libs/lexer)
add_subdirectory(libs/ast)
add_subdirectory(libs/parser)
add_subdirectory(libs/object_system)
add_subdirectory(libs/semantics)
add_subdirectory(libs/captain)
add_subdirectory(libs/bytecode_system)
add_subdirectory(libs/compiler)
add_subdirectory(libs/vm)

add_subdirectory(src)

# ==========================================
# Testing
# ==========================================

if(PROJECT_IS_TOP_LEVEL)
    enable_testing()

    include(FetchContent)
    FetchContent_Declare(
      googletest
      GIT_REPOSITORY https://github.com/google/googletest.git
      GIT_TAG        v1.17.0
    )

    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    add_subdirectory(tests)
endif()

# ==========================================
# CPack Packaging
# ==========================================

# Only include CPack for main build (not for subprojects)
if(PROJECT_IS_TOP_LEVEL)

    # --- Package Metadata ---
    set(CPACK_PACKAGE_NAME "wasp")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "The Wasp Programming Language Compiler")
    set(CPACK_PACKAGE_DESCRIPTION "Wasp is a modern, statically-typed programming language with generics, traits, and algebraic types. It compiles to custom bytecode and executes on a high-performance virtual machine.")
    set(CPACK_PACKAGE_VENDOR "Pranav Athrapullikal")
    set(CPACK_PACKAGE_CONTACT "pranavap96@gmail.com")
    set(CPACK_PACKAGE_HOMEPAGE "https://github.com/pranav-ap/wasp")
    set(CPACK_PACKAGE_LICENSE "MIT")

    # --- Install Targets ---
    # Tell CMake what to put in the package
    install(TARGETS wasp
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
    )

    # Install standard library files
    install(DIRECTORY code/ DESTINATION share/wasp/code
        PATTERN "*.wasp"
        PATTERN ".gitkeep" EXCLUDE
    )

    # --- Platform-Specific CPack Generators ---
    if(WIN32)
        set(CPACK_GENERATOR "ZIP;NSIS")
        set(CPACK_NSIS_DISPLAY_NAME "Wasp Compiler")
        set(CPACK_NSIS_PACKAGE_NAME "Wasp ${PROJECT_VERSION}")
        set(CPACK_NSIS_MODIFY_PATH ON)
    elseif(APPLE)
        set(CPACK_GENERATOR "DragNDrop;ZIP;Bundle")
        set(CPACK_DMG_VOLUME_NAME "Wasp ${PROJECT_VERSION}")
        set(CPACK_BUNDLE_NAME "Wasp")
        set(CPACK_BUNDLE_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/wasp.icns")
    else()
        set(CPACK_GENERATOR "TGZ;DEB")

        # Debian package settings
        set(CPACK_DEBIAN_PACKAGE_NAME "wasp")
        set(CPACK_DEBIAN_PACKAGE_VERSION "${PROJECT_VERSION}")
        set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
        set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6, libstdc++6")
        set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "Wasp Programming Language Compiler")
        set(CPACK_DEBIAN_PACKAGE_SECTION "devel")
        set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
        set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Pranav Athrapullikal <pranavap96@gmail.com>")

        # RPM package settings
        set(CPACK_RPM_PACKAGE_NAME "wasp")
        set(CPACK_RPM_PACKAGE_VERSION "${PROJECT_VERSION}")
        set(CPACK_RPM_PACKAGE_RELEASE "1")
        set(CPACK_RPM_PACKAGE_ARCHITECTURE "x86_64")
        set(CPACK_RPM_PACKAGE_DESCRIPTION "Wasp Programming Language Compiler")
    endif()

    # Source package
    set(CPACK_SOURCE_GENERATOR "TGZ;ZIP")
    set(CPACK_SOURCE_IGNORE_FILES
        "/\\\\.git/"
        "/build/"
        "/\\\\.idea/"
        "\\\\.swp$"
        "~$"
    )

    # Include CPack module
    include(CPack)

endif()

