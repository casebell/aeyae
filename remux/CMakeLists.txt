cmake_minimum_required(VERSION 3.1.0)

project("aeyaeremux")
set(PROGNAME "aeyaeremux")

set(CMAKE_INCLUDE_CURRENT_DIR ON)
include("${PROJECT_SOURCE_DIR}/updateRevision.cmake")

find_package(OpenGL REQUIRED)

set(Boost_USE_MULTITHREAD ON)
find_package(Boost
  COMPONENTS
  thread
  date_time
  system
  filesystem
  locale
  REQUIRED)
add_definitions(-DBOOST_ALL_DYN_LINK)
add_definitions(-DBOOST_FILESYSTEM_NO_DEPRECATED)

add_definitions(-D__STDC_CONSTANT_MACROS)
add_definitions(-D__STDC_LIMIT_MACROS)

set(CMAKE_PREFIX_PATH "$ENV{CMAKE_PREFIX_PATH}" CACHE PATH
  "search path for .cmake files")

find_package(Qt5 COMPONENTS Core Gui Widgets OpenGL)
if (Qt5_FOUND)

  if (NOT (APPLE OR WIN32))
    find_package(Qt5DBus)
  endif ()

  option(YAE_USE_QOPENGL_WIDGET "Use QOpenGLWidget instead of QGLWidget" OFF)
  if (YAE_USE_QOPENGL_WIDGET)
    add_definitions(-DYAE_USE_QOPENGL_WIDGET)
  endif ()
else (Qt5_FOUND)
  find_package(Qt4 REQUIRED)
endif ()

if (QT4_FOUND)
  if (QT_QTDBUS_FOUND)
    set(QT_USE_QTDBUS TRUE)
  endif ()

  set(QT_USE_QTOPENGL TRUE)
  set(QT_USE_QTMAIN TRUE)
  include(${QT_USE_FILE})
endif ()


include_directories(AFTER
  .
  ..
  ../apprenticevideo
  ${PROJECT_BINARY_DIR}
  ${PROJECT_BINARY_DIR}/../apprenticevideo
  ${Boost_INCLUDE_DIR}
  ${OPENGL_INCLUDE_DIR}
  ${QT_INCLUDE_DIR}
  ${QT_QTOPENGL_INCLUDE_DIR}
  )

if (WIN32 AND NOT (MINGW OR MSYS))
  include_directories(AFTER ../apprenticevideo/msIntTypes)
endif ()


set(GLEW_DIR "$ENV{GLEW_DIR}"
  CACHE
  PATH "root path for GLEW lib/ and include/ folders")
find_library(GLEW_LIBRARY GLEW
  PATHS ${GLEW_DIR}/lib ${FFMPEG_LIBS_PATH}
  DOC "GLEW library"
  )


set(project_moc_hxx

  yaeDemuxerView.h
  yaeMainWindow.h
  yaeSpinnerView.h

  yae_player_item.h

  ../apprenticevideo/yaeCanvasWidget.h
  ../apprenticevideo/yaeItemView.h
  ../apprenticevideo/yaeScreenSaverInhibitor.h
  ../apprenticevideo/yaeTextInput.h
  ../apprenticevideo/yaeTimelineItem.h
  ../apprenticevideo/yaeTimelineModel.h
  )

set(project_uis
  yaeAbout.ui
  yaeMainWindow.ui
  )

