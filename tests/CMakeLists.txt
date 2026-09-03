add_executable(obs-dmx-tests
  test-main.cpp
  test-artnet.cpp
  test-sacn.cpp
  test-enttec.cpp
  test-engine.cpp
)
target_link_libraries(obs-dmx-tests PRIVATE obs-dmx-core)
target_compile_options(obs-dmx-tests PRIVATE -Wall -Wextra)
add_test(NAME obs-dmx-tests COMMAND obs-dmx-tests)
