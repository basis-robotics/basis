function(add_plugin TARGET_NAME)
    get_filename_component(PARENT_DIR ${CMAKE_CURRENT_SOURCE_DIR} DIRECTORY)
    get_filename_component(PLUGIN_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
    get_filename_component(PLUGIN_TYPE ${PARENT_DIR} NAME)

    # Remove known arguments from the list
    list(REMOVE_AT ARGV 0)

    # Declare the library
    add_library(${TARGET_NAME} SHARED ${ARGV})

    set_target_properties(${TARGET_NAME} PROPERTIES
        # 
        BASIS_PLUGIN_TYPE ${PLUGIN_TYPE}
        # Rename it to "plugin_name.so"
        PREFIX ""
        OUTPUT_NAME ${PLUGIN_NAME})

    # install to the proper plugins dir
    install(TARGETS ${TARGET_NAME} DESTINATION plugins/${PLUGIN_TYPE})
    
endfunction()