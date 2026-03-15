zephyr_library()
zephyr_library_sources(sensors_sht41.c)

# include-paden/flags erven van Zephyr-interface
zephyr_library_link_libraries(zephyr_interface)

# >>> voeg dit toe zodat de lib jouw headers uit include/ vindt <<<
target_include_directories(${ZEPHYR_CURRENT_LIBRARY} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# generated headers (zephyr/offsets.h) - al toegevoegd in vorige stap
if(TARGET offsets_h)
  add_dependencies(${ZEPHYR_CURRENT_LIBRARY} offsets_h)
elseif(TARGET offsets)
  add_dependencies(${ZEPHYR_CURRENT_LIBRARY} offsets)
endif()
