
function(gp_append_unique list_var value)
    if(NOT value IN_LIST ${list_var})
    set(${list_var} ${${list_var}} "${value}" PARENT_SCOPE)
    endif()
endfunction()

function(list_append_unique list_var)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs VALUE)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    foreach(value IN ITEMS ${ARG_VALUE})
        if(NOT value IN_LIST ${list_var})
            set(${list_var} ${${list_var}} ${value} PARENT_SCOPE)
        endif()
    endforeach()
endfunction()

# Gather dylibs referenced in the LIBRARIES. This function extends 
# the LIBRARIES list with version symlinks sometimes found with .dylibs
function(append_symlink_dylibs LIBRARIES)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs LIBRARIES)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    set(libraries ${ARG_LIBRARIES})

    # Extend / Modify Libraries
    foreach(library IN ITEMS ${libraries})
        if (NOT ${library} MATCHES "\.dylib$")
            continue()
        endif()
        
        get_filename_component(libdir ${library} DIRECTORY)
        get_filename_component(libname ${library} NAME_WE)
        file(GLOB symlink_candidates "${libdir}/${libname}.dylib" "${libdir}/${libname}.*.dylib")
        foreach(candidate IN ITEMS ${symlink_candidates})
            if(${candidate} STREQUAL ${library})
                continue()
            endif()
            gp_append_unique(libraries ${candidate})
        endforeach()

        # TODO: also add local libraries that are loaded by this library
        #get_item_rpaths(${library} RPATH)
    endforeach()

    set(${LIBRARIES} ${libraries} PARENT_SCOPE)
endfunction()

# recursively gather target dependencies that should be contained in the application
# bundle. 
# You can ignore specific libraries / targets by listing them in the IGNORE argument.
function(gather_target_dependencies DEPENDENCIES)
    set(options)
    set(oneValueArgs TARGET)
    set(multiValueArgs IGNORE)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if (ARG_TARGET IN_LIST ARG_IGNORE)
        return()
    endif()

    set(deps ${${DEPENDENCIES}})

    get_target_property(linked_libraries ${ARG_TARGET} LINK_LIBRARIES)
    foreach (linked_library IN ITEMS ${linked_libraries})
        # Skip libraries that we have already visited
        if (linked_library IN_LIST ARG_IGNORE)
            continue()
        endif()

        # If LINK_LIBRARIES is not defined on the target
        # the list contains only the string "linked_libraries-NOTFOUND".
        # This is not a target so it has to be skipped
        if (${linked_library} STREQUAL "linked_libraries-NOTFOUND")
            continue()
        endif()

        if(TARGET ${linked_library})
            get_target_property(target_type ${linked_library} TYPE)
            if("${target_type}" STREQUAL "STATIC_LIBRARY")
                gather_target_dependencies(deps TARGET ${linked_library} IGNORE ${ARG_TARGET} ${ARG_IGNORE})
                continue()
            endif()
        endif()

        # Ignore manually linked frameworks. In modern cmake 
        # these are only system frameworks that don't need to 
        # be bundled
        if (${linked_library} MATCHES "^-framework.+")
            continue()
        endif()

        # Ignore manually linked dylibs as they are assumed to 
        # be system libraries.
        if (${linked_library} MATCHES "^-l.+")
            continue()
        endif()

        # Ignore absolute paths. Gather Frameworks assumes
        # them to be system libraries. If you have linked to libraries
        # with absolute paths, consider moving them to a relative
        # location, or add them to the embedded frameworks manually.
        if (${linked_library} MATCHES "^/")
            list_append_unique(deps VALUE ${linked_library})
            continue()
        endif()

        if (NOT TARGET ${linked_library})
            continue()
        endif()

        # Skip interface targets as they never produce a binary
        # that would need to be embedded
        get_target_property(type ${linked_library} TYPE)
        if (${type} STREQUAL "INTERFACE_LIBRARY")
            continue()
        endif()

        get_target_property(is_imported ${linked_library} IMPORTED)
        if (${is_imported})
            # For imported targets we need to give the path
            # to the library
            get_target_property(location ${linked_library} LOCATION)
            list_append_unique(deps VALUE ${location})
        else()
            # For targets built by Xcode we can just pass the 
            # target name
            list_append_unique(deps VALUE ${linked_library})
        endif()
    endforeach()

    set(${DEPENDENCIES} ${deps} PARENT_SCOPE)
endfunction()

