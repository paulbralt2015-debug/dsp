cmake_minimum_required(VERSION 3.20)
project(TwinsPlayer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Décodeur AC-3 : portable, testable en ligne de commande sur tout système
add_executable(ac3test src/ac3test.cpp src/ac3.cpp)
add_executable(surroundtest src/surroundtest.cpp src/surround.cpp)
add_executable(enginetest src/enginetest.cpp src/engine.cpp src/ac3file.cpp src/ac3.cpp src/surround.cpp)

if(WIN32)
  if(MSVC)
    # CRT statique : l'exécutable tourne sans installer le « Visual C++ Redistributable »
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options(/W3 /utf-8)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX)
  endif()
  add_executable(TwinsPlayer WIN32 src/main.cpp src/player.cpp src/ac3.cpp src/surround.cpp)
  target_compile_definitions(TwinsPlayer PRIVATE UNICODE _UNICODE)
  target_link_libraries(TwinsPlayer PRIVATE
    mfplat mfreadwrite mf mfuuid ole32 uuid user32 gdi32 comctl32 comdlg32 shell32 propsys winmm)

  # ---- Winamp : plug-in d'entrée (in_*.dll) et plug-in DSP (dsp_*.dll) ----
  add_library(in_twins_dolby SHARED src/plugins/in_twins_dolby.cpp src/ac3file.cpp src/ac3.cpp src/engine.cpp src/surround.cpp)
  set_target_properties(in_twins_dolby PROPERTIES PREFIX "")
  target_compile_definitions(in_twins_dolby PRIVATE NOMINMAX)
  target_link_libraries(in_twins_dolby PRIVATE user32)
  add_library(dsp_twins_dolby SHARED src/plugins/dsp_twins_dolby.cpp src/ac3.cpp src/engine.cpp src/surround.cpp)
  set_target_properties(dsp_twins_dolby PROPERTIES PREFIX "")
  target_compile_definitions(dsp_twins_dolby PRIVATE NOMINMAX)
  target_link_libraries(dsp_twins_dolby PRIVATE user32)

  # ---- foobar2000 : nécessite le SDK (cmake -DFB2K_SDK_DIR=chemin/du/SDK) ----
  if(FB2K_SDK_DIR AND EXISTS "${FB2K_SDK_DIR}/foobar2000/SDK")
    file(GLOB PFC_SRC "${FB2K_SDK_DIR}/pfc/*.cpp")
    file(GLOB SDK_SRC "${FB2K_SDK_DIR}/foobar2000/SDK/*.cpp")
    file(GLOB CLIENT_SRC "${FB2K_SDK_DIR}/foobar2000/foobar2000_component_client/*.cpp")
    add_library(foo_twins_dolby SHARED src/plugins/foo_twins_dolby.cpp src/ac3file.cpp src/ac3.cpp src/engine.cpp src/surround.cpp
                ${PFC_SRC} ${SDK_SRC} ${CLIENT_SRC})
    set_target_properties(foo_twins_dolby PROPERTIES PREFIX "")
    target_include_directories(foo_twins_dolby PRIVATE "${FB2K_SDK_DIR}" "${FB2K_SDK_DIR}/foobar2000" "${FB2K_SDK_DIR}/foobar2000/SDK" "${FB2K_SDK_DIR}/pfc" src)
    target_compile_definitions(foo_twins_dolby PRIVATE NOMINMAX UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
      set(FB2K_SHARED_LIB "${FB2K_SDK_DIR}/foobar2000/shared/shared-x64.lib")
    else()
      set(FB2K_SHARED_LIB "${FB2K_SDK_DIR}/foobar2000/shared/shared.lib")
    endif()
    target_link_libraries(foo_twins_dolby PRIVATE "${FB2K_SHARED_LIB}" user32 shlwapi ole32 shell32 advapi32)
  endif()
endif()
