cmake_minimum_required(VERSION 3.14)
project(saber_protocol VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Imposta le variabili di installazione
include(GNUInstallDirs)

# Trova le dipendenze
find_package(OpenSSL REQUIRED)
find_package(pybind11 REQUIRED)
find_package(Threads REQUIRED)

# Aggiungi le directory di include
include_directories(include)

# Definisci i file sorgente
set(SOURCES
    protocol/saber_protocol.cpp
    protocol/mesh.cpp
    protocol/sync.cpp
    protocol/crypto.cpp
)

# Crea la libreria statica
add_library(saber_protocol_static STATIC ${SOURCES})
target_link_libraries(saber_protocol_static
    PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
    sodium
)

# Crea il modulo Python
pybind11_add_module(saber_protocol pybind/pybind_module.cpp ${SOURCES})
target_link_libraries(saber_protocol
    PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
    sodium
)

# Installa la libreria
install(TARGETS saber_protocol
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
