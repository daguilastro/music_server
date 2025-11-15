#include "database.hpp"
#include "bktree.hpp"
#include "inverted_index.hpp"
#include "trie.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <unordered_set>

using namespace std;

// ===== CREAR BASE DE DATOS VACÍA =====
SongDatabase *createDatabase() {
	SongDatabase *db = new SongDatabase();

	db->songCapacity = 100;
	db->songs = new Song[db->songCapacity];
	db->songCount = 0;
	db->nextSongId = 1;

	// ===== INICIALIZAR ÍNDICES =====
	db->invertedIndex = createInvertedIndex();
	db->trie = createTrie();

	cout << "[INFO] Base de datos creada (vacía) en RAM" << endl;

	return db;
}

// ===== LIBERAR MEMORIA =====
void freeDatabase(SongDatabase *db) {
	if (!db) return;
	freeBKNode(db->bkTree);
	freeInvertedIndex(db->invertedIndex);
	freeTrie(db->trie);
	delete[] db->songs;
	delete db;
}

// ===== VERIFICAR URL DUPLICADA =====
bool isDuplicateURL(SongDatabase *db, const char *url) {
	for (int i = 0; i < db->songCount; i++) {
		cout << "song in db: " << db->songs[i].url << endl;
		cout << "song to compare: " << url << endl;
		if (strcmp(db->songs[i].url, url) == 0) {
			return true;
		}
	}
	return false;
}

long getSongOffsetInFile(SongDatabase *db, uint32_t id) {
	// Encontrar índice de la canción
	int index = -1;
	for (int i = 0; i < db->songCount; i++) {
		if (db->songs[i].id == id) {
			index = i;
			break;
		}
	}

	if (index < 0) {
		return -1; // No encontrada
	}

	// Calcular offset: header + (índice × tamaño de Song)
	long offset = sizeof(DatabaseHeader) + (index * sizeof(Song));

	return offset;
}

void insertWordDatabase(SongDatabase *db, string word, uint32_t id) {
	insertWordBKTree(db->bkTree, word, id);
	insertWordTrie(db->trie, word, id);
	insertWordIndex(db->invertedIndex, word, id);
}

// ===== AÑADIR CANCIÓN =====
int addSong(SongDatabase *db, Song songSent) {
	if (db->songCount >= db->songCapacity) {
		db->songCapacity *= 2;
		Song *newSongs = new Song[db->songCapacity];
		memcpy(newSongs, db->songs, sizeof(Song) * db->songCount);
		delete[] db->songs;
		db->songs = newSongs;
	}

	Song *songSave = &db->songs[db->songCount++];
	memcpy(songSave->artist, songSent.artist, sizeof(songSent.artist));
	memcpy(songSave->title, songSent.title, sizeof(songSent.title));
	memcpy(songSave->filename, songSent.filename, sizeof(songSent.filename));
	memcpy(songSave->url, songSent.url, sizeof(songSent.url));
	songSave->duration = songSent.duration;
	songSave->id = db->nextSongId++;

	// ===== INDEXAR TÍTULO =====
	cout << "[INDEX] Indexando título: \"" << songSave->title << "\"..." << endl;

	char words[100][64];
	int titleWordCount = 0;
	int artistWordCount = 0;
	extractWords(songSave->title, words, &titleWordCount, 50);
	extractWords(songSave->artist, words + titleWordCount, &artistWordCount, 50);

	cout << "[INDEX] Palabras extraídas del título y artista: " << titleWordCount + artistWordCount << endl;

	for (int i = 0; i < titleWordCount + artistWordCount; i++) {
		cout << "[INDEX]   - \"" << words[i] << "\"" << endl;
		insertWordDatabase(db, words[i], songSave->id);
	}
	cout << "[INFO] Canción añadida e indexada: [" << songSave->id << "] " << songSave->title << " - " << songSave->artist << endl;

	return songSave->id;
}

// ===== OBTENER CANCIÓN POR ID =====
Song *getSongById(SongDatabase *db, uint32_t id) {
	for (int i = 0; i < db->songCount; i++) {
		if (db->songs[i].id == id) {
			return &db->songs[i];
		}
	}
	return nullptr;
}

// ============================================
// ===== GUARDAR A ARCHIVO BINARIO (POSIX) =====
// ============================================

bool saveDatabase(SongDatabase *db, const char *filepath) {
	// Crear header
	DatabaseHeader header;
	memset(&header, 0, sizeof(DatabaseHeader));

	memcpy(header.magic, "MUSI", 4);
	header.version = 1;
	header.numSongs = db->songCount;
	header.offsetSongs = sizeof(DatabaseHeader);

	// Abrir archivo
	int fd = open(filepath, O_WRONLY | O_CREAT | O_TRUNC, 0644);

	// Escribir header
	write(fd, &header, sizeof(DatabaseHeader));

	// Escribir SOLO canciones
	write(fd, db->songs, sizeof(Song) * db->songCount);

	close(fd);

	// NO guardamos índices ✅

	return true;
}

