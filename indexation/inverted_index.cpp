#include "inverted_index.hpp"
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cctype>
#include <unordered_set>

using namespace std;

// ===== STOP WORDS CON HASH SET (O(1) lookup) =====
static unordered_set<string> STOP_WORDS = {
    // Inglés - Artículos
    "a", "an", "the",
    
    // Inglés - Preposiciones
    "in", "on", "at", "to", "for", "with", "from", "of", "by", "about",
    "into", "through", "during", "before", "after", "above", "below",
    
    // Inglés - Conjunciones
    "and", "or", "but", "nor", "yet", "so",
    
    // Inglés - Pronombres
    "i", "you", "he", "she", "it", "we", "they",
    "me", "him", "her", "us", "them",
    "my", "your", "his", "her", "its", "our", "their",
    
    // Inglés - Verbos auxiliares
    "is", "am", "are", "was", "were", "be", "been", "being",
    "have", "has", "had", "do", "does", "did",
    
    // Inglés - Otros comunes
    "as", "so", "if", "that", "this", "these", "those",
    "what", "which", "who", "when", "where", "why", "how",
    
    // Español - Artículos
    "el", "la", "los", "las", "un", "una", "unos", "unas",
    
    // Español - Preposiciones
    "de", "del", "en", "a", "al", "con", "por", "para", "sin", "sobre",
    "entre", "desde", "hasta", "hacia", "bajo", "tras",
    
    // Español - Conjunciones
    "y", "e", "o", "u", "pero", "sino", "ni",
    
    // Español - Pronombres
    "yo", "tu", "tú", "él", "ella", "nosotros", "nosotras", 
    "vosotros", "vosotras", "ellos", "ellas",
    "me", "te", "se", "le", "lo", "la", "nos", "os", "les",
    "mi", "mis", "tu", "tus", "su", "sus", "nuestro", "nuestra",
    
    // Español - Verbos auxiliares
    "es", "soy", "eres", "somos", "sois", "son",
    "era", "eras", "éramos", "erais", "eran",
    "he", "has", "ha", "hemos", "habéis", "han",
    
    // Español - Otros comunes
    "que", "como", "cual", "cuál", "cuando", "donde", "dónde",
    "quien", "quién", "porque", "si", "esto", "eso", "aquello",
    
    // Japonés - Partículas comunes
    "の", "は", "が", "を", "に", "へ", "と", "で", "から", "まで",
    "や", "も", "か", "ね", "よ", "わ", "な",
    
    // Coreano - Partículas comunes
    "은", "는", "이", "가", "을", "를", "의", "에", "에서", "로", "으로",
    "와", "과", "도", "만", "까지", "부터"
};

// ===== VERIFICAR SI ES STOP WORD (O(1)) =====
bool isStopWord(const char* word) {
    return STOP_WORDS.count(string(word)) > 0;
}

// ===== DETECTAR SI ES CARÁCTER UTF-8 MULTIBYTE =====
static inline bool isUTF8Start(unsigned char c) {
    // UTF-8 primer byte: 11xxxxxx (0xC0-0xFD)
    return (c & 0xC0) == 0xC0;
}

static inline bool isUTF8Continuation(unsigned char c) {
    // UTF-8 byte continuación: 10xxxxxx (0x80-0xBF)
    return (c & 0xC0) == 0x80;
}

// ===== OBTENER LONGITUD DE CARÁCTER UTF-8 =====
static int getUTF8CharLength(unsigned char c) {
    if ((c & 0x80) == 0x00) return 1;      // 0xxxxxxx (ASCII)
    if ((c & 0xE0) == 0xC0) return 2;      // 110xxxxx (2 bytes)
    if ((c & 0xF0) == 0xE0) return 3;      // 1110xxxx (3 bytes) ← Japonés, Coreano
    if ((c & 0xF8) == 0xF0) return 4;      // 11110xxx (4 bytes) ← Emoji
    return 1;  // Inválido, tratar como 1 byte
}

// ===== VERIFICAR SI ES CARÁCTER VÁLIDO PARA PALABRA =====
static bool isValidWordChar(const unsigned char* ptr, int* charLen) {
    unsigned char c = *ptr;
    
    // ASCII alfanumérico
    if (isalnum(c)) {
        *charLen = 1;
        return true;
    }
    
    // UTF-8 multibyte (Japonés: Hiragana, Katakana, Kanji / Coreano: Hangul)
    if (isUTF8Start(c)) {
        *charLen = getUTF8CharLength(c);
        
        // Verificar que sea un carácter completo válido
        for (int i = 1; i < *charLen; i++) {
            if (!isUTF8Continuation(ptr[i])) {
                *charLen = 1;
                return false;  // UTF-8 malformado
            }
        }
        
        return true;  // Carácter UTF-8 válido
    }
    
    *charLen = 1;
    return false;  // No es válido para palabra
}

