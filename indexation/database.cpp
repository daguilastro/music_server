#include "database.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

using namespace std;

// ===== CREAR BASE DE DATOS VACÍA =====
SongDatabase* createDatabase() {
    SongDatabase* db = new SongDatabase();
    
    db->songCapacity = 100;
    db->songs = new Song[db->songCapacity];
    db->songCount = 0;
    db->nextSongId = 1;
    
    cout << "[INFO] Base de datos creada (vacía) en RAM" << endl;
    
    return db;
}

// ===== LIBERAR MEMORIA =====
void freeDatabase(SongDatabase* db) {
    if (!db) return;
    delete[] db->songs;
    delete db;
}

// ===== VERIFICAR URL DUPLICADA =====
bool isDuplicateURL(SongDatabase* db, const char* url) {
    for (int i = 0; i < db->songCount; i++) {
        if (strcmp(db->songs[i].url, url) == 0) {
            return true;
        }
    }
    return false;
}

long getSongOffsetInFile(SongDatabase* db, uint32_t id) {
    // Encontrar índice de la canción
    int index = -1;
    for (int i = 0; i < db->songCount; i++) {
        if (db->songs[i].id == id) {
            index = i;
            break;
        }
    }
    
    if (index < 0) {
        return -1;  // No encontrada
    }
    
    // Calcular offset: header + (índice × tamaño de Song)
    long offset = sizeof(DatabaseHeader) + (index * sizeof(Song));
    
    return offset;
}

// ===== AÑADIR CANCIÓN =====
int addSong(SongDatabase* db, const char* title, const char* filename,
            const char* url, const char* artist, uint32_t duration) {
    
    if (db->songCount >= db->songCapacity) {
        db->songCapacity *= 2;
        Song* newSongs = new Song[db->songCapacity];
        memcpy(newSongs, db->songs, sizeof(Song) * db->songCount);
        delete[] db->songs;
        db->songs = newSongs;
    }
    
    Song* song = &db->songs[db->songCount++];
    song->id = db->nextSongId++;
    
    strncpy(song->title, title, sizeof(song->title) - 1);
    song->title[sizeof(song->title) - 1] = '\0';
    
    strncpy(song->artist, artist, sizeof(song->artist) - 1);
    song->artist[sizeof(song->artist) - 1] = '\0';
    
    strncpy(song->filename, filename, sizeof(song->filename) - 1);
    song->filename[sizeof(song->filename) - 1] = '\0';
    
    strncpy(song->url, url, sizeof(song->url) - 1);
    song->url[sizeof(song->url) - 1] = '\0';
    
    song->duration = duration;
    
    cout << "[INFO] Canción añadida: [" << song->id << "] " << title << " - " << artist << endl;
    
    return song->id;
}

// ===== OBTENER CANCIÓN POR ID =====
Song* getSongById(SongDatabase* db, uint32_t id) {
    for (int i = 0; i < db->songCount; i++) {
        if (db->songs[i].id == id) {
            return &db->songs[i];
        }
    }
    return nullptr;
}

// ===== INFORMACIÓN =====
int getSongCount(SongDatabase* db) {
    return db->songCount;
}

// ============================================
// ===== GUARDAR A ARCHIVO BINARIO (POSIX) =====
// ============================================

bool saveDatabase(SongDatabase* db, const char* filepath) {
    if (!db) {
        cerr << "[ERROR] Base de datos es NULL" << endl;
        return false;
    }
    
    // Abrir archivo con POSIX
    int fd = open(filepath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        cerr << "[ERROR] No se pudo abrir archivo para escritura: " << filepath << endl;
        return false;
    }
    
    cout << "[INFO] Guardando base de datos en: " << filepath << endl;
    
    // ===== CREAR HEADER CON VALORES ACTUALES DE LA DB =====
    DatabaseHeader header;
    
    memcpy(header.magic, "MUSI", 4);
    header.version = 1;
    header.numSongs = db->songCount;           // ← Valor de la DB en memoria
    header.numTitleWords = 0;                  // Por ahora 0 (TODO: cuando haya índices)
    header.numArtistWords = 0;                 // Por ahora 0 (TODO: cuando haya índices)
    header.offsetSongs = sizeof(DatabaseHeader);
    
    // Calcular offset del índice de títulos (después de las canciones)
    header.offsetTitleIndex = sizeof(DatabaseHeader) + (db->songCount * sizeof(Song));
    
    // Por ahora los demás offsets son 0 porque no hay datos
    header.offsetArtistIndex = 0;
    header.offsetTitleTrie = 0;
    header.offsetArtistTrie = 0;
    header.offsetTitleBKTree = 0;
    header.offsetArtistBKTree = 0;
    
    memset(header.reserved, 0, sizeof(header.reserved));
    
    // ===== ESCRIBIR HEADER =====
    ssize_t written = write(fd, &header, sizeof(DatabaseHeader));
    if (written != sizeof(DatabaseHeader)) {
        cerr << "[ERROR] No se pudo escribir el header" << endl;
        close(fd);
        return false;
    }
    
    cout << "[INFO] Header escrito (" << sizeof(DatabaseHeader) << " bytes)" << endl;
    cout << "       - numSongs: " << header.numSongs << endl;
    cout << "       - offsetSongs: " << header.offsetSongs << endl;
    
    // ===== ESCRIBIR CANCIONES =====
    if (db->songCount > 0) {
        size_t totalBytes = sizeof(Song) * db->songCount;
        written = write(fd, db->songs, totalBytes);
        if (written != (ssize_t)totalBytes) {
            cerr << "[ERROR] No se pudieron escribir todas las canciones" << endl;
            close(fd);
            return false;
        }
        
        cout << "[INFO] " << db->songCount << " canciones escritas (" << totalBytes << " bytes)" << endl;
    }
    
    // ===== TODO: ESCRIBIR ÍNDICES (después) =====
    
    // ===== CERRAR ARCHIVO =====
    close(fd);
    
    cout << "[INFO] Base de datos guardada exitosamente" << endl;
    cout << "       - Tamaño total: " 
         << (sizeof(DatabaseHeader) + (db->songCount * sizeof(Song))) 
         << " bytes" << endl;
    
    return true;
}

