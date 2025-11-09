#pragma once

#include <cstdint>
#include "inverted_index.hpp"


using namespace std;

// ===== ESTRUCTURA DE CANCIÓN =====
#pragma pack(1)
struct Song {
    uint32_t id;
    char title[256];
    char artist[128];
    char filename[256];
    char url[512];
    uint32_t duration;
};
#pragma pack()

// ===== HEADER DEL ARCHIVO BINARIO =====
#pragma pack(1)
struct DatabaseHeader {
    char magic[4];
    uint32_t version;
    uint32_t numSongs;
    uint32_t numTitleWords;
    uint32_t numArtistWords;
    
    uint64_t offsetSongs;
    uint64_t offsetTitleIndex;
    uint64_t offsetArtistIndex;
    uint64_t offsetTitleTrie;
    uint64_t offsetArtistTrie;
    uint64_t offsetTitleBKTree;
    uint64_t offsetArtistBKTree;
    
    char reserved[32];
};
#pragma pack()

// ===== ESTRUCTURA PRINCIPAL =====
struct SongDatabase {
    Song* songs;
    int songCount;
    int songCapacity;
    int nextSongId;
    
    // Índices de títulos
    InvertedIndex* titleIndex;
    
    // Índices de artistas
    InvertedIndex* artistIndex;
};

// ===== RESULTADO DE BÚSQUEDA =====
struct SearchResult {
    int* songIds;
    int count;
    int capacity;
};

extern SongDatabase* globalDB;

// ===== FUNCIONES PÚBLICAS =====

// Crear/liberar en memoria
SongDatabase* createDatabase();
void freeDatabase(SongDatabase* db);

// Añadir canción
int addSong(SongDatabase* db, Song song);

// Verificar duplicados
bool isDuplicateURL(SongDatabase* db, const char* url);

// Obtener canción por ID
Song* getSongById(SongDatabase* db, uint32_t id);
long getSongOffsetInFile(SongDatabase* db, uint32_t id);

// Información
int getSongCount(SongDatabase* db);

// ===== BÚSQUEDA =====
SearchResult searchSongs(SongDatabase* db, const char* query, bool searchInTitle, bool searchInArtist);
void freeSearchResult(SearchResult* result);

// ===== PERSISTENCIA =====
bool saveDatabase(SongDatabase* db, const char* filepath);
SongDatabase* loadDatabase(const char* filepath);
void indexSong(Song song);