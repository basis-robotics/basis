function(generate_unit UNIT_NAME)
    set(TARGET_NAME "unit_${UNIT_NAME}")

    cmake_parse_arguments(
        PARSED_ARGS # prefix of output variables
        "" # list of names of the boolean arguments (only defined ones will be true)
        "" # list of names of mono-valued arguments
        "DEPENDS" # list of names of multi-valued arguments (output variables are lists)
        ${ARGN} # arguments of the function to parse, here we take the all original ones
    )
    set(UNIT_FILE_NAME "${CMAKE_CURRENT_SOURCE_DIR}/${UNIT_NAME}.unit.yaml")
    set(GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)

    make_directory(${GENERATED_DIR})
    make_directory(${CMAKE_CURRENT_SOURCE_DIR}/src)
    make_directory(${CMAKE_CURRENT_SOURCE_DIR}/include)
    make_directory(${CMAKE_CURRENT_SOURCE_DIR}/template)

    add_custom_command(
        COMMAND 
            ${BASIS_SOURCE_ROOT}/python/unit/generate_unit.py 
                ${UNIT_FILE_NAME}
                ${CMAKE_CURRENT_BINARY_DIR}/generated
                ${CMAKE_CURRENT_SOURCE_DIR}
        DEPENDS
            ${BASIS_SOURCE_ROOT}/python/unit/generate_unit.py
            ${CMAKE_CURRENT_SOURCE_DIR}/${UNIT_NAME}.unit.yaml
            ${BASIS_SOURCE_ROOT}/python/unit/templates/handler.h.j2
            ${BASIS_SOURCE_ROOT}/python/unit/templates/unit_base.h.j2
            ${BASIS_SOURCE_ROOT}/python/unit/templates/unit_base.cpp.j2
            ${BASIS_SOURCE_ROOT}/python/unit/templates/unit.h.j2
            ${BASIS_SOURCE_ROOT}/python/unit/templates/create_unit.cpp.j2
        OUTPUT
            ${GENERATED_DIR}/unit/${UNIT_NAME}/unit_base.h
            ${GENERATED_DIR}/unit/${UNIT_NAME}/unit_base.cpp
            ${GENERATED_DIR}/unit/${UNIT_NAME}/create_unit.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/${UNIT_NAME}.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/include/${UNIT_NAME}.h
            ${CMAKE_CURRENT_SOURCE_DIR}/template/${UNIT_NAME}.example.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/template/${UNIT_NAME}.example.h
        )
    # TODO: we technically output a bunch of header files also
    # we should just generate the list of files with a --dry-run flag

    add_library(${TARGET_NAME} SHARED
            src/${UNIT_NAME}.cpp
            ${GENERATED_DIR}/unit/${UNIT_NAME}/unit_base.cpp
            ${GENERATED_DIR}/unit/${UNIT_NAME}/create_unit.cpp
        )

    target_include_directories(${TARGET_NAME} PUBLIC ${GENERATED_DIR} include)
    target_link_libraries(${TARGET_NAME} basis::unit basis::synchronizers ${PARSED_ARGS_DEPENDS})
    set_target_properties(${TARGET_NAME} PROPERTIES OUTPUT_NAME "${UNIT_NAME}.unit")
    set_target_properties(${TARGET_NAME} PROPERTIES PREFIX "")

    add_executable(${TARGET_NAME}_bin 
        ${GENERATED_DIR}/unit/${UNIT_NAME}/unit_main.cpp
    )
    target_link_libraries(${TARGET_NAME}_bin ${TARGET_NAME} basis::unit::main)
    set_target_properties(${TARGET_NAME}_bin PROPERTIES OUTPUT_NAME ${UNIT_NAME})

    add_library("unit::${UNIT_NAME}" ALIAS ${TARGET_NAME})

    install(TARGETS ${TARGET_NAME} DESTINATION unit/)
    install(FILES ${UNIT_FILE_NAME} DESTINATION unit/)   

endfunction()