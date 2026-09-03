cmake_minimum_required(VERSION 3.28)

project(obs-dmx VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_AUTOMOC ON)

find_package(libobs REQUIRED)
find_package(obs-frontend-api REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Widgets)

# ---------------------------------------------------------------------------
# obs-dmx-core : le moteur, volontairement sans aucune dependance a libobs ni
# a Qt, pour rester testable hors d'OBS (voir tests/).
# ---------------------------------------------------------------------------
find_package(nlohmann_json REQUIRED)

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
)
target_include_directories(obs-dmx-core PUBLIC src)
target_link_libraries(obs-dmx-core PUBLIC nlohmann_json::nlohmann_json)

# ---------------------------------------------------------------------------
# Le module OBS proprement dit.
# ---------------------------------------------------------------------------
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

# libobs charge le module par dlopen : pas de prefixe "lib".
set_target_properties(obs-dmx PROPERTIES PREFIX "")

target_compile_definitions(obs-dmx PRIVATE PLUGIN_VERSION="${PROJECT_VERSION}")

target_compile_options(obs-dmx PRIVATE -Wall -Wextra)
target_compile_options(obs-dmx-core PRIVATE -Wall -Wextra)

include(GNUInstallDirs)
install(TARGETS obs-dmx LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}/obs-plugins")
install(DIRECTORY data/ DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/obs/obs-plugins/obs-dmx")

option(BUILD_TESTING_OBS_DMX "Construire les tests du moteur" OFF)
if(BUILD_TESTING_OBS_DMX)
  enable_testing()
  add_subdirectory(tests)
endif()
