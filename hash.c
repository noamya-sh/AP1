#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TABLE_SIZE 100 // Size of the hash table

// Node structure for storing key-value pairs
typedef struct Node {
    char* key;
    char* value;
    struct Node* next;
} Node;

// Hash table structure
typedef struct HashTable {
    Node* table[TABLE_SIZE];
} HashTable;

// Hash function
int hash(char* key) {
    int hashVal = 0;
    for (int i = 0; i < strlen(key); i++) {
        hashVal += key[i];
    }
    return hashVal % TABLE_SIZE;
}

// Create a new node with key and value
Node* createNode(char* key, char* value) {
    Node* newNode = (Node*)malloc(sizeof(Node));
    if (newNode == NULL) {
        printf("Memory allocation failed\n");
        exit(1);
    }
    newNode->key = key;
    newNode->value = value;
    newNode->next = NULL;
    return newNode;
}

// Insert a key-value pair into the hash table
void insert(HashTable* ht, char* key, char* value) {
    int index = hash(key);
    Node* newNode = createNode(key, value);

    // If the bucket is empty, insert the node as the head
    if (ht->table[index] == NULL) {
        ht->table[index] = newNode;
    }
        // If the bucket is not empty, append the node to the end of the list
    else {
        Node* currNode = ht->table[index];
        while (currNode->next != NULL) {
            currNode = currNode->next;
        }
        currNode->next = newNode;
    }
}

// Search for a value by key in the hash table
char* search(HashTable* ht, char* key) {
    int index = hash(key);
    Node* currNode = ht->table[index];
    while (currNode != NULL) {
        if (strcmp(currNode->key, key) == 0) {
            return currNode->value;
        }
        currNode = currNode->next;
    }
    return NULL; // Key not found
}
// Free the memory allocated for the nodes in the hash table
void freeHashTable(HashTable* ht) {
    for (int i = 0; i < TABLE_SIZE; i++) {
        Node* currNode = ht->table[i];
        while (currNode != NULL) {
            Node* nextNode = currNode->next;
            free(currNode);
            currNode = nextNode;
        }
    }
}
// Main function for demonstration
//int main() {
//    HashTable ht;
//    for (int i = 0; i < TABLE_SIZE; i++) {
//        ht.table[i] = NULL;
//    }
//
//    // Insert key-value pairs into the hash table
//    insert(&ht, "key1", "value1");
//    insert(&ht, "key2", "value2");
//    insert(&ht, "key3", "value3");
//
//    // Search for values by keys
//    printf("Value for key 'key1': %s\n", search(&ht, "key1"));
//    printf("Value for key 'key2': %s\n", search(&ht, "key2"));
//    printf("Value for key 'key3': %s\n", search(&ht, "key3"));
//
//    return 0;
//}