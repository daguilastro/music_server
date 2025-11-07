#pragma once

#include <cstdint>

// ===== ENTRADA DEL ÍNDICE INVERTIDO =====
struct WordEntry {
    char word[64];
    int* songIds;
    int count;
    int capacity;
};

struct InvertedIndex {
    WordEntry* entries;
    int count;
    int capacity;
};

// ===== FUNCIONES =====

InvertedIndex* createInvertedIndex();
void freeInvertedIndex(InvertedIndex* index);

void addToIndex(InvertedIndex* index, const char* word, int songId);
WordEntry* findWord(InvertedIndex* index, const char* word);