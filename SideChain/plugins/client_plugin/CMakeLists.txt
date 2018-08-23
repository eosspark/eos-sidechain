file(GLOB HEADERS "include/eosio/client_plugin/*.hpp")

add_library( client_plugin
             client_plugin.cpp
             client_manager.cpp
             ${HEADERS}
           )

target_link_libraries( client_plugin appbase fc chain_plugin http_client_plugin eosio_chain eos_utilities http_plugin)
target_include_directories( client_plugin
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )
