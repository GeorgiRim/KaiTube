# Replace absolute Switch 1 system/GPU paths exported by portlib packages.
# Ordinary Switch portlibs (SDL, curl, codecs, etc.) remain usable with libounce.
function(kaitube_fix_switch2_links target)
    if(NOT TARGET "${target}")
        return()
    endif()
    get_target_property(alias "${target}" ALIASED_TARGET)
    if(alias)
        return()
    endif()
    foreach(prop IN ITEMS LINK_LIBRARIES INTERFACE_LINK_LIBRARIES
            LINK_DIRECTORIES INTERFACE_LINK_DIRECTORIES
            LINK_OPTIONS INTERFACE_LINK_OPTIONS INCLUDE_DIRECTORIES
            INTERFACE_INCLUDE_DIRECTORIES IMPORTED_LOCATION
            IMPORTED_LOCATION_RELEASE IMPORTED_LOCATION_DEBUG
            IMPORTED_LINK_INTERFACE_LIBRARIES
            IMPORTED_LINK_INTERFACE_LIBRARIES_RELEASE)
        get_target_property(value "${target}" "${prop}")
        if(NOT value OR value MATCHES "-NOTFOUND$")
            continue()
        endif()
        set(original "${value}")
        string(REPLACE "${DEVKITPRO}/libnx/" "${DEVKITPRO}/libounce/" value "${value}")
        foreach(lib IN ITEMS EGL GLESv2 glapi drm_nouveau vulkan)
            string(REPLACE "${DEVKITPRO}/portlibs/switch/lib/lib${lib}.a"
                "${DEVKITPRO}/portlibs/switch2/lib/lib${lib}.a" value "${value}")
        endforeach()
        if(NOT value STREQUAL original)
            set_property(TARGET "${target}" PROPERTY "${prop}" "${value}")
            message(STATUS "Switch 2 link paths corrected: ${target} (${prop})")
        endif()
    endforeach()
endfunction()

function(kaitube_fix_directory_links)
    get_property(imports DIRECTORY PROPERTY IMPORTED_TARGETS)
    get_property(targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
    foreach(target IN LISTS imports targets)
        kaitube_fix_switch2_links("${target}")
    endforeach()
endfunction()
