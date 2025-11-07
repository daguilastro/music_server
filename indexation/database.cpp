#include "database.hpp"
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
int addSong(SongDatabase *db, const char *title, const char *filename,
	const char *url, const char *artist, uint32_t duration) {
	if (db->songCount >= db->songCapacity) {
		db->songCapacity *= 2;
		Song *newSongs = new Song[db->songCapacity];
		memcpy(newSongs, db->songs, sizeof(Song) * db->songCount);
		delete[] db->songs;
		db->songs = newSongs;
	}

	Song *song = &db->songs[db->songCount++];
	song->id = db->nextSongId++;

	strncpy(song->title, title, sizeof(song->title) - 1);
	song->title[sizeof(song->title) - 1] = '\0';

	strncpy(song->artist, artist, sizeof(song->artist) - 1);
	song->artist[sizeof(song->artist) - 1] = '\0';

	strncpy(song->filename, filename, sizeof(song->filename) - 1);
	song->filename[sizeof(song->filename) - 1] = '\0';

	strncpy(song->url, url, sizeof(song->url) - 1);
	song->url[sizeof(song->url) - 1] = '\0';

	song->duration = duration;

	// ===== INDEXAR TÍTULO =====
	cout << "[INDEX] Indexando título: \"" << title << "\"..." << endl;

	char titleWords[50][64];
	int titleWordCount = 0;
	extractWords(title, titleWords, &titleWordCount, 50);

	cout << "[INDEX] Palabras extraídas del título: " << titleWordCount << endl;

	for (int i = 0; i < titleWordCount; i++) {
		cout << "[INDEX]   - \"" << titleWords[i] << "\"" << endl;
		addToIndex(db->titleIndex, titleWords[i], song->id);
	}

	// ===== INDEXAR ARTISTA =====
	if (artist[0] != '\0' && strcmp(artist, "Unknown") != 0) {
		cout << "[INDEX] Indexando artista: \"" << artist << "\"..." << endl;

		char artistWords[50][64];
		int artistWordCount = 0;
		extractWords(artist, artistWords, &artistWordCount, 50);

		cout << "[INDEX] Palabras extraídas del artista: " << artistWordCount << endl;

		for (int i = 0; i < artistWordCount; i++) {
			cout << "[INDEX]   - \"" << artistWords[i] << "\"" << endl;
			addToIndex(db->artistIndex, artistWords[i], song->id);
		}
	}

	cout << "[INFO] Canción añadida e indexada: [" << song->id << "] " << title << " - " << artist << endl;
	cout << "[DEBUG] Estado de índices:" << endl;
	cout << "[DEBUG]   titleIndex->count = " << db->titleIndex->count << endl;
	cout << "[DEBUG]   artistIndex->count = " << db->artistIndex->count << endl;

	return song->id;
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
	cout << "[DEBUG] ===== INICIO saveDatabase() =====" << endl;

	if (!db) {
		cerr << "[ERROR] Base de datos es NULL" << endl;
		return false;
	}

	cout << "[DEBUG] db->songCount = " << db->songCount << endl;
	cout << "[DEBUG] db->titleIndex = " << (void *)db->titleIndex << endl;
	cout << "[DEBUG] db->artistIndex = " << (void *)db->artistIndex << endl;

	if (!db->titleIndex || !db->artistIndex) {
		cerr << "[ERROR] Los índices no están inicializados" << endl;
		return false;
	}

	cout << "[DEBUG] db->titleIndex->count = " << db->titleIndex->count << endl;
	cout << "[DEBUG] db->artistIndex->count = " << db->artistIndex->count << endl;

	int fd = open(filepath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0) {
		cerr << "[ERROR] No se pudo abrir archivo para escritura: " << filepath << endl;
		return false;
	}

	cout << "[INFO] Guardando base de datos en: " << filepath << endl;

	// ===== CREAR HEADER =====
	DatabaseHeader header;
	memset(&header, 0, sizeof(DatabaseHeader));

	memcpy(header.magic, "MUSI", 4);
	header.version = 1;
	header.numSongs = db->songCount;
	header.numTitleWords = db->titleIndex->count;
	header.numArtistWords = db->artistIndex->count;

	cout << "[DEBUG] Header preparado:" << endl;
	cout << "        - magic: " << header.magic[0] << header.magic[1]
		 << header.magic[2] << header.magic[3] << endl;
	cout << "        - version: " << header.version << endl;
	cout << "        - numSongs: " << header.numSongs << endl;
	cout << "        - numTitleWords: " << header.numTitleWords << endl;
	cout << "        - numArtistWords: " << header.numArtistWords << endl;

	// Calcular offsets
	header.offsetSongs = sizeof(DatabaseHeader);
	header.offsetTitleIndex = header.offsetSongs + (db->songCount * sizeof(Song));

	cout << "[DEBUG] Offset canciones: " << header.offsetSongs << endl;
	cout << "[DEBUG] Offset índice títulos: " << header.offsetTitleIndex << endl;

	// Calcular tamaño del índice de títulos
	size_t titleIndexSize = 0;
	for (int i = 0; i < db->titleIndex->count; i++) {
		titleIndexSize += 64 + 4 + (4 * db->titleIndex->entries[i].count);
	}

	header.offsetArtistIndex = header.offsetTitleIndex + titleIndexSize;

	cout << "[DEBUG] Tamaño índice títulos: " << titleIndexSize << " bytes" << endl;
	cout << "[DEBUG] Offset índice artistas: " << header.offsetArtistIndex << endl;

	header.offsetTitleTrie = 0;
	header.offsetArtistTrie = 0;
	header.offsetTitleBKTree = 0;
	header.offsetArtistBKTree = 0;

	memset(header.reserved, 0, sizeof(header.reserved));

	// ===== ESCRIBIR HEADER =====
	cout << "[DEBUG] Escribiendo header..." << endl;
	ssize_t written = write(fd, &header, sizeof(DatabaseHeader));
	if (written != sizeof(DatabaseHeader)) {
		cerr << "[ERROR] No se pudo escribir el header" << endl;
		cerr << "[DEBUG] Bytes escritos: " << written << " de " << sizeof(DatabaseHeader) << endl;
		close(fd);
		return false;
	}

	cout << "[INFO] Header escrito (" << sizeof(DatabaseHeader) << " bytes)" << endl;

	// ===== ESCRIBIR CANCIONES =====
	if (db->songCount > 0) {
		cout << "[DEBUG] Escribiendo " << db->songCount << " canciones..." << endl;
		size_t totalBytes = sizeof(Song) * db->songCount;
		written = write(fd, db->songs, totalBytes);
		if (written != (ssize_t)totalBytes) {
			cerr << "[ERROR] No se pudieron escribir todas las canciones" << endl;
			cerr << "[DEBUG] Bytes escritos: " << written << " de " << totalBytes << endl;
			close(fd);
			return false;
		}

		cout << "[INFO] " << db->songCount << " canciones escritas (" << totalBytes << " bytes)" << endl;
	} else {
		cout << "[INFO] No hay canciones para guardar" << endl;
	}

	// ===== ESCRIBIR ÍNDICE DE TÍTULOS =====
	if (db->titleIndex->count > 0) {
		cout << "[DEBUG] Escribiendo índice de títulos (" << db->titleIndex->count << " palabras)..." << endl;
		for (int i = 0; i < db->titleIndex->count; i++) {
			WordEntry *entry = &db->titleIndex->entries[i];

			cout << "[DEBUG]   - Palabra " << i << ": \"" << entry->word
				 << "\" (" << entry->count << " IDs)" << endl;

			write(fd, entry->word, 64);
			write(fd, &entry->count, sizeof(int));
			write(fd, entry->songIds, sizeof(int) * entry->count);
		}
		cout << "[INFO] Índice de títulos guardado" << endl;
	} else {
		cout << "[INFO] No hay palabras en índice de títulos" << endl;
	}

	// ===== ESCRIBIR ÍNDICE DE ARTISTAS =====
	if (db->artistIndex->count > 0) {
		cout << "[DEBUG] Escribiendo índice de artistas (" << db->artistIndex->count << " palabras)..." << endl;
		for (int i = 0; i < db->artistIndex->count; i++) {
			WordEntry *entry = &db->artistIndex->entries[i];

			cout << "[DEBUG]   - Palabra " << i << ": \"" << entry->word
				 << "\" (" << entry->count << " IDs)" << endl;

			write(fd, entry->word, 64);
			write(fd, &entry->count, sizeof(int));
			write(fd, entry->songIds, sizeof(int) * entry->count);
		}
		cout << "[INFO] Índice de artistas guardado" << endl;
	} else {
		cout << "[INFO] No hay palabras en índice de artistas" << endl;
	}

	close(fd);

	cout << "[INFO] Base de datos guardada exitosamente" << endl;
	cout << "[DEBUG] ===== FIN saveDatabase() =====" << endl;

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
	cout << "[INFO] Palabras en títulos: " << header.numTitleWords << endl;
	cout << "[INFO] Palabras en artistas: " << header.numArtistWords << endl;

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

	// ===== CREAR ÍNDICES =====
	cout << "[DEBUG] Creando índices..." << endl;
	db->titleIndex = createInvertedIndex();
	db->artistIndex = createInvertedIndex();

	if (!db->titleIndex || !db->artistIndex) {
		cerr << "[ERROR] No se pudieron crear los índices" << endl;
		freeDatabase(db);
		close(fd);
		return nullptr;
	}

	cout << "[DEBUG] Índices creados (vacíos)" << endl;

	// ===== CARGAR ÍNDICE DE TÍTULOS =====
	if (header.numTitleWords > 0 && header.offsetTitleIndex > 0) {
		cout << "[DEBUG] Cargando índice de títulos..." << endl;
		lseek(fd, header.offsetTitleIndex, SEEK_SET);

		for (uint32_t i = 0; i < header.numTitleWords; i++) {
			char word[64];
			int count;

			read(fd, word, 64);
			read(fd, &count, sizeof(int));

			cout << "[DEBUG]   - Palabra " << i << ": \"" << word << "\" (" << count << " IDs)" << endl;

			if (count > 0 && count < 10000) {
				int *songIds = new int[count];
				read(fd, songIds, sizeof(int) * count);

				for (int j = 0; j < count; j++) {
					addToIndex(db->titleIndex, word, songIds[j]);
				}

				delete[] songIds;
			}
		}
		cout << "[INFO] Índice de títulos cargado (" << db->titleIndex->count << " palabras)" << endl;
	}

	// ===== CARGAR ÍNDICE DE ARTISTAS =====
	if (header.numArtistWords > 0 && header.offsetArtistIndex > 0) {
		cout << "[DEBUG] Cargando índice de artistas..." << endl;
		lseek(fd, header.offsetArtistIndex, SEEK_SET);

		for (uint32_t i = 0; i < header.numArtistWords; i++) {
			char word[64];
			int count;

			read(fd, word, 64);
			read(fd, &count, sizeof(int));

			cout << "[DEBUG]   - Palabra " << i << ": \"" << word << "\" (" << count << " IDs)" << endl;

			if (count > 0 && count < 10000) {
				int *songIds = new int[count];
				read(fd, songIds, sizeof(int) * count);

				for (int j = 0; j < count; j++) {
					addToIndex(db->artistIndex, word, songIds[j]);
				}

				delete[] songIds;
			}
		}
		cout << "[INFO] Índice de artistas cargado (" << db->artistIndex->count << " palabras)" << endl;
	}

	close(fd);

	cout << "[INFO] Base de datos cargada exitosamente" << endl;
	cout << "[DEBUG] ===== FIN loadDatabase() =====" << endl;

	return db;
}

// ============================================
// ===== BÚSQUEDA DE CANCIONES =====
// ============================================

SearchResult searchSongs(SongDatabase *db, const char *query, bool searchInTitle, bool searchInArtist) {
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
	cout << "         - En títulos: " << (searchInTitle ? "SÍ" : "NO") << endl;
	cout << "         - En artistas: " << (searchInArtist ? "SÍ" : "NO") << endl;

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

	// ===== BUSCAR EN TÍTULOS =====
	if (searchInTitle) {
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
	}

	// ===== BUSCAR EN ARTISTAS =====
	if (searchInArtist) {
		for (int i = 0; i < queryWordCount; i++) {
			cout << "[DEBUG] Buscando \"" << queryWords[i] << "\" en artistIndex..." << endl;

			WordEntry *entry = findWord(db->artistIndex, queryWords[i]);

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

void freeSearchResult(SearchResult *result) {
	if (result && result->songIds) {
		delete[] result->songIds;
		result->songIds = nullptr;
		result->count = 0;
		result->capacity = 0;
	}
}
