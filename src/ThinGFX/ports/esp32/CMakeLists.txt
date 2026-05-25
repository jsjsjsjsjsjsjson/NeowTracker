idf_component_register(
    SRCS
        "../../src/ThinGFXCore.cpp"
        "../../src/ThinGFX.cpp"
        "../../src/ThinGFXFonts.cpp"
        "ThinGFXPortDefault.cpp"
    INCLUDE_DIRS
        "../../include"
    REQUIRES
        esp_lcd
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE THINGFX_HAS_DEFAULT_PORT=1)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_11)
