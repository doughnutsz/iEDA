if(DEBUG_IRT_INTERFACE)            
    message(STATUS "RT: DEBUG_IRT_INTERFACE")       
    set(CMAKE_BUILD_TYPE "Debug")
else()
    message(STATUS "RT: RELEASE_IRT_INTERFACE")   
    set(CMAKE_BUILD_TYPE "Release")
endif()

add_library(irt_interface
    ${IRT_INTERFACE}/RTInterface.cpp
)

target_link_libraries(irt_interface
    PRIVATE
        irt_source
        idrc_pro_api
        ls_assigner
        power
)

target_include_directories(irt_interface
    PUBLIC
        ${IRT_INTERFACE}   
)
