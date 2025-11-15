#pragma once

#include <string>

using std::string;

// ===== ENTRADA DEL ÍNDICE INVERTIDO =====
#pragma pack(1)
struct WordEntry {
    char word[64];      // La palabra indexada (ej: "bohemian")
    int* songIds;       // Array dinámico de IDs de canciones
    int count;          // Cantidad ACTUAL de IDs en el array
    int capacity;       // Capacidad MÁXIMA del array (memoria alocada)
    int documentFrequency = 0;  // Frecuencia con la que aparece
    double inverseDocumentFrequency = 0.0;    // lo que dice su nombre
};
#pragma pack()

struct InvertedIndex {
    WordEntry* entries;  // Array dinámico de palabras
    int count;           // Cantidad actual de palabras
    int capacity;        // Capacidad máxima del array
};

// ===== FUNCIONES =====

InvertedIndex* createInvertedIndex();
void freeInvertedIndex(InvertedIndex* index);

void insertWordIndex(InvertedIndex* index, string word, int songId);
WordEntry* findWord(InvertedIndex* index, const char* word);

// Helper para palabras
void extractWords(const char* text, char words[][64], int* wordCount, int maxWords);
bool isStopWord(const char* word);