// ===== EXTRAER PALABRAS DE UN TEXTO (CON SOPORTE UTF-8) =====
void extractWords(const char* text, char words[][64], int* wordCount, int maxWords) {    
    if (!text || text[0] == '\0') {
        return;
    }
    
    char currentWord[64] = {0};
    int wordByteLen = 0;
    
    const unsigned char* ptr = (const unsigned char*)text;
    
    while (*ptr && *wordCount < maxWords) {
        int charLen;
        
        if (isValidWordChar(ptr, &charLen)) {
            // Carácter válido (ASCII o UTF-8)
            
            // Verificar que quepa en el buffer
            if (wordByteLen + charLen < 63) {
                // Copiar carácter (1-4 bytes)
                for (int i = 0; i < charLen; i++) {
                    // Convertir ASCII a minúsculas, dejar UTF-8 sin cambios
                    if (charLen == 1 && isalpha(*ptr)) {
                        currentWord[wordByteLen++] = tolower(*ptr);
                    } else {
                        currentWord[wordByteLen++] = *ptr;
                    }
                    ptr++;
                }
            } else {
                ptr += charLen;
            }
        } else {
            // Separador (espacio, puntuación, etc.)
            
            if (wordByteLen > 0) {
                // Fin de palabra
                currentWord[wordByteLen] = '\0';
                
                // Contar caracteres (no bytes) para política de stop words
                int charCount = 0;
                const unsigned char* temp = (const unsigned char*)currentWord;
                while (*temp) {
                    int len = getUTF8CharLength(*temp);
                    charCount++;
                    temp += len;
                }
                
                // Política: añadir si NO es stop word, o si es muy corta (≤2 caracteres)
                if (!isStopWord(currentWord) || charCount <= 2) {
                    memcpy(words[*wordCount], currentWord, wordByteLen + 1);
                    (*wordCount)++;
                }
                
                // Reset
                wordByteLen = 0;
                memset(currentWord, 0, sizeof(currentWord));
            }
            
            ptr++;
        }
    }
    
    // Procesar última palabra si existe
    if (wordByteLen > 0 && *wordCount < maxWords) {
        currentWord[wordByteLen] = '\0';
        
        int charCount = 0;
        const unsigned char* temp = (const unsigned char*)currentWord;
        while (*temp) {
            int len = getUTF8CharLength(*temp);
            charCount++;
            temp += len;
        }
        
        if (!isStopWord(currentWord) || charCount <= 2) {
            memcpy(words[*wordCount], currentWord, wordByteLen + 1);
            (*wordCount)++;
        }
    }
}

// ===== CREAR ÍNDICE INVERTIDO VACÍO =====
InvertedIndex* createInvertedIndex() {
    InvertedIndex* index = new InvertedIndex();
    
    index->capacity = 100;
    index->entries = new WordEntry[index->capacity]();
    index->count = 0;
    
    // Inicializar entradas
    for (int i = 0; i < index->capacity; i++) {
        index->entries[i].word[0] = '\0';
        index->entries[i].songIds = nullptr;
        index->entries[i].count = 0;
        index->entries[i].capacity = 0;
    }
    
    return index;
}

// ===== LIBERAR ÍNDICE =====
void freeInvertedIndex(InvertedIndex* index) {
    if (!index) return;
    
    // Liberar arrays de songIds
    for (int i = 0; i < index->count; i++) {
        delete[] index->entries[i].songIds;
    }
    
    delete[] index->entries;
    delete index;
}

// ===== BUSCAR PALABRA EN EL ÍNDICE =====
WordEntry* findWord(InvertedIndex* index, const char* word) {
    if (!index || !word) return nullptr;
    
    for (int i = 0; i < index->count; i++) {
        if (strcmp(index->entries[i].word, word) == 0) {
            return &index->entries[i];
        }
    }
    
    return nullptr;
}

// ===== AÑADIR PALABRA + SONG ID AL ÍNDICE =====
void insertWordIndex(InvertedIndex* index, string wordString, int songId) {
    const char* word = wordString.c_str();
    if (!index || !word || word[0] == '\0') return;
    
    // Buscar si la palabra ya existe
    WordEntry* entry = findWord(index, word);
    
    if (!entry) {
        // ===== PALABRA NUEVA =====
        
        // Expandir array de entradas si es necesario
        if (index->count >= index->capacity) {
            int newCapacity = index->capacity * 2;
            WordEntry* newEntries = new WordEntry[newCapacity]();
            
            // Copiar entradas existentes
            memcpy(newEntries, index->entries, sizeof(WordEntry) * index->count);
            
            // Inicializar nuevas entradas
            for (int i = index->count; i < newCapacity; i++) {
                newEntries[i].word[0] = '\0';
                newEntries[i].songIds = nullptr;
                newEntries[i].count = 0;
                newEntries[i].capacity = 0;
            }
            
            delete[] index->entries;
            index->entries = newEntries;
            index->capacity = newCapacity;
        }
        
        // Crear nueva entrada
        entry = &index->entries[index->count];
        index->count++;
        
        // Copiar palabra
        size_t wordLen = strlen(word);
        memcpy(entry->word, word, min(wordLen + 1, (size_t)64));
        entry->word[63] = '\0';
        
        // Inicializar array de songIds
        entry->capacity = 4;
        entry->songIds = new int[entry->capacity];
        entry->count = 0;
    }
    
    // ===== VERIFICAR DUPLICADOS =====
    for (int i = 0; i < entry->count; i++) {
        if (entry->songIds[i] == songId) {
            return;  // Ya existe
        }
    }
    
    // ===== AÑADIR SONG ID =====
    
    // Expandir array de songIds si es necesario
    if (entry->count >= entry->capacity) {
        int newCapacity = entry->capacity * 2;
        int* newSongIds = new int[newCapacity];
        
        // Copiar IDs existentes
        memcpy(newSongIds, entry->songIds, sizeof(int) * entry->count);
        
        delete[] entry->songIds;
        entry->songIds = newSongIds;
        entry->capacity = newCapacity;
    }
    
    // Añadir nuevo songId
    entry->songIds[entry->count] = songId;
    entry->count++;
}