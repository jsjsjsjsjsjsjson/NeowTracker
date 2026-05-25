idf_component_register(
    SRCS
        "../../src/FamiGFXCore.cpp"
        "../../src/FamiGFX.cpp"
        "../../src/FamiGFXFonts.cpp"
        "FamiGFXPortDefault.cpp"
    INCLUDE_DIRS
        "../../include"
    REQUIRES
        esp_lcd
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE FAMIGFX_HAS_DEFAULT_PORT=1)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_11)
