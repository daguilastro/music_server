#include "bktree.hpp"
#include <cstddef>
#include <cstring>
#include <algorithm>

void insertWordBKTree(BKNode *&root, string word, int songId) {
	if (root == nullptr) {
		root = createBKNode(word, songId);
		return;
	}

	recursiveBKInsert(root, word, songId);
}

void recursiveBKInsert(BKNode *node, string word, int songId) {
	string nodeWord(node->word);

	if (word == nodeWord) {
		for (int i = 0; i < node->songIdCount; i++) {
			if (node->songIds[i] == songId) {
				return;
			}
		}
		if (node->songIdCount >= node->songIdCapacity) {
			int newCapacity = node->songIdCapacity * 2;
			int *newSongIds = new int[newCapacity];
			memcpy(newSongIds, node->songIds, node->songIdCount * sizeof(int));
			delete[] node->songIds;
			node->songIds = newSongIds;
			node->songIdCapacity = newCapacity;
		}
		node->songIds[node->songIdCount++] = songId;
		return;
	}

	int distance = levenshteinDistance(word, node->word);

	BKNode *child = nullptr;
	for (int i = 0; i < node->childrenCount; i++) {
		if (node->children[i].distance == distance) {
			child = node->children[i].node;
			break;
		}
	}

	if (child != nullptr) {
		recursiveBKInsert(child, word, songId);
		return;
	} else {
		BKNode *newNode = createBKNode(word, songId);

		if (node->childrenCount >= node->childrenCapacity) {
			int newCapacity = node->childrenCapacity * 2;
			BKChild *newChildren = new BKChild[newCapacity];
			memcpy(newChildren, node->children, node->childrenCount * sizeof(BKChild));
			delete[] node->children;
			node->children = newChildren;
			node->childrenCapacity = newCapacity;
		}

		node->children[node->childrenCount].distance = distance;
		node->children[node->childrenCount++].node = newNode;
	}
}

int levenshteinDistance(string word, string target) {
	int **dp = new int *[word.length() + 1];
	for (size_t i = 0; i <= word.length(); i++) {
		dp[i] = new int[target.length() + 1];
	}

	for (size_t j = 0; j <= target.length(); j++) {
		dp[0][j] = j;
	}
	for (size_t i = 0; i <= word.length(); i++) {
		dp[i][0] = i;
	}
	for (size_t i = 1; i <= word.length(); i++) {
		for (size_t j = 1; j <= target.length(); j++) {
			if (word[i - 1] == target[j - 1]) {
				dp[i][j] = dp[i - 1][j - 1];
			} else {
				dp[i][j] = 1 + std::min({ dp[i - 1][j], dp[i][j - 1], dp[i - 1][j - 1] });
			}
		}
	}

	int res = dp[word.length()][target.length()];

	for (size_t i = 0; i <= word.length(); i++) delete[] dp[i];
	delete[] dp;

	return res;
}

BKNode *createBKNode(string word, int songId) {
	BKNode *root = new BKNode;
	root->songIdCapacity = 4;
	root->songIds = new int[4];
	root->songIds[0] = songId;
	root->songIdCount = 1;
	root->childrenCount = 0;
	root->childrenCapacity = 5;
	strcpy(root->word, word.c_str());
	root->children = new BKChild[5];

	return root;
}

void recursiveBKSearch(BKNode* node, string word, int tolerance, std::unordered_set<int>& idsFound){
	int distance = levenshteinDistance(node->word, word);

	if (distance <= tolerance){
		for (int i = 0; i < node->songIdCount; i++){
			idsFound.insert(node->songIds[i]);
		}
	}

	int minDistance = distance - tolerance;
	int maxDistance = distance + tolerance;

	for (int i = 0; i < node->childrenCount; i++){
		int childDistance = node->children[i].distance;

		if (childDistance >= minDistance && childDistance <= maxDistance){
			recursiveBKSearch(node->children[i].node, word, tolerance, idsFound);
		}
	}
}

void freeBKNode(BKNode* node) {
	if (!node) return;
	
	// Liberar hijos recursivamente
	for (int i = 0; i < node->childrenCount; i++) {
		freeBKNode(node->children[i].node);
	}
	
	// Liberar arrays
	delete[] node->songIds;
	delete[] node->children;
	
	// Liberar nodo
	delete node;
}