#pragma once

struct BKNode {
    char word[64];
    int* songIds;
    int songIdsCount;
    int songIdsCapacity;
    BKNode** children;
    int* childDistances;
    int childCount;
    int childCapacity;
};

// ===== FUNCIONES =====

BKNode* createBKNode(const char* word);
void freeBKNode(BKNode* node);

void insertIntoBKTree(BKNode** root, const char* word, int songId);

int levenshteinDistance(const char* s1, const char* s2);