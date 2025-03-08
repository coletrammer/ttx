if(NOT ttx_LIB_ONLY)
    install(TARGETS ttx_app RUNTIME COMPONENT ttx_Runtime)
endif()

if(NOT ttx_APP_ONLY)
    if(PROJECT_IS_TOP_LEVEL)
        set(CMAKE_INSTALL_INCLUDEDIR
            "include/ttx-${PROJECT_VERSION}"
            CACHE STRING ""
        )
        set_property(CACHE CMAKE_INSTALL_INCLUDEDIR PROPERTY TYPE PATH)
    endif()

    include(CMakePackageConfigHelpers)
    include(GNUInstallDirs)

    # find_package(<package>) call for consumers to find this project
    set(package ttx)

    install(
        TARGETS ttx_ttx
        EXPORT ttxTargets
        RUNTIME COMPONENT ttx_Runtime
        LIBRARY COMPONENT ttx_Runtime NAMELINK_COMPONENT ttx_Development
        ARCHIVE COMPONENT ttx_Development
        INCLUDES
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
        FILE_SET HEADERS
    )

    write_basic_package_version_file("${package}ConfigVersion.cmake" COMPATIBILITY SameMajorVersion ARCH_INDEPENDENT)
    # Allow package maintainers to freely override the path for the configs
    set(ttx_INSTALL_CMAKEDIR
        "${CMAKE_INSTALL_DATADIR}/${package}"
        CACHE STRING "CMake package config location relative to the install prefix"
    )
    set_property(CACHE ttx_INSTALL_CMAKEDIR PROPERTY TYPE PATH)
    mark_as_advanced(ttx_INSTALL_CMAKEDIR)

    install(
        FILES meta/cmake/install-config.cmake
        DESTINATION "${ttx_INSTALL_CMAKEDIR}"
        RENAME "${package}Config.cmake"
        COMPONENT ttx_Development
    )

    install(
        FILES "${PROJECT_BINARY_DIR}/${package}ConfigVersion.cmake"
        DESTINATION "${ttx_INSTALL_CMAKEDIR}"
        COMPONENT ttx_Development
    )

    install(
        EXPORT ttxTargets
        NAMESPACE ttx::
        DESTINATION "${ttx_INSTALL_CMAKEDIR}"
        COMPONENT ttx_Development
    )
endif()

if(PROJECT_IS_TOP_LEVEL)
    include(CPack)
endif()
