#pragma once

struct TrieNode {
    TrieNode* children[26];  // solo a-z
    int* songIds;
    int songIdsCount;
    int songIdsCapacity;
    bool isEndOfWord;
};

// ===== FUNCIONES =====

TrieNode* createTrieNode();
void freeTrieNode(TrieNode* node);

void insertIntoTrie(TrieNode* root, const char* word, int songId);
void searchTrieByPrefix(TrieNode* root, const char* prefix, int* results, 
                       int* resultCount, int maxResults);