// ============================================
// ===== CARGAR DESDE ARCHIVO BINARIO (POSIX) =====
// ===== CON FALLBACK A CREAR NUEVA =====
// ============================================

SongDatabase *loadDatabase(const char *filepath) {
	cout << "[DEBUG] ===== INICIO loadDatabase() =====" << endl;
	cout << "[DEBUG] Archivo: " << filepath << endl;

	int fd = open(filepath, O_RDONLY);

	if (fd < 0) {
		cout << "[INFO] No se encontró archivo de base de datos: " << filepath << endl;
		cout << "[INFO] Creando base de datos nueva..." << endl;

		SongDatabase *db = createDatabase();

		if (!db) {
			cerr << "[ERROR] No se pudo crear base de datos" << endl;
			return nullptr;
		}

		cout << "[INFO] Guardando base de datos vacía en disco..." << endl;
		if (!saveDatabase(db, filepath)) {
			cerr << "[WARNING] No se pudo guardar la base de datos vacía" << endl;
		}

		cout << "[DEBUG] ===== FIN loadDatabase() (nueva) =====" << endl;
		return db;
	}

	cout << "[INFO] Cargando base de datos desde: " << filepath << endl;

	// ===== LEER HEADER =====
	cout << "[DEBUG] Leyendo header..." << endl;
	DatabaseHeader header;
	ssize_t bytes_read = read(fd, &header, sizeof(DatabaseHeader));

	cout << "[DEBUG] Bytes leídos: " << bytes_read << " de " << sizeof(DatabaseHeader) << endl;

	if (bytes_read != sizeof(DatabaseHeader)) {
		cerr << "[ERROR] No se pudo leer el header (archivo corrupto)" << endl;
		close(fd);

		cout << "[INFO] Eliminando archivo corrupto y creando nuevo..." << endl;
		unlink(filepath);

		SongDatabase *db = createDatabase();
		if (db) {
			saveDatabase(db, filepath);
		}
		return db;
	}

	// ===== VALIDAR MAGIC NUMBER =====
	cout << "[DEBUG] Magic number: " << header.magic[0] << header.magic[1]
		 << header.magic[2] << header.magic[3] << endl;

	if (memcmp(header.magic, "MUSI", 4) != 0) {
		cerr << "[ERROR] Archivo corrupto (magic number incorrecto)" << endl;
		close(fd);
		unlink(filepath);

		SongDatabase *db = createDatabase();
		if (db) {
			saveDatabase(db, filepath);
		}
		return db;
	}

	cout << "[INFO] Versión: " << header.version << endl;
	cout << "[INFO] Canciones: " << header.numSongs << endl;

	// ===== CREAR BASE DE DATOS =====
	SongDatabase *db = new SongDatabase();

	// ===== CARGAR CANCIONES =====
	if (header.numSongs > 0) {
		cout << "[DEBUG] Cargando " << header.numSongs << " canciones..." << endl;
		db->songCapacity = header.numSongs;
		db->songs = new Song[db->songCapacity];

		lseek(fd, header.offsetSongs, SEEK_SET);

		size_t totalBytes = sizeof(Song) * header.numSongs;
		bytes_read = read(fd, db->songs, totalBytes);

		cout << "[DEBUG] Bytes leídos: " << bytes_read << " de " << totalBytes << endl;

		if (bytes_read != (ssize_t)totalBytes) {
			cerr << "[ERROR] No se pudieron leer todas las canciones" << endl;
			delete[] db->songs;
			delete db;
			close(fd);
			return nullptr;
		}

		db->songCount = header.numSongs;

		uint32_t maxId = 0;
		for (int i = 0; i < db->songCount; i++) {
			cout << "[DEBUG]   - Canción " << i << ": [" << db->songs[i].id << "] "
				 << db->songs[i].title << endl;
			if (db->songs[i].id > maxId) {
				maxId = db->songs[i].id;
			}
		}
		db->nextSongId = maxId + 1;

	} else {
		db->songCapacity = 100;
		db->songs = new Song[db->songCapacity];
		db->songCount = 0;
		db->nextSongId = 1;
	}

	close(fd);

	// ===== CREAR ÍNDICES =====
	cout << "[DEBUG] Creando índices..." << endl;
	db->invertedIndex = createInvertedIndex();
	db->bkTree = nullptr;
	db->trie = createTrie();

	cout << "[DEBUG] Índices creados (vacíos)" << endl;

	for (int i = 0; i < db->songCount; i++) {
		Song *song = &db->songs[i];

		char words[100][64];
		int titleWordCount = 0;
		int artistWordCount = 0;
		extractWords(song->title, words, &titleWordCount, 50);
		extractWords(song->artist, words + titleWordCount, &artistWordCount, 50);

		cout << "[INDEX] Palabras extraídas del título: " << titleWordCount << endl;

		for (int i = 0; i < titleWordCount + artistWordCount; i++) {
			cout << "[INDEX]   - \"" << words[i] << "\"" << endl;
			insertWordDatabase(db, words[i], song->id);
		}
	}
	cout << "[INFO] Índices reconstruidos:" << endl;
	cout << "  - Palabras en index: " << db->invertedIndex->count << endl;
	return db;
}

