add_executable(obs-dmx-tests
  test-main.cpp
  test-artnet.cpp
  test-sacn.cpp
  test-enttec.cpp
  test-color.cpp
  test-patch.cpp
  test-library-files.cpp
  test-show.cpp
  test-effects.cpp
  test-audio.cpp
  test-engine.cpp
)
target_link_libraries(obs-dmx-tests PRIVATE obs-dmx-core)
# The tests read the shipped profiles straight from the source tree.
target_compile_definitions(obs-dmx-tests PRIVATE
  OBS_DMX_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../data/fixtures")
if(MSVC)
  target_compile_options(obs-dmx-tests PRIVATE /W4)
else()
  target_compile_options(obs-dmx-tests PRIVATE -Wall -Wextra)
endif()
add_test(NAME obs-dmx-tests COMMAND obs-dmx-tests)
