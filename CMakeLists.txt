zephyr_include_directories(include)
zephyr_library()

if(CONFIG_SHIELD_JPF_ADAPTER)

        add_subdirectory(${ZEPHYR_CURRENT_MODULE_DIR}/drivers/display)


endif()