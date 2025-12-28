#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "unarr::unarr" for configuration "Debug"
set_property(TARGET unarr::unarr APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(unarr::unarr PROPERTIES
  IMPORTED_IMPLIB_DEBUG "${_IMPORT_PREFIX}/lib/unarr.lib"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/bin/unarr.dll"
  )

list(APPEND _cmake_import_check_targets unarr::unarr )
list(APPEND _cmake_import_check_files_for_unarr::unarr "${_IMPORT_PREFIX}/lib/unarr.lib" "${_IMPORT_PREFIX}/bin/unarr.dll" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
