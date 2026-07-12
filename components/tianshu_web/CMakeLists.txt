idf_component_register(SRCS 
                        "web_server/web_server.cc"
                    INCLUDE_DIRS 
                        "."
                        "web_server"
                    PRIV_REQUIRES
                        esp_http_server
                        esp_netif
                        vfs
                        spiffs
                        json
                        esp_system
                    )

spiffs_create_partition_image(web ${CMAKE_CURRENT_SOURCE_DIR}/web_static FLASH_IN_PROJECT)