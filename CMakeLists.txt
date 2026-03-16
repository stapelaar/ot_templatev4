cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ot_templatev4)

# Project headers (include/)
target_include_directories(app PRIVATE ${CMAKE_SOURCE_DIR}/include)

# App sources
target_sources(app PRIVATE
  src/main.c
  src/app_core.c
  src/transport.c
  src/coap_client.c
  src/topic.c
)

# >>> Forceer link van de SHT41 publisher als de toggle aan staat <<<
if(CONFIG_APP_USE_SHT41_SENSOR)
  target_sources(app PRIVATE lib/sensors_sht41/sensors_sht41.c)
endif()

if(CONFIG_APP_USE_SCD41_SENSOR)
  target_sources(app PRIVATE lib/sensors_scd41/sensors_scd41.c)
endif()


