cmake_minimum_required(VERSION 3.0.0)
project(inkbox-power-daemon VERSION 0.1.0)

include(CTest)
enable_testing()

# https://stackoverflow.com/questions/1662909/undefined-reference-to-pthread-create-in-linux
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

include_directories(${CMAKE_SOURCE_DIR}/lib/FBInk/)
include_directories(${CMAKE_SOURCE_DIR}/lib/libevdev/) 
include_directories(${CMAKE_SOURCE_DIR}/lib/FBInk/libi2c-staged/include/) 
include_directories(${CMAKE_SOURCE_DIR}/lib/boost_1_79_0/)

# https://stackoverflow.com/questions/3201154/automatically-add-all-files-in-a-folder-to-a-target-using-cmake
file(GLOB fileSrc
     "src/*.h"
     "src/*.cpp"
     "src/*.c"
)

link_directories(${CMAKE_SOURCE_DIR}/lib/libevdev/install/lib/)

# To be made better when integrated with scripts repo
link_directories(/home/build/qos/FBInk/Release/static/)
link_directories(/home/build/qos/FBInk/libi2c-staged/lib/)

add_executable(ipd ${fileSrc})

# https://stackoverflow.com/questions/33149878/experimentalfilesystem-linker-error
# The order of these here is important
target_link_libraries(ipd libevdev.a libfbink.a libi2c.a stdc++fs)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
