
idf_component_register(
        SRCS "AD7124.c" "CN0391.c" "Communication.c" "z2m.c"
        INCLUDE_DIRS "."
        REQUIRES driver "espressif__esp-zigbee-lib"
)
target_compile_options(${COMPONENT_LIB} PRIVATE
        -Wno-unused-function
)