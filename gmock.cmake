
add_library(gmock 
  contrib/gmock-gtest-all.cc
)
target_include_directories(gmock
  SYSTEM PUBLIC contrib/gtest contrib/gmock contrib)
target_link_libraries(gmock pthread)
