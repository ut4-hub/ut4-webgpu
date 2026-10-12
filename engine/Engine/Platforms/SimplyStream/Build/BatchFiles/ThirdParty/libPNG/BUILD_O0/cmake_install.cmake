# Install script for directory: D:/UnrealEngines/5.6/Engine/Source/ThirdParty/libPNG/libPNG-1.6.44

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "D:/UnrealEngines/UE5.5/Engine/Platforms/SimplyStream/emsdk/upstream/emscripten/cache/sysroot")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Debug")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "TRUE")
endif()

# Set default install directory permissions.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "D:/emsdk/upstream/bin/llvm-objdump.exe")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "D:/UnrealEngines/5.6/Engine/Platforms/SimplyStream/Build/BatchFiles/ThirdParty/libPNG/BUILD_O0/libpng16d.a")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE FILE FILES "D:/UnrealEngines/5.6/Engine/Platforms/SimplyStream/Build/BatchFiles/ThirdParty/libPNG/BUILD_O0/libpng.a")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include" TYPE FILE FILES
    "D:/UnrealEngines/5.6/Engine/Source/ThirdParty/libPNG/libPNG-1.6.44/png.h"
    "D:/UnrealEngines/5.6/Engine/Source/ThirdParty/libPNG/libPNG-1.6.44/pngconf.h"
    "D:/UnrealEngines/5.6/Engine/Platforms/SimplyStream/Build/BatchFiles/ThirdParty/libPNG/BUILD_O0/pnglibconf.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/libpng16" TYPE FILE FILES
    "D:/UnrealEngines/5.6/Engine/Source/ThirdParty/libPNG/libPNG-1.6.44/png.h"
    "D:/UnrealEngines/5.6/Engine/Source/ThirdParty/libPNG/libPNG-1.6.44/pngconf.h"
    "D:/UnrealEngines/5.6/Engine/Platforms/SimplyStream/Build/BatchFiles/ThirdParty/libPNG/BUILD_O0/pnglibconf.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE PROGRAM FILES "D:/UnrealEngines/5.6/Engine/Platforms/SimplyStream/Build/BatchFiles/ThirdParty/libPNG/BUILD_O0/libpng-config")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE PROGRAM FILES "D:/UnrealEngines/5.6/Engine/Platforms/SimplyStream/Build/BatchFiles/ThirdParty/libPNG/BUILD_O0/libpng16-config")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/man/man3" TYPE FILE FILES
    "D:/UnrealEngines/5.6/Engine/Source/ThirdParty/libPNG/libPNG-1.6.44/libpng.3"
    "D:/UnrealEngines/5.6/Engine/Source/ThirdParty/libPNG/libPNG-1.6.44/libpngpf.3"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/man/man5" TYPE FILE FILES "D:/UnrealEngines/5.6/Engine/Source/ThirdParty/libPNG/libPNG-1.6.44/png.5")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libpng/libpng16.cmake")
    file(DIFFERENT _cmake_export_file_changed FILES
         "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libpng/libpng16.cmake"
         "D:/UnrealEngines/5.6/Engine/Platforms/SimplyStream/Build/BatchFiles/ThirdParty/libPNG/BUILD_O0/CMakeFiles/Export/27e805ec6cde31251f2809ac4798f31f/libpng16.cmake")
    if(_cmake_export_file_changed)
      file(GLOB _cmake_old_config_files "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libpng/libpng16-*.cmake")
      if(_cmake_old_config_files)
        string(REPLACE ";" ", " _cmake_old_config_files_text "${_cmake_old_config_files}")
        message(STATUS "Old export file \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libpng/libpng16.cmake\" will be replaced.  Removing files [${_cmake_old_config_files_text}].")
        unset(_cmake_old_config_files_text)
        file(REMOVE ${_cmake_old_config_files})
      endif()
      unset(_cmake_old_config_files)
    endif()
    unset(_cmake_export_file_changed)
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/libpng" TYPE FILE FILES "D:/UnrealEngines/5.6/Engine/Platforms/SimplyStream/Build/BatchFiles/ThirdParty/libPNG/BUILD_O0/CMakeFiles/Export/27e805ec6cde31251f2809ac4798f31f/libpng16.cmake")
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/libpng" TYPE FILE FILES "D:/UnrealEngines/5.6/Engine/Platforms/SimplyStream/Build/BatchFiles/ThirdParty/libPNG/BUILD_O0/CMakeFiles/Export/27e805ec6cde31251f2809ac4798f31f/libpng16-debug.cmake")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "D:/UnrealEngines/5.6/Engine/Platforms/SimplyStream/Build/BatchFiles/ThirdParty/libPNG/BUILD_O0/libpng16d.a")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/PNG/PNGTargets.cmake")
    file(DIFFERENT _cmake_export_file_changed FILES
         "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/PNG/PNGTargets.cmake"
         "D:/UnrealEngines/5.6/Engine/Platforms/SimplyStream/Build/BatchFiles/ThirdParty/libPNG/BUILD_O0/CMakeFiles/Export/37fe81d72e9d17407a692c28a4a9718c/PNGTargets.cmake")
    if(_cmake_export_file_changed)
      file(GLOB _cmake_old_config_files "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/PNG/PNGTargets-*.cmake")
      if(_cmake_old_config_files)
        string(REPLACE ";" ", " _cmake_old_config_files_text "${_cmake_old_config_files}")
        message(STATUS "Old export file \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/PNG/PNGTargets.cmake\" will be replaced.  Removing files [${_cmake_old_config_files_text}].")
        unset(_cmake_old_config_files_text)
        file(REMOVE ${_cmake_old_config_files})
      endif()
      unset(_cmake_old_config_files)
    endif()
    unset(_cmake_export_file_changed)
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/PNG" TYPE FILE FILES "D:/UnrealEngines/5.6/Engine/Platforms/SimplyStream/Build/BatchFiles/ThirdParty/libPNG/BUILD_O0/CMakeFiles/Export/37fe81d72e9d17407a692c28a4a9718c/PNGTargets.cmake")
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/PNG" TYPE FILE FILES "D:/UnrealEngines/5.6/Engine/Platforms/SimplyStream/Build/BatchFiles/ThirdParty/libPNG/BUILD_O0/CMakeFiles/Export/37fe81d72e9d17407a692c28a4a9718c/PNGTargets-debug.cmake")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/PNG" TYPE FILE FILES
    "D:/UnrealEngines/5.6/Engine/Source/ThirdParty/libPNG/libPNG-1.6.44/scripts/cmake/PNGConfig.cmake"
    "D:/UnrealEngines/5.6/Engine/Platforms/SimplyStream/Build/BatchFiles/ThirdParty/libPNG/BUILD_O0/PNGConfigVersion.cmake"
    )
endif()

if(CMAKE_INSTALL_COMPONENT)
  set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INSTALL_COMPONENT}.txt")
else()
  set(CMAKE_INSTALL_MANIFEST "install_manifest.txt")
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
file(WRITE "D:/UnrealEngines/5.6/Engine/Platforms/SimplyStream/Build/BatchFiles/ThirdParty/libPNG/BUILD_O0/${CMAKE_INSTALL_MANIFEST}"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
