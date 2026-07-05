set(SOURCES 
    "main.cc"
    "application.cc"
)

set(INCLUDE_DIRS "." "display" "sensor_manager")

list(APPEND SOURCES 
     "boards/common/board.cc" 
 ) 
 list(APPEND INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/boards/common)

if(CONFIG_BOARD_TYPE_TIANSHU_MESH)
    set(BOARD_TYPE "tianshu-mesh")
endif()

file(GLOB BOARD_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_TYPE}/*.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_TYPE}/*.c
    )

list(APPEND SOURCES ${BOARD_SOURCES})

file(GLOB DISPLAY_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/display/*.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/display/*.c
    )

file(GLOB SENSOR_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/sensor_manager/*.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/sensor_manager/*.c
    )

list(APPEND SOURCES ${DISPLAY_SOURCES} ${SENSOR_SOURCES})

idf_component_register(SRCS ${SOURCES}
                    INCLUDE_DIRS ${INCLUDE_DIRS}
                    WHOLE_ARCHIVE
                    PRIV_REQUIRES
                        bt
                        esp_netif
                        esp_wifi
                        app_update
                        nvs_flash
                        spi_flash
                        esp_pm
                        esp_driver_gpio
                        esp_driver_i2c
                        efuse
                        console
                        freertos
                        esp_system
                        esp_event
                        driver
                        tianshu_mesh_core
                        tianshu_web_service
                    )

target_compile_definitions(${COMPONENT_LIB}
                        PRIVATE BOARD_TYPE=\"${BOARD_TYPE}\"
                        )