set(project_sources
  yaeDemuxerView.cpp
  yaeDemuxerView.h
  yaeMain.cpp
  yaeMainWindow.cpp
  yaeMainWindow.h
  yaeRemux.cpp
  yaeRemux.h
  yaeSpinnerView.cpp
  yaeSpinnerView.h

  yae_player_item.cpp
  yae_player_item.h

  yae_axis_item.cpp
  yae_axis_item.h
  yae_checkbox_item.cpp
  yae_checkbox_item.h
  yae_input_proxy_item.cpp
  yae_input_proxy_item.h
  yae_lshape.cpp
  yae_lshape.h
  yae_plot_item.cpp
  yae_plot_item.h
  yae_tab_rect.cpp
  yae_tab_rect.h

  ../apprenticevideo/yaeBBox.cpp
  ../apprenticevideo/yaeBBox.h
  ../apprenticevideo/yaeCanvas.h
  ../apprenticevideo/yaeCanvas.cpp
  ../apprenticevideo/yaeCanvasWidget.h
  ../apprenticevideo/yaeCanvasQPainterUtils.h
  ../apprenticevideo/yaeCanvasQPainterUtils.cpp
  ../apprenticevideo/yaeCanvasRenderer.h
  ../apprenticevideo/yaeCanvasRenderer.cpp
  ../apprenticevideo/yaeColor.h
  ../apprenticevideo/yaeDashedRect.cpp
  ../apprenticevideo/yaeDashedRect.h
  ../apprenticevideo/yaeDonutRect.cpp
  ../apprenticevideo/yaeDonutRect.h
  ../apprenticevideo/yaeExpression.h
  ../apprenticevideo/yaeFlickableArea.cpp
  ../apprenticevideo/yaeFlickableArea.h
  ../apprenticevideo/yaeGradient.cpp
  ../apprenticevideo/yaeGradient.h
  ../apprenticevideo/yaeImageProvider.cpp
  ../apprenticevideo/yaeImageProvider.h
  ../apprenticevideo/yaeInputArea.cpp
  ../apprenticevideo/yaeInputArea.h
  ../apprenticevideo/yaeItem.cpp
  ../apprenticevideo/yaeItem.h
  ../apprenticevideo/yaeItemFocus.cpp
  ../apprenticevideo/yaeItemFocus.h
  ../apprenticevideo/yaeItemRef.h
  ../apprenticevideo/yaeItemView.cpp
  ../apprenticevideo/yaeItemView.h
  ../apprenticevideo/yaeItemViewStyle.cpp
  ../apprenticevideo/yaeItemViewStyle.h
  ../apprenticevideo/yaeProperty.h
  ../apprenticevideo/yaeRectangle.cpp
  ../apprenticevideo/yaeRectangle.h
  ../apprenticevideo/yaeRoundRect.cpp
  ../apprenticevideo/yaeRoundRect.h
  ../apprenticevideo/yaeScreenSaverInhibitor.h
  ../apprenticevideo/yaeScreenSaverInhibitor.cpp
  ../apprenticevideo/yaeScrollview.cpp
  ../apprenticevideo/yaeScrollview.h
  ../apprenticevideo/yaeSegment.cpp
  ../apprenticevideo/yaeSegment.h
  ../apprenticevideo/yaeText.cpp
  ../apprenticevideo/yaeText.h
  ../apprenticevideo/yaeTextInput.cpp
  ../apprenticevideo/yaeTextInput.h
  ../apprenticevideo/yaeTexture.cpp
  ../apprenticevideo/yaeTexture.h
  ../apprenticevideo/yaeTexturedRect.cpp
  ../apprenticevideo/yaeTexturedRect.h
  ../apprenticevideo/yaeTimelineItem.h
  ../apprenticevideo/yaeTimelineItem.cpp
  ../apprenticevideo/yaeTimelineModel.h
  ../apprenticevideo/yaeTimelineModel.cpp
  ../apprenticevideo/yaeTransform.cpp
  ../apprenticevideo/yaeTransform.h
  ../apprenticevideo/yaeUtilsQt.h
  ../apprenticevideo/yaeUtilsQt.cpp
  ../apprenticevideo/yaeVec.h
  ../apprenticevideo/yaeVersion.cpp
  )

if (QT4_FOUND)
  add_definitions(-DYAE_USE_QT4)
  qt4_add_resources(project_qrc_cxx ${PROGNAME}.qrc)
endif ()

if (Qt5_FOUND)
  add_definitions(-DYAE_USE_QT5)
  qt5_add_resources(project_qrc_cxx ${PROGNAME}.qrc)
endif ()

if (APPLE)
  set(project_sources
    ${project_sources}
    ../apprenticevideo/yaeAppleUtils.h
    ../apprenticevideo/yaeAppleUtils.mm
    ../apprenticevideo/yaeAudioUnitRenderer.h
    ../apprenticevideo/yaeAudioUnitRenderer.mm
    )
else ()
  set(project_sources
    ${project_sources}
    ../apprenticevideo/yaePortaudioRenderer.h
    ../apprenticevideo/yaePortaudioRenderer.cpp)
endif ()


find_path(LIBASS_INCLUDE_DIR
  ass/ass.h
  PATHS
  ${LIBASS_DIR}/include
  ${FFMPEG_HEADERS_PATH}
  /usr/include
  /usr/local/include
  /opt/local/include)

if (LIBASS_INCLUDE_DIR)
  include_directories(AFTER ${LIBASS_INCLUDE_DIR})
endif ()

