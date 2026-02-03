# Scintilla + Lexilla build configuration
cmake_minimum_required(VERSION 3.16)

# Scintilla core sources
set(SCINTILLA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/scintilla)
set(LEXILLA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lexilla)

# Scintilla source files
file(GLOB SCINTILLA_SRC
    ${SCINTILLA_DIR}/src/*.cxx
    ${SCINTILLA_DIR}/call/*.cxx
    ${SCINTILLA_DIR}/win32/PlatWin.cxx
    ${SCINTILLA_DIR}/win32/ScintillaWin.cxx
    ${SCINTILLA_DIR}/win32/HanjaDic.cxx
)

# Lexilla library sources
file(GLOB LEXILLA_LEXLIB_SRC
    ${LEXILLA_DIR}/lexlib/*.cxx
)

# Lexilla lexers - include all to satisfy Lexilla.cxx
file(GLOB LEXILLA_LEXERS_SRC
    ${LEXILLA_DIR}/lexers/*.cxx
)

# Lexilla src (main entry)
set(LEXILLA_SRC
    ${LEXILLA_DIR}/src/Lexilla.cxx
)

# Create static library
add_library(scintilla STATIC
    ${SCINTILLA_SRC}
    ${LEXILLA_LEXLIB_SRC}
    ${LEXILLA_LEXERS_SRC}
    ${LEXILLA_SRC}
)

# Include directories
target_include_directories(scintilla PUBLIC
    ${SCINTILLA_DIR}/include
    ${LEXILLA_DIR}/include
)

target_include_directories(scintilla PRIVATE
    ${SCINTILLA_DIR}/src
    ${LEXILLA_DIR}/lexlib
)

# Compile definitions
target_compile_definitions(scintilla PRIVATE
    STATIC_BUILD
    SCI_LEXER
    _CRT_SECURE_NO_WARNINGS
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

# Windows libraries
target_link_libraries(scintilla PRIVATE
    imm32
    ole32
    oleaut32
    msimg32
    gdi32
    user32
)

# C++ standard
set_target_properties(scintilla PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Disable warnings for third-party code
if(MSVC)
    target_compile_options(scintilla PRIVATE /W0)
endif()
