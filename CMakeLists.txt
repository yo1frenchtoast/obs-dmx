cmake_minimum_required(VERSION 3.28)

project(obs-dmx VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Only the Qt side needs moc.
set(CMAKE_AUTOMOC ON)

# The plugin module needs OBS and Qt; the engine and its tests do not. Turning
# this off lets a contributor build and run the tests with nothing but a
# compiler.
option(BUILD_PLUGIN "Build the OBS plugin module" ON)
option(BUILD_TESTING_OBS_DMX "Build the engine tests" OFF)

# ---------------------------------------------------------------------------
# obs-dmx-core: the engine, deliberately free of libobs and Qt so it stays
# testable outside OBS (see tests/).
# ---------------------------------------------------------------------------
# Packaged on most distributions and inside the OBS Flatpak runtime; fetched
# otherwise, so a Windows or macOS build needs no package manager.
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
  include(FetchContent)
  FetchContent_Declare(nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d)
  set(JSON_BuildTests OFF CACHE INTERNAL "")
  FetchContent_MakeAvailable(nlohmann_json)
endif()

add_library(obs-dmx-core STATIC
  src/core/audio-analysis.cpp
  src/core/color.cpp
  src/core/dmx-engine.cpp
  src/core/effect-runner.cpp
  src/core/fixture-library.cpp
  src/core/fixture-profile.cpp
  src/core/patch.cpp
  src/core/show.cpp
  src/output/artnet-output.cpp
  src/output/sacn-output.cpp
  src/output/enttec-output.cpp
  src/output/serial-port.cpp
  src/output/udp-socket.cpp
)
target_include_directories(obs-dmx-core PUBLIC src)
target_link_libraries(obs-dmx-core PUBLIC nlohmann_json::nlohmann_json)
if(WIN32)
  # Winsock is not linked by default, unlike the POSIX socket API.
  target_link_libraries(obs-dmx-core PUBLIC ws2_32)
endif()

if(MSVC)
  target_compile_options(obs-dmx-core PRIVATE /W4)
else()
  target_compile_options(obs-dmx-core PRIVATE -Wall -Wextra)
endif()

# ---------------------------------------------------------------------------
# The OBS module itself.
# ---------------------------------------------------------------------------
if(BUILD_PLUGIN)
  find_package(libobs REQUIRED)
  find_package(obs-frontend-api REQUIRED)
  find_package(Qt6 REQUIRED COMPONENTS Widgets)

  add_library(obs-dmx MODULE
    src/plugin-main.cpp
    src/audio/obs-audio-tap.cpp
    src/obs/persistence.cpp
    src/obs/scene-binding.cpp
    src/ui/add-fixture-dialog.cpp
    src/ui/dmx-dock.cpp
    src/ui/effect-editor.cpp
    src/ui/level-meter.h
    src/ui/output-settings.cpp
    src/ui/patch-page.cpp
    src/ui/programs-page.cpp
    src/ui/slider-row.h
  )

  target_include_directories(obs-dmx PRIVATE src)

  target_link_libraries(obs-dmx PRIVATE
    obs-dmx-core
    OBS::libobs
    OBS::obs-frontend-api
    Qt6::Widgets
  )

  # libobs loads the module with dlopen: no "lib" prefix.
  set_target_properties(obs-dmx PROPERTIES PREFIX "")

  target_compile_definitions(obs-dmx PRIVATE PLUGIN_VERSION="${PROJECT_VERSION}")

  if(MSVC)
    target_compile_options(obs-dmx PRIVATE /W4)
  else()
    target_compile_options(obs-dmx PRIVATE -Wall -Wextra)
  endif()

  # Each platform expects a different layout, and OBS looks in exactly one
  # place per platform. Getting this wrong builds a plugin that loads nowhere.
  include(GNUInstallDirs)
  if(APPLE)
    # A macOS plugin is a bundle; OBS loads the executable inside it.
    set_target_properties(obs-dmx PROPERTIES
      BUNDLE TRUE
      BUNDLE_EXTENSION plugin
      MACOSX_BUNDLE_BUNDLE_NAME obs-dmx
      MACOSX_BUNDLE_GUI_IDENTIFIER com.obsproject.obs-dmx)
    # LIBRARY here too: BUNDLE covers executables, and a MODULE library stays
    # a LIBRARY for installation even once it is bundled.
    install(TARGETS obs-dmx LIBRARY DESTINATION ".")
    install(DIRECTORY data/ DESTINATION "obs-dmx.plugin/Contents/Resources")
  elseif(WIN32)
    # RUNTIME would cover executables and the DLLs of SHARED libraries; a
    # MODULE library is a LIBRARY here as everywhere else.
    install(TARGETS obs-dmx LIBRARY DESTINATION "obs-plugins/64bit")
    install(DIRECTORY data/ DESTINATION "data/obs-plugins/obs-dmx")
  else()
    install(TARGETS obs-dmx LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}/obs-plugins")
    install(DIRECTORY data/ DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/obs/obs-plugins/obs-dmx")
  endif()
endif()

if(BUILD_TESTING_OBS_DMX)
  enable_testing()
  add_subdirectory(tests)
endif()