if (LIBASS_LIBRARY)
  add_definitions(-DYAE_USE_LIBASS)
  set(project_sources
    ${project_sources}
    ../apprenticevideo/yaeLibass.h
    ../apprenticevideo/yaeLibass.cpp
    )
endif ()


if (NOT (APPLE))
  set(PORTAUDIO_DIR "$ENV{PORTAUDIO_DIR}"
    CACHE
    PATH "root path for portaudio lib/ and include/ folders")

  find_library(PORTAUDIO_LIBRARY portaudio
    PATHS ${PORTAUDIO_DIR}/lib ${FFMPEG_LIBS_PATH}
    DOC "portaudio library")

  find_path(PORTAUDIO_INCLUDE_DIR
    portaudio.h
    PATHS
    ${PORTAUDIO_DIR}/include
    ${FFMPEG_HEADERS_PATH}
    /usr/include
    /usr/local/include
    /opt/local/include)

  if (PORTAUDIO_INCLUDE_DIR)
    include_directories(AFTER ${PORTAUDIO_INCLUDE_DIR})
  endif ()
endif()


find_path(GLEW_INCLUDE_DIR
  GL/glew.h
  PATHS
  ${GLEW_DIR}/include
  ${FFMPEG_HEADERS_PATH}
  /usr/include
  /usr/local/include
  /opt/local/include)

if (GLEW_INCLUDE_DIR)
  include_directories(AFTER ${GLEW_INCLUDE_DIR})
endif ()


if (WIN32)
  set (YAE_VERSIONED_FILE "${PROJECT_BINARY_DIR}/${PROGNAME}.rc")
endif ()

if (APPLE)
  set (YAE_VERSIONED_FILE "${PROJECT_BINARY_DIR}/${PROGNAME}.plist")
endif ()

add_custom_target("update_revision_aeyaeremux" ALL
  DEPENDS
  "${YAE_VERSIONED_FILE}.tmp"
  )

add_custom_command(OUTPUT
  "${YAE_VERSIONED_FILE}.tmp"
  COMMAND "${CMAKE_COMMAND}"
  -DPROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
  -DPROJECT_BINARY_DIR="${PROJECT_BINARY_DIR}"
  -DPROGNAME="${PROGNAME}"
  -DSubversion_SVN_EXECUTABLE="${Subversion_SVN_EXECUTABLE}"
  -DGIT_EXECUTABLE="${GIT_EXECUTABLE}"
  -P "${PROJECT_SOURCE_DIR}/updateRevision.cmake")

set(project_sources ${project_sources} ${YAE_VERSIONED_FILE})

# Set some Win32 Specific Settings
if (WIN32)
  set(GUI_TYPE WIN32)

  if (MSYS OR MINGW)
    set(CMAKE_RC_COMPILER_INIT windres)
    enable_language(RC)
    set(CMAKE_RC_COMPILE_OBJECT
      "<CMAKE_RC_COMPILER> <FLAGS> -O coff <DEFINES> -i <SOURCE> -o <OBJECT>")
  endif ()
endif ()

# Set some Apple MacOS Specific settings
if (APPLE)
  set(GUI_TYPE MACOSX_BUNDLE)

  set(project_sources
    ${project_sources}
    ${PROGNAME}.icns
    InfoPlist.strings
    )
endif ()

if (QT4_FOUND)
  qt4_wrap_cpp(project_moc_cxx ${project_moc_hxx})
  qt4_wrap_ui(project_uis_hxx ${project_uis})
endif ()

if (Qt5_FOUND)
  qt5_wrap_cpp(project_moc_cxx ${project_moc_hxx})
  qt5_wrap_ui(project_uis_hxx ${project_uis})
endif ()

set(project_sources
  ${project_sources}
  ${project_moc_cxx}
  ${project_uis_hxx}
  ${project_qrc_cxx}
  )

add_executable(${PROGNAME}
  ${GUI_TYPE}
  ${project_sources}
  )

add_dependencies(${PROGNAME} "update_revision_aeyaeremux")

set(TARGET_LIBS
  ${TARGET_LIBS}
  aeyae
  ${LIBASS_LIBRARY}
  ${GLEW_LIBRARY}
  ${QT_QTOPENGL_LIBRARY}
  ${QT_LIBRARIES}
  ${OPENGL_LIBRARIES}
  ${Boost_LIBRARIES}
  )

if (PORTAUDIO_LIBRARY)
  set(TARGET_LIBS ${TARGET_LIBS} ${PORTAUDIO_LIBRARY})
endif ()