# Gather frameworks for macOS bundles
function(gather_frameworks FRAMEWORKS)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs TARGETS LIBRARIES IGNORED_ITEMS)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    
    set(targets ${ARG_TARGETS})
    set(libraries ${ARG_LIBRARIES})

    # Extract all libraries from the targets
    foreach(target IN ITEMS ${targets})
        gather_target_dependencies(dependencies TARGET ${target})
        list(APPEND dylibs ${dependencies})
    endforeach()

    append_symlink_dylibs(dylibs LIBRARIES ${dylibs})

    # Ensure that the output path points to a .framework if possible
    foreach(candidate IN ITEMS ${dylibs})
        if (${candidate} MATCHES "\.a$")
            continue()
        endif()

        if (NOT ${candidate} MATCHES ".+\.(dylib|framework)$" AND NOT TARGET ${candidate})
            get_filename_component(framework_directory ${candidate} DIRECTORY)
            # Search for the directory ending in ".framework"
            while(TRUE)
                if("${framework_directory}" STREQUAL "")
                    message(FATAL_ERROR "Dependency \"${candidate}\" is neither a framework nor a dylib")
                    break()
                endif()

                if(${framework_directory} MATCHES ".+\.framework$")
                    set(candidate ${framework_directory})
                    break()
                endif()

                # Not yet at the .framework
                # Proceed to parent directory
                get_filename_component(framework_directory ${framework_directory} DIRECTORY)
            endwhile()
        endif()

        list(APPEND libraries ${candidate})
    endforeach()

    set(${FRAMEWORKS} ${libraries} PARENT_SCOPE)
endfunction()

function(make_framework)
    if (APPLE)
        set(options)
        set(oneValueArgs TARGET VERSION IDENTIFIER)
        set(multiValueArgs PUBLIC_HEADER)
        cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

        # Shared libraries should be frameworks on macOS
        get_target_property(type ${ARG_TARGET} TYPE)
        if (${type} STREQUAL "SHARED_LIBRARY")
            gather_frameworks(used_frameworks TARGETS ${ARG_TARGET})

            set_target_properties(${ARG_TARGET} PROPERTIES
                FRAMEWORK TRUE
                FRAMEWORK_VERSION C
                MACOSX_FRAMEWORK_IDENTIFIER ${ARG_IDENTIFIER}
                # "current version" in semantic format in Mach-O binary file
                VERSION ${ARG_VERSION}
                # "compatibility version" in semantic format in Mach-O binary file
                SOVERSION ${ARG_VERSION}
            )
        endif()

        set_target_properties(${ARG_TARGET} PROPERTIES
            XCODE_ATTRIBUTE_SKIP_INSTALL YES # Frameworks should generally skip the install
            PUBLIC_HEADER "${ARG_PUBLIC_HEADER}"
            # XCODE_LINK_BUILD_PHASE_MODE KNOWN_LOCATION
            XCODE_EMBED_FRAMEWORKS "${used_frameworks}"
            XCODE_EMBED_FRAMEWORKS_CODE_SIGN_ON_COPY      TRUE
            XCODE_EMBED_FRAMEWORKS_REMOVE_HEADERS_ON_COPY TRUE
        )
    endif()
endfunction()

function(xcode_dependencies_skip_install)
    if (APPLE)
        set(options)
        set(oneValueArgs TARGET)
        set(multiValueArgs)
        cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

        get_target_property(linked_libraries ${ARG_TARGET} LINK_LIBRARIES)
        foreach (linked_library IN ITEMS ${linked_libraries})
            if (TARGET ${linked_library})
                set_target_properties(${linked_library} PROPERTIES XCODE_ATTRIBUTE_SKIP_INSTALL YES)
            endif()
        endforeach()
    endif()
endfunction()

# Add all files in a subdirectory to a Xcode "Copy Files" build step.
# The output "FILES" of this function takes the form of a list of paths
# that need to be added to the sources of the target for which the "Copy Files"
# build step should be executed.
#   gather_xcode_copy_files(xcode_copy_plugins PLUGIN_DIRECTORIES "${qt_dir}/plugins/platform")
#   add_executable(MyTarget ... ${xcode_copy_plugins})
# You must pair this function call with a call to "configure_xcode_copy_files" 
# in order to keep the directory structure and to enable "Code Sign On Copy"
function(gather_xcode_copy_files FILES)
    if (APPLE)
        set(options)
        set(oneValueArgs)
        set(multiValueArgs PLUGIN_DIRECTORIES)
        cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

        if(NOT ARG_DESTINATION_DIRECTORY)
            set(ARG_DESTINATION_DIRECTORY "PlugIns")
        endif()

        foreach(plugin_directory IN ITEMS ${ARG_PLUGIN_DIRECTORIES})
            file(GLOB_RECURSE all_files_in_dir "${plugin_directory}/*")
            list(APPEND files "${all_files_in_dir}")
        endforeach()
    endif()
    set(${FILES} ${files} PARENT_SCOPE)
endfunction()

