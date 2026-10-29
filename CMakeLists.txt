cmake_minimum_required(VERSION 3.20)
cmake_policy(SET CMP0091 NEW)
project(Osteon CXX RC)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>") # static CRT: Osteon.exe runs without the VC++ redistributable
if(MSVC)
  add_compile_options(/W3 /utf-8 /permissive- /EHsc)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX UNICODE _UNICODE)
endif()

file(GLOB ENGINE_SRC CONFIGURE_DEPENDS src/engine/*.cpp)
add_library(osteon_engine STATIC ${ENGINE_SRC})
target_include_directories(osteon_engine PUBLIC src/engine third_party/nlohmann)

file(GLOB TEST_SRC CONFIGURE_DEPENDS tests/*.cpp)
add_executable(osteon_tests ${TEST_SRC})
target_link_libraries(osteon_tests PRIVATE osteon_engine)
target_include_directories(osteon_tests PRIVATE tests)

set(IMGUI third_party/imgui)
add_library(imgui STATIC ${IMGUI}/imgui.cpp ${IMGUI}/imgui_draw.cpp ${IMGUI}/imgui_tables.cpp
  ${IMGUI}/imgui_widgets.cpp ${IMGUI}/backends/imgui_impl_win32.cpp ${IMGUI}/backends/imgui_impl_dx11.cpp)
target_include_directories(imgui PUBLIC ${IMGUI} ${IMGUI}/backends)

file(GLOB EDITOR_SRC CONFIGURE_DEPENDS src/editor/*.cpp)
if(EDITOR_SRC)
  add_executable(Osteon WIN32 ${EDITOR_SRC} src/editor/app.rc)
  target_link_libraries(Osteon PRIVATE osteon_engine imgui d3d11 d3dcompiler dxgi windowscodecs ole32 shell32 comdlg32 dwmapi)
  target_include_directories(Osteon PRIVATE src/editor)
  if(MSVC)
    target_link_options(Osteon PRIVATE /MANIFEST:NO)
  endif()
  set_target_properties(Osteon PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

  # Editor-layer tests (document model, undo, keying) without the UI.
  file(GLOB EDITOR_TEST_SRC CONFIGURE_DEPENDS tests/editor/*.cpp)
  add_executable(osteon_editor_tests ${EDITOR_TEST_SRC} tests/main.cpp src/editor/Document.cpp src/editor/Util.cpp src/editor/Gfx.cpp)
  target_link_libraries(osteon_editor_tests PRIVATE osteon_engine d3d11 d3dcompiler windowscodecs ole32 shell32 comdlg32)
  target_include_directories(osteon_editor_tests PRIVATE src/editor tests third_party/imgui)
endif()