if (APPLE)
  find_library(CORE_AUDIO_FRAMEWORK CoreAudio)
  mark_as_advanced(CORE_AUDIO_FRAMEWORK)
  if (CORE_AUDIO_FRAMEWORK)
    set(TARGET_LIBS ${TARGET_LIBS} ${CORE_AUDIO_FRAMEWORK})
  endif ()

  find_library(AUDIO_TOOLBOX_FRAMEWORK AudioToolbox)
  mark_as_advanced(AUDIO_TOOLBOX_FRAMEWORK)
  if (AUDIO_TOOLBOX_FRAMEWORK)
    set(TARGET_LIBS ${TARGET_LIBS} ${AUDIO_TOOLBOX_FRAMEWORK})
  endif ()

  find_library(AUDIO_UNIT_FRAMEWORK AudioUnit)
  mark_as_advanced(AUDIO_UNIT_FRAMEWORK)
  if (AUDIO_UNIT_FRAMEWORK)
    set(TARGET_LIBS ${TARGET_LIBS} ${AUDIO_UNIT_FRAMEWORK})
  endif ()
endif ()

if (WIN32)

  if (LIBGCC_LIBRARY)
    set(TARGET_LIBS ${TARGET_LIBS} ${LIBGCC_LIBRARY})
  endif ()

  if (LIBMINGW32_LIBRARY)
    set(TARGET_LIBS ${TARGET_LIBS} ${LIBMINGW32_LIBRARY})
  endif ()

  if (LIBMINGWEX_LIBRARY)
    set(TARGET_LIBS ${TARGET_LIBS} ${LIBMINGWEX_LIBRARY})
  endif ()

  if (LIBCOLDNAME_LIBRARY)
    set(TARGET_LIBS ${TARGET_LIBS} ${LIBCOLDNAME_LIBRARY})
  endif ()

  if (LIBMOLDNAME_RELEASE_LIBRARY)
    set(TARGET_LIBS ${TARGET_LIBS} ${LIBMOLDNAME_RELEASE_LIBRARY})
  endif ()

  if (LIBMOLDNAME_DEBUG_LIBRARY)
    set(TARGET_LIBS ${TARGET_LIBS} ${LIBMOLDNAME_DEBUG_LIBRARY})
  endif ()

  if (AVICAP32_LIBRARY)
    set(TARGET_LIBS ${TARGET_LIBS} ${AVICAP32_LIBRARY})
  endif ()

  if (AVIFIL32_LIBRARY)
    set(TARGET_LIBS ${TARGET_LIBS} ${AVIFIL32_LIBRARY})
  endif ()

  if (ICUUC_LIBRARY)
    set(TARGET_LIBS ${TARGET_LIBS} ${ICUUC_LIBRARY})
  endif ()

  if (ICUDT_LIBRARY)
    set(TARGET_LIBS ${TARGET_LIBS} ${ICUDT_LIBRARY})
  endif ()

  if (MSYS OR MINGW)
    set(TARGET_LIBS ${TARGET_LIBS}
      -L/Developer/ffmpeg/lib
      vo-aacenc
      vo-amrwbenc
      orc-0.4
      xavs
      pthread
      wsock32
      winmm)
  endif ()
endif ()

if (QT4_FOUND)
  #message("QT_CONFG: "${QT_CONFIG})
  if (QT_CONFIG MATCHES "static" OR QT4_PLUGINS_QTIFF MATCHES ".a$")
    add_definitions(-DQT_LIBS_ARE_STATIC)

    if (QT4_PLUGINS_QGIF)
      set(TARGET_LIBS ${TARGET_LIBS} ${QT4_PLUGINS_QGIF})
    endif ()

    if (QT4_PLUGINS_QTIFF)
      set(TARGET_LIBS ${TARGET_LIBS} ${QT4_PLUGINS_QTIFF})
    endif ()

    if (QT4_PLUGINS_QICO)
      set(TARGET_LIBS ${TARGET_LIBS} ${QT4_PLUGINS_QICO})
    endif ()

    if (QT4_PLUGINS_QJPEG)
      set(TARGET_LIBS ${TARGET_LIBS} ${QT4_PLUGINS_QJPEG})
    endif ()

    if (QT4_PLUGINS_QMNG)
      set(TARGET_LIBS ${TARGET_LIBS} ${QT4_PLUGINS_QMNG})
    endif ()

  endif ()
endif ()

if (WIN32)
  set(TARGET_LIBS
    ${TARGET_LIBS}
    ws2_32
    psapi
    )