// ============================================
// ===== BÚSQUEDA DE CANCIONES =====
// ============================================

// mejorar esta función un montón
SearchResult searchSongs(SongDatabase *db, const char *query) {
	SearchResult result;
	result.capacity = 100;
	result.songIds = new int[result.capacity];
	result.count = 0;

	if (!db || !query || query[0] == '\0') {
		cerr << "[ERROR] searchSongs: parámetros inválidos" << endl;
		return result;
	}

	// ===== VERIFICACIÓN CRÍTICA =====
	if (!db->invertedIndex) {
		cerr << "[ERROR] searchSongs: invertedIndex es NULL" << endl;
		return result;
	}

	cout << "[SEARCH] Buscando: \"" << query << "\"" << endl;
	// ===== DEBUG: ESTADO DE LOS ÍNDICES =====
	cout << "[DEBUG] invertedIndex->count = " << db->invertedIndex->count << endl;

	// Extraer palabras de la consulta
	char queryWords[50][64];
	int queryWordCount = 0;
	extractWords(query, queryWords, &queryWordCount, 50);

	cout << "[SEARCH] Palabras extraídas: " << queryWordCount << endl;
	for (int i = 0; i < queryWordCount; i++) {
		cout << "         - \"" << queryWords[i] << "\"" << endl;
	}

	// Set para evitar duplicados
	unordered_set<int> foundIds;
	unordered_set<int> trieIds;
	unordered_set<int> bkTreeIds;

	// ===== BUSCAR EN INDEX (exact match), TRIE (prefix) y BK-TREE (fuzzy) =====
	for (int i = 0; i < queryWordCount; i++) {
		const string q = queryWords[i];

		cout << "[DEBUG] Buscando \"" << q << "\" en inverted index..." << endl;
		WordEntry *entry = findWord(db->invertedIndex, q.c_str());
		if (entry) {
			cout << "[SEARCH] \"" << q << "\" encontrada en index: " << entry->count << " canciones" << endl;
			for (int j = 0; j < entry->count; j++) {
				foundIds.insert(entry->songIds[j]);
			}
		} else {
			cout << "[DEBUG] \"" << q << "\" NO encontrada en inverted index" << endl;
		}

		// Buscar por prefijo en el trie (autocompletar)
		if (db->trie) {
			cout << "[DEBUG] Buscando prefijos \"" << q << "\" en trie..." << endl;
			searchPrefix(db->trie, q, trieIds);
		} else {
			cout << "[DEBUG] trie es NULL, omitiendo búsqueda por prefijo" << endl;
		}

		// Búsqueda fuzzy en BK-tree (tolerancia = 2)
		if (db->bkTree != nullptr) {
			cout << "[DEBUG] Buscando fuzzy \"" << q << "\" en BK-tree..." << endl;
			recursiveBKSearch(db->bkTree, q, 2, bkTreeIds);
		} else {
			cout << "[DEBUG] bkTree es NULL, omitiendo búsqueda fuzzy" << endl;
		}
	}

	// Fusionar resultados de trie y bk-tree en foundIds
	if (!bkTreeIds.empty()) {
		cout << "  ✅ Fuzzy encontró " << bkTreeIds.size() << " canciones" << endl;
		for (int id : bkTreeIds) foundIds.insert(id);
	} else {
		cout << "  ❌ Fuzzy no encontró resultados" << endl;
	}

	if (!trieIds.empty()) {
		cout << "  ✅ Prefix encontró " << trieIds.size() << " canciones" << endl;
		for (int id : trieIds) foundIds.insert(id);
	} else {
		cout << "  ❌ Prefix no encontró resultados" << endl;
	}
	// ===== CONVERTIR SET A ARRAY =====
	for (int id : foundIds) {
		if (result.count >= result.capacity) {
			result.capacity *= 2;
			int *newIds = new int[result.capacity];
			memcpy(newIds, result.songIds, sizeof(int) * result.count);
			delete[] result.songIds;
			result.songIds = newIds;
		}

		result.songIds[result.count++] = id;
	}

	cout << "[SEARCH] Resultados totales: " << result.count << " canciones" << endl;

	return result;
}

void indexSong(Song song) {
	// Verificar NUEVAMENTE que no exista (por seguridad)
	if (isDuplicateURL(globalDB, song.url)) {
		string response = "ERROR duplicate_url\n";
		cout << "[INDEX] URL duplicada (doble verificación)" << endl;
		return;
	}

	// Añadir canción a la database (SOLO EN MEMORIA)
	int songId = addSong(globalDB, song);

	if (songId < 0) {
		string error = "ERROR could_not_add_song\n";
		return;
	}

	cout << "[INDEX] Canción añadida e indexada en memoria: [" << songId << "] "
		 << song.title << " - " << song.artist << endl;
}

void freeSearchResult(SearchResult *result) {
	if (result && result->songIds) {
		delete[] result->songIds;
		result->songIds = nullptr;
		result->count = 0;
		result->capacity = 0;
	}
}