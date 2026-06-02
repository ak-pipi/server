
file(GLOB SERVER_SRC_LIST
  ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/*.h
  ${CMAKE_CURRENT_SOURCE_DIR}/*/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/*/*.h)

assign_folder_group(${SERVER_SRC_LIST})

add_executable(Server ${SERVER_SRC_LIST})

target_compile_options(Server PRIVATE ${COMPILE_OPTIONS_DEFAULT})
target_include_directories(Server PRIVATE "${CMAKE_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(Server PRIVATE ${OPENSSL_LIBRARIES} ${3RDPART_LIBRARIES} ${MYSQL_LIBRARIES} ${RABBITMQ_LIBRARIES} ${FRAMEWORK_LIBRARY} ${MAHJONG_LIBRARY} ${POKER_LIBRARY} ${Boost_LIBRARIES})
if(WIN32)
  target_link_directories(Server PRIVATE "${BOOST_ROOT}/lib")
elseif(APPLE)
  target_link_directories(Server PRIVATE "${Boost_INCLUDE_DIRS}/../lib")
  # mysql-connector-c++ JDBC 依赖的库搜索路径
  target_link_directories(Server PRIVATE "/opt/homebrew/opt/mysql-client/lib")
  target_link_directories(Server PRIVATE "/opt/homebrew/opt/openssl@3/lib")
else()
  target_link_directories(Server PRIVATE "${Boost_INCLUDE_DIRS}/../lib")
endif()