endif ()

if (APPLE)
  find_library(CORE_SERVICES_FRAMEWORK CoreServices)
  mark_as_advanced(CORE_SERVICES_FRAMEWORK)
  if (CORE_SERVICES_FRAMEWORK)
    set(TARGET_LIBS ${TARGET_LIBS} ${CORE_SERVICES_FRAMEWORK})
  endif ()

  find_library(CORE_VIDEO_FRAMEWORK CoreVideo)
  mark_as_advanced(CORE_VIDEO_FRAMEWORK)
  if (CORE_VIDEO_FRAMEWORK)
    set(TARGET_LIBS ${TARGET_LIBS} ${CORE_VIDEO_FRAMEWORK})
  endif ()

  find_library(CORE_VDA_FRAMEWORK VideoDecodeAcceleration)
  mark_as_advanced(CORE_VDA_FRAMEWORK)
  if (CORE_VDA_FRAMEWORK)
    set(TARGET_LIBS ${TARGET_LIBS} ${CORE_VDA_FRAMEWORK})
  endif ()

  find_library(COCOA_FRAMEWORK Cocoa)
  mark_as_advanced(COCOA_FRAMEWORK)
  if (COCOA_FRAMEWORK)
    set(TARGET_LIBS ${TARGET_LIBS} ${COCOA_FRAMEWORK})
  endif ()

  find_library(IOKIT_FRAMEWORK IOKit)
  mark_as_advanced(IOKIT_FRAMEWORK)
  if (IOKIT_FRAMEWORK)
    set(TARGET_LIBS ${TARGET_LIBS} ${IOKIT_FRAMEWORK})
  endif ()
endif ()

if (Qt5_FOUND)
  set(TARGET_LIBS ${TARGET_LIBS}
    Qt5::Core
    Qt5::Gui
    Qt5::OpenGL
    Qt5::Widgets
    )

  if (Qt5DBus_FOUND)
    set(TARGET_LIBS ${TARGET_LIBS}
      Qt5::DBus
      )
  endif ()
endif ()

set_property(TARGET ${PROGNAME} PROPERTY CXX_STANDARD 98)
# set_property(TARGET ${PROGNAME} PROPERTY CXX_STANDARD 11)

target_link_libraries(${PROGNAME}
  ${TARGET_LIBS}
  )

if (Qt4_FOUND)
  get_filename_component(QT_QMAKE_EXECUTABLE_DIR
    ${QT_QMAKE_EXECUTABLE} DIRECTORY)
  # message("QT_QMAKE_EXECUTABLE_DIR: " ${QT_QMAKE_EXECUTABLE_DIR})
endif ()

if (Qt5_FOUND)
  get_filename_component(QT_QMAKE_EXECUTABLE_DIR
    ${Qt5_DIR}/../../../bin ABSOLUTE)
  # message("QT_QMAKE_EXECUTABLE_DIR: " ${QT_QMAKE_EXECUTABLE_DIR})
endif ()

if (Qt5_FOUND)
  find_program(QT_QMAKE_EXECUTABLE NAMES qmake
    HINTS ${QT_QMAKE_EXECUTABLE_DIR}
    DOC
    "qmake - Qt build configuration utility")
endif ()

if (APPLE)
  set_target_properties(${PROGNAME}
    PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST ${PROJECT_BINARY_DIR}/${PROGNAME}.plist)

  set_source_files_properties(${PROJECT_SOURCE_DIR}/${PROGNAME}.icns
    PROPERTIES
    MACOSX_PACKAGE_LOCATION Resources)

  set_source_files_properties(${PROJECT_SOURCE_DIR}/InfoPlist.strings
    PROPERTIES
    MACOSX_PACKAGE_LOCATION Resources/en.lproj)

  find_program(MACDEPLOYQT NAMES macdeployqt
    HINTS ${QT_QMAKE_EXECUTABLE_DIR}
    DOC
    "macdeployqt - Qt utility used to include frameworks required by the application into its bundle")

  add_custom_command(TARGET ${PROGNAME} POST_BUILD
    COMMAND ${PROJECT_SOURCE_DIR}/../apprenticevideo/DeployOSX.sh
    ARGS
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_CFG_INTDIR}/${PROGNAME}.app
    ${MACDEPLOYQT}
    ${CMAKE_INSTALL_PREFIX}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    ${CMAKE_CFG_INTDIR}
    COMMENT "pull in external dependencies"
    )
endif ()

