project(kcm_ufw)
cmake_minimum_required(VERSION 2.6.0)

find_package(KDE4 REQUIRED)
configure_file(config.h.cmake ${CMAKE_BINARY_DIR}/config.h)

add_subdirectory(helper)
add_subdirectory(kcm)

add_subdirectory(doc)

#kde4_install_icons( ${ICON_INSTALL_DIR} )
