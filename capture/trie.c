/******************************************************************************/
/* trie.c  -- Simple trie implementation
 *
 * Copyright 2012-2017 AOL Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this Software except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "moloch.h"

uint64_t addNCnt;
uint64_t addCnt;
uint64_t getCnt;
uint64_t delCnt;
uint64_t walkNCnt;


void moloch_trie_init(MolochTrie_t *trie)
{
    memset(trie, 0, sizeof(*trie));
    trie->root.data = 0;
    trie->root.first = 0;
    trie->root.first = 255;
    trie->root.children = malloc(sizeof(MolochTrieNode_t *) * 256);
    memset(trie->root.children, 0, sizeof(MolochTrieNode_t *) * 256);
}

MolochTrieNode_t *moloch_trie_add_node(MolochTrieNode_t *node, const char key)
{
    addNCnt++;
    // No children, add 1 child
    if (!node->children) {
        node->first = node->last = key;
        node->children = malloc(sizeof(MolochTrieNode_t *));
        memset(node->children, 0, sizeof(void *));
        node->children[0] = malloc(sizeof(MolochTrieNode_t));
        memset(node->children[0], 0, sizeof(MolochTrieNode_t));
        return node->children[0];
    }

    // Some children, is this value in range
    if (key >= node->first && key <= node->last) {
        const int p = key - node->first;
        if (!node->children[p]) {
            node->children[p] = malloc(sizeof(MolochTrieNode_t));
            memset(node->children[p], 0, sizeof(MolochTrieNode_t));
        }
        return node->children[p];
    }

    // Not in range, so expand
    if (key < node->first) {
        MolochTrieNode_t **newchildren = malloc(sizeof(MolochTrieNode_t *)*(node->last - key + 1));
        memset(newchildren, 0, sizeof(void*)*(node->first-key));
        memcpy(newchildren + (node->first-key), (void*)node->children, sizeof(void*) * (node->last - node->first + 1));
        free(node->children);
        node->children = newchildren;
        node->first = key;
    } else if (key > node->last) {
        node->children = realloc(node->children, sizeof(MolochTrieNode_t *)*(key - node->first + 1));
        memset(node->children + (node->last - node->first + 1), 0, sizeof(void*)*(key-node->last));
        node->last = key;
    }

    const int p = key - node->first;
    if (!node->children[p]) {
        node->children[p] = malloc(sizeof(MolochTrieNode_t));
        memset(node->children[p], 0, sizeof(MolochTrieNode_t));
    }
    return node->children[p];
}

void moloch_trie_add_forward(MolochTrie_t *trie, const char *key, const int len, void *data)
{
    addCnt++;
    MolochTrieNode_t *node = &trie->root;

    int i;
    for (i = 0; i < len; i++) {
        node = moloch_trie_add_node(node, key[i]);
    }
    if (!node->data)
        trie->size++;
    node->data = data;
}

void moloch_trie_add_reverse(MolochTrie_t *trie, const char *key, const int len, void *data)
{
    MolochTrieNode_t *node = &trie->root;

    int i;
    for (i = len-1; i >= 0; i--) {
        node = moloch_trie_add_node(node, key[i]);
    }
    if (!node->data)
        trie->size++;
    node->data = data;
}

MolochTrieNode_t * moloch_trie_walk_forward(MolochTrieNode_t *node, const char *key, const int len)
{
    walkNCnt++;
    int i;
    for (i = 0; i < len; i++) {
        if (!node->children)
            return NULL;

        if (key[i] < node->first || key[i] > node->last)
            return NULL;

        const int p = key[i] - node->first;
        if (!node->children[p])
            return NULL;

        node = node->children[p];
        continue;
    }
    return node;
}

void * moloch_trie_get_forward(MolochTrie_t *trie, const char *key, const int len)
{
    getCnt++;
    MolochTrieNode_t *node = &trie->root;

    int i;
    for (i = 0; i < len; i++) {
        if (!node->children)
            return NULL;

        if (key[i] < node->first || key[i] > node->last)
            return NULL;

        const int p = key[i] - node->first;
        if (!node->children[p])
            return NULL;

        node = node->children[p];
        continue;
    }
    return node->data;
}

void * moloch_trie_get_reverse(MolochTrie_t *trie, const char *key, const int len)
{
    MolochTrieNode_t *node = &trie->root;

    int i;
    for (i = len-1; i >= 0; i--) {
        if (!node->children)
            return NULL;

        if (key[i] < node->first || key[i] > node->last)
            return NULL;

        const int p = key[i] - node->first;
        if (!node->children[p])
            return NULL;

        node = node->children[p];
        continue;
    }
    return node->data;
}

void * moloch_trie_best_forward(MolochTrie_t *trie, const char *key, const int len)
{
    MolochTrieNode_t *node = &trie->root;
    void *data = NULL;

    int i;
    for (i = 0; i < len; i++) {
        if (!node->children)
            return data;

        if (key[i] < node->first || key[i] > node->last)
            return data;

        const int p = key[i] - node->first;
        if (!node->children[p])
            return data;

        node = node->children[p];
        if (node->data)
            data = node->data;
    }
    return data;
}

void * moloch_trie_best_reverse(MolochTrie_t *trie, const char *key, const int len)
{
    MolochTrieNode_t *node = &trie->root;
    void *data = NULL;

    int i;
    for (i = len-1; i >= 0; i--) {
        if (!node->children)
            return data;

        if (key[i] < node->first || key[i] > node->last)
            return data;

        const int p = key[i] - node->first;
        if (!node->children[p])
            return data;

        node = node->children[p];
        if (node->data)
            data = node->data;
    }
    return data;
}

void * moloch_trie_del_forward(MolochTrie_t *trie, const char *key, const int len)
{
    delCnt++;
    MolochTrieNode_t *node = &trie->root;

    int i;
    for (i = 0; i < len; i++) {
        if (!node->children)
            return NULL;

        if (key[i] < node->first || key[i] > node->last)
            return NULL;

        const int p = key[i] - node->first;
        if (!node->children[p])
            return NULL;

        node = node->children[p];
        continue;
    }
    if (node->data)
        trie->size--;
    void *data = node->data;
    node->data = NULL;
    return data;
}

void * moloch_trie_del_reverse(MolochTrie_t *trie, const char *key, const int len)
{
    MolochTrieNode_t *node = &trie->root;

    int i;
    for (i = len-1; i >= 0; i--) {
        if (!node->children)
            return NULL;

        if (key[i] < node->first || key[i] > node->last)
            return NULL;

        const int p = key[i] - node->first;
        if (!node->children[p])
            return NULL;

        node = node->children[p];
        continue;
    }
    if (node->data)
        trie->size--;
    void *data = node->data;
    node->data = NULL;
    return data;
}

void moloch_trie_print_node(MolochTrieNode_t *node, int level)
{
    int p;
    if (node->data)
        printf("%.*s data:%p\n", level, "          ", node->data);

    if (node->children) {
        for (p = 0; p <= node->last - node->first; p++) {
            if (node->children[p]) {
                printf("%.*s %c\n", level, "          ", p + node->first);
                moloch_trie_print_node(node->children[p], level+1);
            }
        }
    }
}

void moloch_trie_print(MolochTrie_t *trie)
{
    printf("Size: %d\n", trie->size);
    moloch_trie_print_node(&trie->root, 0);
}

void moloch_trie_exit()
{
    /*
    printf("addN: %ld\n", addNCnt);
    printf("add: %ld\n", addCnt);
    printf("get: %ld\n", getCnt);
    printf("del: %ld\n", delCnt);
    printf("walkN: %ld\n", walkNCnt);
    */
}
/*
main() {
  MolochTrie_t trie;
  moloch_trie_init(&trie);
  moloch_trie_add_forward(&trie, "a", 1, (void*)1);
  moloch_trie_add_forward(&trie, "b", 1, (void*)2);
  moloch_trie_add_forward(&trie, "ab", 2, (void*)3);
  moloch_trie_add_forward(&trie, "aa", 2, (void*)4);
  moloch_trie_add_forward(&trie, "ba", 2, (void*)5);
  moloch_trie_add_forward(&trie, "bb", 2, (void*)6);
  moloch_trie_add_forward(&trie, "andy", 4, (void*)7);
  moloch_trie_add_reverse(&trie, "andy", 4, (void*)8);
  moloch_trie_print(&trie);
  moloch_trie_add_reverse(&trie, "andy", 4, (void*)8);
  printf("Size: 8=%d\n", trie.size);

  printf("bndy forward %x\n", moloch_trie_get_forward(&trie, "bndy", 4));
  printf("andy forward %x\n", moloch_trie_get_forward(&trie, "andy", 4));
  printf("andy reverse %x\n", moloch_trie_get_reverse(&trie, "andy", 4));

  printf("best bndy forward %x\n", moloch_trie_best_forward(&trie, "bndy", 4));
  printf("best andy forward %x\n", moloch_trie_best_forward(&trie, "andy", 4));
  printf("best andy reverse %x\n", moloch_trie_best_reverse(&trie, "andy", 4));
  printf("best andyandy forward %x\n", moloch_trie_best_forward(&trie, "andyandy", 8));
  printf("best andyandy reverse %x\n", moloch_trie_best_reverse(&trie, "andyandy", 8));

  moloch_trie_del_forward(&trie, "a", 1);
  moloch_trie_print(&trie);
  moloch_trie_del_forward(&trie, "a", 1);
  printf("Size: 7=%d\n", trie.size);
}*/
