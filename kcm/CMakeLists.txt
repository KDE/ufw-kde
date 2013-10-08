include_directories(${KDE4_INCLUDES} ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_BINARY_DIR})

set(kcm_ufw_SRCS kcm.cpp ruledialog.cpp types.cpp strings.cpp rule.cpp ruleslist.cpp profile.cpp appprofiles.cpp 
    statusbox.cpp stackedwidget.cpp combobox.cpp lineedit.cpp blocker.cpp logviewer.cpp)
kde4_add_ui_files(kcm_ufw_SRCS ufw.ui rulewidget.ui)
kde4_add_plugin(kcm_ufw ${kcm_ufw_SRCS})

target_link_libraries(kcm_ufw ${KDE4_KIO_LIBS})

install( TARGETS kcm_ufw  DESTINATION ${PLUGIN_INSTALL_DIR} )
install( FILES ufw.desktop  DESTINATION  ${SERVICES_INSTALL_DIR} )

#kde4_install_icons( ${DATA_INSTALL_DIR}/kufw/icons )
