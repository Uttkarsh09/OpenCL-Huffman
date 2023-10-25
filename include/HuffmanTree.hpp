#ifndef HUFFTREE
#define HUFFTREE

#include <stdio.h>
#include "configuration.hpp"
#include <algorithm>

size_t round_global_size(int local_size, unsigned int global_size);
bool sortByCharFreq(const HuffNode *a, const HuffNode *b);
HuffNode *combineNodes(HuffNode *a, HuffNode *b);
HuffNode *generateHuffmanTree(vector<HuffNode *> frequencies);
void generateHuffmanTree(HuffNode *const root, const string &codes, const unsigned char ch);
u_int32_t populateHuffmanTable(vector<HuffNode *> *frequencies, HuffNode *rootNode, string &value);

#endif // HUFFTREE
