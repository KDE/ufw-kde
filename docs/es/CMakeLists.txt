kde4_create_handbook(index.docbook INSTALL_DESTINATION ${HTML_INSTALL_DIR}/es SUBDIR kcontrol/ufw)

# Jpegs not installed by above line???

install(FILES 
        example-1-general.jpg
        example-2-ssh.jpg
        example-3-dropbox1.jpg
        example-4-dropbox2.jpg
        example-5-samba.jpg
        example-5-samba-module.jpg
        example-6-ktorrent.jpg
        example-7-all-rules.jpg
        DESTINATION
        ${HTML_INSTALL_DIR}/es/kcontrol/ufw)

