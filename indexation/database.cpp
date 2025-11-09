#include "database.hpp"
#include "bktree.hpp"
#include "inverted_index.hpp"
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
	db->titleIndex = createInvertedIndex();
	db->artistIndex = createInvertedIndex();

	if (!db->titleIndex || !db->artistIndex) {
		cerr << "[ERROR] No se pudieron crear los índices" << endl;
		delete[] db->songs;
		delete db;
		return nullptr;
	}

	cout << "[INFO] Base de datos creada (vacía) en RAM" << endl;

	return db;
}

// ===== LIBERAR MEMORIA =====
void freeDatabase(SongDatabase *db) {
	if (!db) return;
	freeBKNode(db->bkTree);
	freeInvertedIndex(db->titleIndex);
	freeInvertedIndex(db->artistIndex);
	delete[] db->songs;
	delete db;
}

// ===== VERIFICAR URL DUPLICADA =====
bool isDuplicateURL(SongDatabase *db, const char *url) {
	for (int i = 0; i < db->songCount; i++) {
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
	songSave->duration = songSent.duration;
	songSave->id = db->nextSongId++;

	// ===== INDEXAR TÍTULO =====
	cout << "[INDEX] Indexando título: \"" << songSave->title << "\"..." << endl;

	char titleWords[50][64];
	int titleWordCount = 0;
	extractWords(songSave->title, titleWords, &titleWordCount, 50);

	cout << "[INDEX] Palabras extraídas del título: " << titleWordCount << endl;

	for (int i = 0; i < titleWordCount; i++) {
		cout << "[INDEX]   - \"" << titleWords[i] << "\"" << endl;
		insertWordBKTree(db->bkTree, titleWords[i], songSave->id);
		addToIndex(db->titleIndex, titleWords[i], songSave->id);
	}

	// ===== INDEXAR ARTISTA =====
	if (songSave->artist[0] != '\0' && strcmp(songSave->artist, "Unknown") != 0) {
		cout << "[INDEX] Indexando artista: \"" << songSave->artist << "\"..." << endl;

		char artistWords[50][64];
		int artistWordCount = 0;
		extractWords(songSave->artist, artistWords, &artistWordCount, 50);

		cout << "[INDEX] Palabras extraídas del artista: " << artistWordCount << endl;

		for (int i = 0; i < artistWordCount; i++) {
			cout << "[INDEX]   - \"" << artistWords[i] << "\"" << endl;
			insertWordBKTree(db->bkTree, artistWords[i], songSave->id);
			addToIndex(db->artistIndex, artistWords[i], songSave->id);
		}
	}

	cout << "[INFO] Canción añadida e indexada: [" << songSave->id << "] " << songSave->title << " - " << songSave->artist << endl;
	cout << "[DEBUG] Estado de índices:" << endl;
	cout << "[DEBUG]   titleIndex->count = " << db->titleIndex->count << endl;
	cout << "[DEBUG]   artistIndex->count = " << db->artistIndex->count << endl;

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

// ===== INFORMACIÓN =====
int getSongCount(SongDatabase *db) {
	return db->songCount;
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
	db->titleIndex = createInvertedIndex();
	db->artistIndex = createInvertedIndex();
	db->bkTree = nullptr;

	if (!db->titleIndex || !db->artistIndex) {
		cerr << "[ERROR] No se pudieron crear los índices" << endl;
		freeDatabase(db);
		close(fd);
		return nullptr;
	}

	cout << "[DEBUG] Índices creados (vacíos)" << endl;

	for (int i = 0; i < db->songCount; i++) {
		Song *song = &db->songs[i];
		char titleWords[50][64];
		int titleWordCount = 0;

		extractWords(song->title, titleWords, &titleWordCount, 50);

		for (int j = 0; j < titleWordCount; j++) {
			addToIndex(db->titleIndex, titleWords[j], song->id);
			insertWordBKTree(db->bkTree, titleWords[j], song->id);
		}

		char artistWords[50][64];
		int artistWordCount = 0;

		extractWords(song->artist, artistWords, &artistWordCount, 50);

		for (int j = 0; j < artistWordCount; j++) {
			addToIndex(db->artistIndex, artistWords[j], song->id);
			insertWordBKTree(db->bkTree, artistWords[j], song->id);
		}
	}

	cout << "[INFO] Índices reconstruidos:" << endl;
	cout << "  - Palabras en títulos: " << db->titleIndex->count << endl;
	cout << "  - Palabras en artistas: " << db->artistIndex->count << endl;

	return db;
}

// ============================================
// ===== BÚSQUEDA DE CANCIONES =====
// ============================================

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
	if (!db->titleIndex) {
		cerr << "[ERROR] searchSongs: titleIndex es NULL" << endl;
		return result;
	}

	if (!db->artistIndex) {
		cerr << "[ERROR] searchSongs: artistIndex es NULL" << endl;
		return result;
	}

	cout << "[SEARCH] Buscando: \"" << query << "\"" << endl;
	// ===== DEBUG: ESTADO DE LOS ÍNDICES =====
	cout << "[DEBUG] titleIndex->count = " << db->titleIndex->count << endl;
	cout << "[DEBUG] artistIndex->count = " << db->artistIndex->count << endl;

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

	unordered_set<int> bkTreeIds;

	// ===== BUSCAR EN TÍTULOS =====

	for (int i = 0; i < queryWordCount; i++) {
		cout << "[DEBUG] Buscando \"" << queryWords[i] << "\" en titleIndex..." << endl;

		WordEntry *entry = findWord(db->titleIndex, queryWords[i]);
		if (entry) {
			cout << "[SEARCH] \"" << queryWords[i] << "\" encontrada en títulos: "
				 << entry->count << " canciones" << endl;

			for (int j = 0; j < entry->count; j++) {
				foundIds.insert(entry->songIds[j]);
			}
		} else {
			cout << "[DEBUG] \"" << queryWords[i] << "\" NO encontrada en titleIndex" << endl;
		}
	}

	// ===== BUSCAR EN ARTISTAS Y BK TREE LUEGO TOCA ARREGLAR TODA ESTA PARTE=====

	for (int i = 0; i < queryWordCount; i++) {
		cout << "[DEBUG] Buscando \"" << queryWords[i] << "\" en artistIndex..." << endl;

		WordEntry *entry = findWord(db->artistIndex, queryWords[i]);
		recursiveBKSearch(db->bkTree, queryWords[i], 2, bkTreeIds);
		if (entry) {
			cout << "[SEARCH] \"" << queryWords[i] << "\" encontrada en artistas: "
				 << entry->count << " canciones" << endl;

			for (int j = 0; j < entry->count; j++) {
				foundIds.insert(entry->songIds[j]);
			}
		} else {
			cout << "[DEBUG] \"" << queryWords[i] << "\" NO encontrada en artistIndex" << endl;
		}
	}

	if (!bkTreeIds.empty()) {
		cout << "  ✅ Fuzzy encontró " << bkTreeIds.size() << " canciones" << endl;

		// Añadir todos los IDs del BK-Tree a foundIds
		for (int id : bkTreeIds) {
			foundIds.insert(id);
		}
	} else {
		cout << "  ❌ Fuzzy no encontró resultados" << endl;
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
