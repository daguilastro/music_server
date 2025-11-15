#pragma once

#include <string>
#include <unordered_set>

using std::string;


struct BKNode;

struct BKChild{
    int distance;
    BKNode* node;
};

struct BKNode{
    char word[64];
    int* songIds;
    int songIdCount;
    int songIdCapacity;

    BKChild* children;
    int childrenCount;
    int childrenCapacity;
};

void insertWordBKTree(BKNode*& root, string word, int songId);


// ===== FUNCIONES =====

BKNode* createBKNode(string word, int songId);

void freeBKNode(BKNode* node);

void recursiveBKInsert(BKNode* node, string word, int songId);

int levenshteinDistance(string word1, string word2);

void recursiveBKSearch(BKNode* node, string word, int tolerance, std::unordered_set<int>& idsFound);