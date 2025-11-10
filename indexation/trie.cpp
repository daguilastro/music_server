#include "trie.hpp"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <unordered_set>

// ===== CREAR NODO =====
TrieNode* createTrieNode() {
    TrieNode* node = new TrieNode();
    node->isEndOfWord = false;
    node->songIdCapacity = 4;
    node->songIds = new int[4];
    node->songIdCount = 0;
    return node;
}

// ===== CREAR TRIE =====
Trie* createTrie() {
    Trie* trie = new Trie();
    trie->root = createTrieNode();
    return trie;
}

// ===== INSERTAR PALABRA =====
void insertWord(Trie* trie, string word, int songId) {
    if (!trie || word.empty()) return;
    
    TrieNode* node = trie->root;
    
    // Convertir a minúsculas y recorrer
    for (char c : word) {
        c = tolower(c);
        
        // Si no existe hijo con ese carácter, crearlo
        if (node->children.find(c) == node->children.end()) {
            node->children[c] = createTrieNode();
        }
        
        // Bajar al hijo
        node = node->children[c];
    }
    
    // Marcar fin de palabra
    node->isEndOfWord = true;
    
    // Verificar duplicados
    for (int i = 0; i < node->songIdCount; i++) {
        if (node->songIds[i] == songId) {
            return;  // Ya existe
        }
    }
    
    // Expandir si es necesario
    if (node->songIdCount >= node->songIdCapacity) {
        int newCapacity = node->songIdCapacity * 2;
        int* newIds = new int[newCapacity];
        memcpy(newIds, node->songIds, node->songIdCount * sizeof(int));
        delete[] node->songIds;
        node->songIds = newIds;
        node->songIdCapacity = newCapacity;
    }
    
    // Añadir songId
    node->songIds[node->songIdCount++] = songId;
}


// ===== BÚSQUEDA RECURSIVA DE TODOS LOS HIJOS =====
void collectAllSongIds(TrieNode* node, unordered_set<int>& results) {
    if (!node) return;
    
    // Si es fin de palabra, añadir IDs
    if (node->isEndOfWord) {
        for (int i = 0; i < node->songIdCount; i++) {
            results.insert(node->songIds[i]);
        }
    }
    
    // Recorrer todos los hijos
    for (auto& pair : node->children) {
        collectAllSongIds(pair.second, results);
    }
}

// ===== BUSCAR POR PREFIJO (AUTOCOMPLETAR) =====
void searchPrefix(Trie* trie, string prefix, unordered_set<int>& results) {
    if (!trie || prefix.empty()) return;
    
    TrieNode* node = trie->root;
    
    // Buscar el nodo del prefijo
    for (char c : prefix) {
        c = tolower(c);
        
        if (node->children.find(c) == node->children.end()) {
            return;  // Prefijo no existe
        }
        
        node = node->children[c];
    }
    
    // Recolectar todos los IDs desde este nodo hacia abajo
    collectAllSongIds(node, results);
}

// ===== LIBERAR NODO RECURSIVAMENTE =====
void freeTrieNode(TrieNode* node) {
    if (!node) return;
    
    // Liberar todos los hijos
    for (auto& pair : node->children) {
        freeTrieNode(pair.second);
    }
    
    // Liberar array de songIds
    delete[] node->songIds;
    
    // Liberar nodo
    delete node;
}

// ===== LIBERAR TRIE =====
void freeTrie(Trie* trie) {
    if (!trie) return;
    freeTrieNode(trie->root);
    delete trie;
}