# Configure files added to a target with gather_xcode_copy_files to 
# keep the directory structure and enabling "Code Sign on Copy".
#       configure_xcode_copy_files(
#           BUNDLE_DESTINATION_DIRECTORY "PlugIns"
#           PLUGIN_DIRECTORIES "${qt_dir}/plugins/platform"
#       )
# Will configure the files (i.e. .dylib files) to be placed in
# a "platform" subdirectory in the PlugIns directory of the bundle.
#       Gestaltor.app/Contents/PlugIns/platform/libqcocoa.dylib
# This function must be called after the "add_executable" or "add_library"
# call that adds the files to the target.
function(configure_xcode_copy_files)
    if (APPLE)
        set(options)
        set(oneValueArgs CODESIGN_ON_COPY BUNDLE_DESTINATION_DIRECTORY)
        set(multiValueArgs PLUGIN_DIRECTORIES PLUGIN_FILES)
        cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

        if(NOT DEFINED ARG_CODESIGN_ON_COPY)
            set(ARG_CODESIGN_ON_COPY ON)
        endif()

        foreach(plugin_directory IN ITEMS ${ARG_PLUGIN_DIRECTORIES})
            file(GLOB_RECURSE plugin_files LIST_DIRECTORIES OFF "${plugin_directory}/*")
            set(relative_directory ${plugin_directory})
            
            # Copy the whole directory to the destination if the path doesn't end with a "/" just like
            # the unix "cp" command
            if (NOT ${plugin_directory} MATCHES ".*/$")
                get_filename_component(relative_directory ${plugin_directory} DIRECTORY)
            endif()
            
            file(GLOB_RECURSE plugin_files_relative LIST_DIRECTORIES OFF RELATIVE "${relative_directory}" "${plugin_directory}/*")
            foreach(plugin_file plugin_file_relative IN ZIP_LISTS plugin_files plugin_files_relative)
                get_filename_component(plugin_file_relative_directory ${plugin_file_relative} DIRECTORY)
                set_source_files_properties(${plugin_file} PROPERTIES
                    MACOSX_PACKAGE_LOCATION "${ARG_BUNDLE_DESTINATION_DIRECTORY}/${plugin_file_relative_directory}"
                )
                if (${ARG_CODESIGN_ON_COPY})
                    get_filename_component(extension ${plugin_file} LAST_EXT)
                    set_source_files_properties(${plugin_file} PROPERTIES
                        XCODE_FILE_ATTRIBUTES "CodeSignOnCopy"
                    )
                endif()
            endforeach()
        endforeach()

        foreach(plugin_file IN ITEMS ${ARG_PLUGIN_FILES})
            set_source_files_properties(${plugin_file} PROPERTIES
                MACOSX_PACKAGE_LOCATION "${ARG_BUNDLE_DESTINATION_DIRECTORY}"
            )
            if (${ARG_CODESIGN_ON_COPY})
                set_source_files_properties(${plugin_file} PROPERTIES
                    XCODE_FILE_ATTRIBUTES "CodeSignOnCopy"
                )
            endif()
        endforeach()
    endif()
endfunction()

function(xcode_codesign_file)
    if(APPLE)
        set(options HARDENED_RUNTIME)
        set(oneValueArgs TARGET ENTITLEMENTS APPNAME)
        set(multiValueArgs CODESIGN_FILES)
        cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

        foreach(file IN ITEMS ${ARG_CODESIGN_FILES})
            if (DEFINED ARG_ENTITLEMENTS)
                set(entitlements --entitlements "${ARG_ENTITLEMENTS}")
            endif()

            if (DEFINED ARG_HARDENED_RUNTIME)
                set(options --options runtime)
            endif()

            set(APP_BUNDLE_DIR "${CMAKE_CURRENT_BINARY_DIR}/bin/\${CONFIGURATION}/${ARG_APPNAME}.app/Contents")

            message("ARG_APPNAME ${ARG_APPNAME}")

            add_custom_command(
                TARGET ${ARG_TARGET} 
                POST_BUILD 
                COMMAND codesign ${entitlements} --force ${options} --sign ${UX3D_CODESIGN_IDENTITY} ${APP_BUNDLE_DIR}/${file}
            )
        endforeach()
    endif()
endfunction()

function(xcode_patch_rpath)
    if(APPLE)
        set(options)
        set(oneValueArgs TARGET APPNAME FROM_RPATH TO_RPATH ADD_RPATH FILE)
        set(multiValueArgs)
        cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

        set(APP_BUNDLE_DIR "${CMAKE_CURRENT_BINARY_DIR}/bin/\${CONFIGURATION}/${ARG_APPNAME}.app/Contents")
        if (DEFINED ARG_FROM_RPATH AND DEFINED ARG_TO_RPATH)
            set(from_to_rpath_args -rpath ${ARG_FROM_RPATH} ${ARG_TO_RPATH})
        elseif(DEFINED ARG_ADD_RPATH)
            set(add_rpath_args -add_rpath ${ARG_ADD_RPATH})
        else()
            message(FATAL_ERROR "Must use either FROM_RPATH and TO_RPATH or ADD_RPATH in xcode_patch_rpath")
        endif()

        # make sure the install_name_tool always operates on a fresh binary (with broken rpaths)
        add_custom_command(
            TARGET ${ARG_TARGET}
            PRE_BUILD
            COMMAND rm -f ${APP_BUNDLE_DIR}/${ARG_FILE}
        )

        # remove any existing signature, as install_name_tool will otherwise emit a warning
        add_custom_command(
            TARGET ${ARG_TARGET}
            POST_BUILD
            COMMAND codesign --remove-signature ${APP_BUNDLE_DIR}/${ARG_FILE}
        )
        
        # add an rpath to the binary
        add_custom_command(
            TARGET ${ARG_TARGET}
            POST_BUILD
            COMMAND install_name_tool ${from_to_rpath_args} ${add_rpath_args} ${APP_BUNDLE_DIR}/${ARG_FILE}
        )
    endif()
endfunction()
