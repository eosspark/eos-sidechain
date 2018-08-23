file(GLOB HEADERS "include/eosio/confirm_plugin/*.hpp")
add_library( confirm_plugin
        confirm_plugin.cpp
        ${HEADERS} )

target_link_libraries( confirm_plugin net_plugin chain_plugin producer_plugin client_plugin appbase fc history_plugin)
target_include_directories( confirm_plugin PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/../chain_interface/include  "${CMAKE_CURRENT_SOURCE_DIR}/../../libraries/appbase/include")
