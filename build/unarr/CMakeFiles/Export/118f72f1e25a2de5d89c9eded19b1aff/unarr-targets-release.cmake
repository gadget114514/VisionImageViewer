#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "unarr::unarr" for configuration "Release"
set_property(TARGET unarr::unarr APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(unarr::unarr PROPERTIES
  IMPORTED_IMPLIB_RELEASE "${_IMPORT_PREFIX}/lib/unarr.lib"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/bin/unarr.dll"
  )

list(APPEND _cmake_import_check_targets unarr::unarr )
list(APPEND _cmake_import_check_files_for_unarr::unarr "${_IMPORT_PREFIX}/lib/unarr.lib" "${_IMPORT_PREFIX}/bin/unarr.dll" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
