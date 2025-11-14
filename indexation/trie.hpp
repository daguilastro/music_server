#pragma once

#include <unordered_map>
#include <string>
#include <unordered_set>
#include <vector>

using namespace std;

struct TrieNode {
    unordered_map<char, TrieNode*> children;
    bool isEndOfWord;
    int* songIds;
    int songIdCount;
    int songIdCapacity;
};

struct Trie {
    TrieNode* root;
};


Trie* createTrie();
void insertWordTrie(Trie* trie, string word, int songId);
void searchPrefix(Trie* trie, string prefix, unordered_set<int>& results);
void freeTrie(Trie* trie);