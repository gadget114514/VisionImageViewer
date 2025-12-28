# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "D:/ws/VisionPhoto/build/_deps/unarr-src")
  file(MAKE_DIRECTORY "D:/ws/VisionPhoto/build/_deps/unarr-src")
endif()
file(MAKE_DIRECTORY
  "D:/ws/VisionPhoto/build/_deps/unarr-build"
  "D:/ws/VisionPhoto/build/_deps/unarr-subbuild/unarr-populate-prefix"
  "D:/ws/VisionPhoto/build/_deps/unarr-subbuild/unarr-populate-prefix/tmp"
  "D:/ws/VisionPhoto/build/_deps/unarr-subbuild/unarr-populate-prefix/src/unarr-populate-stamp"
  "D:/ws/VisionPhoto/build/_deps/unarr-subbuild/unarr-populate-prefix/src"
  "D:/ws/VisionPhoto/build/_deps/unarr-subbuild/unarr-populate-prefix/src/unarr-populate-stamp"
)

set(configSubDirs Debug)
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "D:/ws/VisionPhoto/build/_deps/unarr-subbuild/unarr-populate-prefix/src/unarr-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "D:/ws/VisionPhoto/build/_deps/unarr-subbuild/unarr-populate-prefix/src/unarr-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