// ============================================
// ===== CARGAR DESDE ARCHIVO BINARIO (POSIX) =====
// ===== CON FALLBACK A CREAR NUEVA =====
// ============================================

SongDatabase* loadDatabase(const char* filepath) {
    // Intentar abrir archivo
    int fd = open(filepath, O_RDONLY);
    
    if (fd < 0) {
        // No existe el archivo, crear base de datos nueva
        cout << "[INFO] No se encontró archivo de base de datos: " << filepath << endl;
        cout << "[INFO] Creando base de datos nueva..." << endl;
        
        SongDatabase* db = createDatabase();
        
        // Guardar inmediatamente el archivo vacío
        cout << "[INFO] Guardando base de datos vacía en disco..." << endl;
        if (!saveDatabase(db, filepath)) {
            cerr << "[WARNING] No se pudo guardar la base de datos vacía" << endl;
        }
        
        return db;
    }
    
    // El archivo existe, intentar cargarlo
    cout << "[INFO] Cargando base de datos desde: " << filepath << endl;
    
    // ===== LEER HEADER =====
    DatabaseHeader header;
    ssize_t bytes_read = read(fd, &header, sizeof(DatabaseHeader));
    if (bytes_read != sizeof(DatabaseHeader)) {
        cerr << "[ERROR] No se pudo leer el header" << endl;
        close(fd);
        return nullptr;
    }
    
    // ===== VALIDAR MAGIC NUMBER =====
    if (memcmp(header.magic, "MUSI", 4) != 0) {
        cerr << "[ERROR] Archivo corrupto o formato incorrecto (magic: ";
        cerr.write(header.magic, 4);
        cerr << ")" << endl;
        close(fd);
        return nullptr;
    }
    
    cout << "[INFO] Magic number válido: MUSI" << endl;
    
    // ===== VALIDAR VERSIÓN =====
    if (header.version != 1) {
        cerr << "[ERROR] Versión no soportada: " << header.version << endl;
        close(fd);
        return nullptr;
    }
    
    cout << "[INFO] Versión: " << header.version << endl;
    cout << "[INFO] Canciones en archivo: " << header.numSongs << endl;
    
    // ===== CREAR BASE DE DATOS =====
    SongDatabase* db = new SongDatabase();
    
    // ===== CARGAR CANCIONES =====
    if (header.numSongs > 0) {
        db->songCapacity = header.numSongs;
        db->songs = new Song[db->songCapacity];
        
        // Posicionarse en offset de canciones
        if (lseek(fd, header.offsetSongs, SEEK_SET) < 0) {
            cerr << "[ERROR] No se pudo posicionar en offset de canciones" << endl;
            delete[] db->songs;
            delete db;
            close(fd);
            return nullptr;
        }
        
        // Leer canciones
        size_t totalBytes = sizeof(Song) * header.numSongs;
        bytes_read = read(fd, db->songs, totalBytes);
        if (bytes_read != (ssize_t)totalBytes) {
            cerr << "[ERROR] No se pudieron leer todas las canciones (" 
                 << bytes_read << "/" << totalBytes << " bytes)" << endl;
            delete[] db->songs;
            delete db;
            close(fd);
            return nullptr;
        }
        
        db->songCount = header.numSongs;
        
        // Calcular nextSongId (el máximo ID + 1)
        int maxId = 0;
        for (int i = 0; i < db->songCount; i++) {
            if ((int)db->songs[i].id > maxId) {
                maxId = db->songs[i].id;
            }
        }
        db->nextSongId = maxId + 1;
        
        cout << "[INFO] " << header.numSongs << " canciones cargadas" << endl;
        cout << "[INFO] Próximo ID: " << db->nextSongId << endl;
        
    } else {
        // Archivo vacío (0 canciones)
        db->songCapacity = 100;
        db->songs = new Song[db->songCapacity];
        db->songCount = 0;
        db->nextSongId = 1;
        
        cout << "[INFO] Base de datos vacía (0 canciones)" << endl;
    }
    
    // ===== TODO: CARGAR ÍNDICES (después) =====
    
    close(fd);
    
    cout << "[INFO] Base de datos cargada exitosamente" << endl;
    
    return db;
}