if (WIN32)
  if (Qt5_FOUND)
    execute_process(COMMAND ${QT_QMAKE_EXECUTABLE} -query QT_INSTALL_PLUGINS
      OUTPUT_VARIABLE QT_PLUGINS_DIR
      OUTPUT_STRIP_TRAILING_WHITESPACE)
    set(DEPLOY_QT_PLUGINS
      "-deploy-to" "platforms" "${QT_PLUGINS_DIR}/platforms" "qwindows\.dll"
      "-deploy-to" "imageformats" "${QT_PLUGINS_DIR}/imageformats" ".*\.dll"
      )
  endif ()

  if (MINGW OR MSYS)
    string(REPLACE "/" "\\\\" NATIVE_PROGRAM_ICON "${PROJECT_SOURCE_DIR}/${PROGNAME}.ico")
    string(REPLACE "/" "\\\\" NATIVE_DEPENDS_EXE "${DEPENDENCY_WALKER}")
    string(REPLACE "/" "\\\\" NATIVE_ALLOWED_PATHS "${ALLOWED_PATHS}")
    string(REPLACE "/" "\\\\" NATIVE_CANDLE_EXE "${WIX_CANDLE}")
    string(REPLACE "/" "\\\\" NATIVE_LIGHT_EXE "${WIX_LIGHT}")
    string(REPLACE "/" "\\\\" NATIVE_VC_CRT_MSM "${VC_CRT_MSM}")
  else (MINGW OR MSYS)
    file(TO_NATIVE_PATH "${PROJECT_SOURCE_DIR}/${PROGNAME}.ico" NATIVE_PROGRAM_ICON)
    file(TO_NATIVE_PATH "${DEPENDENCY_WALKER}" NATIVE_DEPENDS_EXE)
    file(TO_NATIVE_PATH "${ALLOWED_PATHS}" NATIVE_ALLOWED_PATHS)
    file(TO_NATIVE_PATH "${WIX_CANDLE}" NATIVE_CANDLE_EXE)
    file(TO_NATIVE_PATH "${WIX_LIGHT}" NATIVE_LIGHT_EXE)
    file(TO_NATIVE_PATH "${VC_CRT_MSM}" NATIVE_VC_CRT_MSM)
  endif ()

  add_custom_target("create_aeyaeremux_installer"
    ALL DEPENDS
    wixWrapper
    "${PROJECT_BINARY_DIR}/depends-exe-log.txt"
    )

  add_custom_command(OUTPUT "${PROJECT_BINARY_DIR}/depends-exe-log.txt"
    COMMAND wixWrapper
    ARGS
    "-what" "aeyaeremux"
    "-dep-walker" "${NATIVE_DEPENDS_EXE}"
    "-allow" "$<TARGET_FILE_DIR:${PROGNAME}>;${NATIVE_ALLOWED_PATHS}"
    "-wix-candle" "${NATIVE_CANDLE_EXE}"
    "-wix-light" "${NATIVE_LIGHT_EXE}"
    "-vc-redist" "${NATIVE_VC_CRT_MSM}"
    "-icon" "${NATIVE_PROGRAM_ICON}"
    "-url" "https://github.com/pkoshevoy/aeyae"
    "-deploy" $<TARGET_FILE:${PROGNAME}>
    "-deploy" $<TARGET_FILE:aeyae>
    ${DEPLOY_QT_PLUGINS}
    DEPENDS ${PROGNAME} wixWrapper
    COMMENT "create aeyaeremux installer package"
    )
endif ()

if (NOT (WIN32 OR APPLE))
  install(TARGETS ${PROGNAME} DESTINATION bin)

  # qmake -query QT_INSTALL_LIBS
  execute_process(COMMAND ${QT_QMAKE_EXECUTABLE} -query QT_INSTALL_LIBS
    OUTPUT_VARIABLE QT_LIBRARIES_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE)

  configure_file(
    "${PROJECT_SOURCE_DIR}/${PROGNAME}.desktop.in"
    "${PROJECT_BINARY_DIR}/${PROGNAME}.desktop"
    @ONLY
    )

  install(FILES "${PROJECT_BINARY_DIR}/${PROGNAME}.desktop"
    DESTINATION share/applications
    )

  install(FILES "${PROJECT_SOURCE_DIR}/images/aeyae-remux-logo.png"
    DESTINATION share/icons
    RENAME "${PROGNAME}.png"
    )
endif ()

add_dependencies(${PROGNAME} aeyae)
