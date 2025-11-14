#pragma once

#include <cstdint>
#include "bktree.hpp"
#include <vector>
#include "inverted_index.hpp"
#include "trie.hpp"


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
    uint64_t offsetSongs;
    char reserved[64];
};
#pragma pack()

// ===== ESTRUCTURA PRINCIPAL =====
struct SongDatabase {
    Song* songs;
    int songCount;
    int songCapacity;
    int nextSongId;
    
    // Índices de títulos
    InvertedIndex* invertedIndex;

    // Árbol BK
    BKNode* bkTree;

    Trie* trie;
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

// ===== BÚSQUEDA =====
SearchResult searchSongs(SongDatabase* db, const char* query);
void freeSearchResult(SearchResult* result);

// ===== PERSISTENCIA =====
bool saveDatabase(SongDatabase* db, const char* filepath);
SongDatabase* loadDatabase(const char* filepath);
void indexSong(Song song);

void insertWordDatabase(SongDatabase* db, string word, uint32_t id);