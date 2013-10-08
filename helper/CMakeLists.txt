include_directories(${KDE4_INCLUDES} ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_BINARY_DIR})

set(kcm_ufw_helper_SRCS helper.cpp)
kde4_add_executable(kcm_ufw_helper ${kcm_ufw_helper_SRCS})

set_target_properties(kcm_ufw_helper PROPERTIES OUTPUT_NAME kcm_ufw_helper)
target_link_libraries(kcm_ufw_helper ${KDE4_KDECORE_LIBS})
configure_file(kcm_ufw_helper.py.cmake ${CMAKE_BINARY_DIR}/kcm_ufw_helper.py)

install(TARGETS kcm_ufw_helper DESTINATION ${LIBEXEC_INSTALL_DIR})
install(PROGRAMS ${CMAKE_BINARY_DIR}/kcm_ufw_helper.py DESTINATION ${LIBEXEC_INSTALL_DIR})

configure_file(org.kde.ufw.service.cmake ${CMAKE_CURRENT_BINARY_DIR}/session/org.kde.ufw.service)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/session/org.kde.ufw.service DESTINATION ${DBUS_SERVICES_INSTALL_DIR})
install(FILES defaults DESTINATION ${DATA_INSTALL_DIR}/kcm_ufw)

kde4_install_auth_helper_files(kcm_ufw_helper org.kde.ufw root)
kde4_install_auth_actions(org.kde.ufw ufw.actions)
