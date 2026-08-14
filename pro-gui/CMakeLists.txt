cmake_minimum_required(VERSION 3.16)
project(dogecoin-pro-gui LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
# WSL often lacks wayland-scanner; X11 is enough for the spike
set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(GLFW_BUILD_X11 OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_WIN32 ON CACHE BOOL "" FORCE)
else()
  set(GLFW_BUILD_X11 ON CACHE BOOL "" FORCE)
endif()

# Prefer system glfw if present; otherwise fetch
find_package(glfw3 QUIET)
find_package(PkgConfig QUIET)
if(NOT TARGET glfw AND PkgConfig_FOUND)
  pkg_check_modules(GLFW3 QUIET glfw3)
endif()

if(NOT TARGET glfw AND NOT GLFW3_FOUND)
  message(STATUS "Fetching GLFW…")
  FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(glfw)
endif()

find_package(OpenGL REQUIRED)

set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui)
if(NOT EXISTS ${IMGUI_DIR}/imgui.h)
  message(FATAL_ERROR "ImGui missing at ${IMGUI_DIR} (docking branch)")
endif()

add_library(imgui STATIC
  ${IMGUI_DIR}/imgui.cpp
  ${IMGUI_DIR}/imgui_demo.cpp
  ${IMGUI_DIR}/imgui_draw.cpp
  ${IMGUI_DIR}/imgui_tables.cpp
  ${IMGUI_DIR}/imgui_widgets.cpp
  ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
  ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
  ${IMGUI_DIR}
  ${IMGUI_DIR}/backends
)
target_compile_definitions(imgui PUBLIC IMGUI_DEFINE_MATH_OPERATORS)
if(TARGET glfw)
  target_link_libraries(imgui PUBLIC glfw OpenGL::GL)
elseif(GLFW3_FOUND)
  target_include_directories(imgui PUBLIC ${GLFW3_INCLUDE_DIRS})
  target_link_libraries(imgui PUBLIC ${GLFW3_LIBRARIES} OpenGL::GL)
else()
  message(FATAL_ERROR "No GLFW target")
endif()

set(PRO_GUI_SOURCES
  src/main.cpp
  src/app.cpp
  src/texture.cpp
  src/theme.cpp
  src/rpc_client.cpp
  src/rpc_probe.cpp
  src/settings_io.cpp
  src/node_launch.cpp
  src/arcade_host.cpp
  src/arcade_webview.cpp
  src/qr_tiny.cpp
  src/memestream_http.cpp
  src/tor_local.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../src/qt/memestreampublishkey.cpp
)
if(WIN32)
  list(APPEND PRO_GUI_SOURCES src/win_tray.cpp)
else()
  list(APPEND PRO_GUI_SOURCES src/linux_tray.cpp)
endif()

# Embed original Dogecoin .ico into the Windows PE (taskbar / Explorer icon)
if(WIN32)
  enable_language(RC)
  # windres resolves ICON path relative to the .rc file location (src/)
  # so point at ../assets/brand/dogecoin.ico from src/win_icon.rc
  list(APPEND PRO_GUI_SOURCES src/win_icon.rc)
endif()

if(WIN32)
  add_executable(dogecoin-pro-gui WIN32 ${PRO_GUI_SOURCES})
else()
  add_executable(dogecoin-pro-gui ${PRO_GUI_SOURCES})
endif()
target_include_directories(dogecoin-pro-gui PRIVATE
  src
  ${CMAKE_CURRENT_SOURCE_DIR}/../src
  ${IMGUI_DIR}
  ${IMGUI_DIR}/backends
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/webview2/include
)
target_link_libraries(dogecoin-pro-gui PRIVATE imgui)

add_custom_command(TARGET dogecoin-pro-gui POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/assets
    $<TARGET_FILE_DIR:dogecoin-pro-gui>/assets
)

if(UNIX AND NOT APPLE)
  target_link_libraries(dogecoin-pro-gui PRIVATE dl pthread)
endif()

if(WIN32)
  target_link_libraries(dogecoin-pro-gui PRIVATE
    opengl32 ws2_32 gdi32 user32 shell32 comdlg32 imm32 ole32 oleaut32 uuid version shlwapi
    winhttp wininet dwmapi advapi32
  )
  # Portable smoke .exe: no libgcc/libstdc++ DLLs required on a plain Windows box
  if(MINGW)
    target_link_options(dogecoin-pro-gui PRIVATE
      -static-libgcc
      -static-libstdc++
      -static
      -mwindows
    )
  endif()
  # WebView2 loader next to the binary (runtime LoadLibrary)
  add_custom_command(TARGET dogecoin-pro-gui POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      ${CMAKE_CURRENT_SOURCE_DIR}/third_party/webview2/x64/WebView2Loader.dll
      $<TARGET_FILE_DIR:dogecoin-pro-gui>/WebView2Loader.dll
  )
endif()

# ---------------------------------------------------------------------------
# corepro-launch — WIN32 (no console). Hybrid picker + start GUI/TUI.
# ---------------------------------------------------------------------------
if(WIN32)
  add_executable(corepro-launch WIN32 src/corepro_launch.cpp)
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/win_icon.rc)
    target_sources(corepro-launch PRIVATE src/win_icon.rc)
  endif()
  target_link_libraries(corepro-launch PRIVATE user32 gdi32 shell32 comctl32 advapi32)
  if(MINGW)
    target_link_options(corepro-launch PRIVATE -static-libgcc -static-libstdc++ -static -mwindows)
  endif()
endif()

# ---------------------------------------------------------------------------
# dogeinit — start dogecoind, wait for RPC (real AppInit), then launch GUI
# ---------------------------------------------------------------------------
add_executable(dogeinit
  src/dogeinit_main.cpp
  src/node_launch.cpp
  src/rpc_client.cpp
)
target_include_directories(dogeinit PRIVATE src)
if(WIN32)
  # win_icon for dogeinit too
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/win_icon.rc)
    target_sources(dogeinit PRIVATE src/win_icon.rc)
  endif()
  target_link_libraries(dogeinit PRIVATE ws2_32 shell32)
  if(MINGW)
    target_link_options(dogeinit PRIVATE -static-libgcc -static-libstdc++ -static)
  endif()
endif()
if(UNIX AND NOT APPLE)
  target_link_libraries(dogeinit PRIVATE pthread)
endif()
