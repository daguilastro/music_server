#pragma once

#include <cstdint>
#include <iostream>
#include "trie.hpp"
#include "bktree.hpp"
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
    uint32_t duration;  // segundos
};
#pragma pack()

// ===== HEADER DEL ARCHIVO BINARIO =====
#pragma pack(1)
struct DatabaseHeader {
    char magic[4];              // "MUSI"
    uint32_t version;           // 1
    uint32_t numSongs;          // Número de canciones
    uint32_t numTitleWords;     // (Por ahora 0)
    uint32_t numArtistWords;    // (Por ahora 0)
    
    uint64_t offsetSongs;           // Offset a sección de canciones
    uint64_t offsetTitleIndex;      // (Por ahora 0)
    uint64_t offsetArtistIndex;     // (Por ahora 0)
    uint64_t offsetTitleTrie;       // (Por ahora 0)
    uint64_t offsetArtistTrie;      // (Por ahora 0)
    uint64_t offsetTitleBKTree;     // (Por ahora 0)
    uint64_t offsetArtistBKTree;    // (Por ahora 0)
    
    char reserved[32];          // Reservado
};
#pragma pack()

// ===== ESTRUCTURA PRINCIPAL =====
struct SongDatabase {
    // Canciones
    Song* songs;
    int songCount;
    int songCapacity;
    int nextSongId;
    
    // Índice de títulos
    TrieNode* titleTrieRoot;
    BKNode* titleBKRoot;
    InvertedIndex* titleIndex;
    
    // Índice de artistas
    TrieNode* artistTrieRoot;
    BKNode* artistBKRoot;
    InvertedIndex* artistIndex;
};

// ===== FUNCIONES PÚBLICAS =====

// Crear/liberar
SongDatabase* createDatabase();
void freeDatabase(SongDatabase* db);

// Añadir canción
int addSong(SongDatabase* db, const char* title, const char* filename,
            const char* url, const char* artist, uint32_t duration);

// Verificar duplicados
bool isDuplicateURL(SongDatabase* db, const char* url);

// Obtener canción por ID
Song* getSongById(SongDatabase* db, int id);

// Información
int getSongCount(SongDatabase* db);

bool saveDatabase(SongDatabase* db, const char* filepath);

SongDatabase* loadDatabase(const char* filepath);