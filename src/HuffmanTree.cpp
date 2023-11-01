#include "HuffmanTree.hpp"

size_t round_global_size(int local_size, unsigned int global_size)
{
	// Round Global Size To Nearest Multiple Of Local Size

	// Code
	unsigned int remainder = global_size % local_size;

	if (remainder == 0)
		return global_size;
	else
		return global_size + local_size - remainder;
}

bool sortByCharFreq(const HuffNode *a, const HuffNode *b)
{
	return a->frequency > b->frequency;
}

HuffNode *combineNodes(HuffNode *a, HuffNode *b)
{
	// printf("Combining %c(%d) & %c(%d)\n", a->ch, a->frequency, b->ch, b->frequency);

	HuffNode *parent = new HuffNode((a ? a->frequency : 0) + (b ? b->frequency : 0));
	parent->left = b;
	parent->right = a;
	return parent;
}

HuffNode *generateHuffmanTree(vector<HuffNode *> frequencies)
{
	HuffNode *one, *two, *parent;
	// Sort by frequency
	sort(frequencies.begin(), frequencies.end(), sortByCharFreq);


	if (frequencies.size() == 1)
	{
		return combineNodes(frequencies.back(), nullptr);
	}

	while (frequencies.size() > 2)
	{
		one = *(frequencies.end() - 1);
		two = *(frequencies.end() - 2);
		frequencies.pop_back();
		frequencies.pop_back();
		parent = combineNodes(one, two);
		frequencies.push_back(parent);

		vector<HuffNode *>::iterator itr = frequencies.end() - 2;
		while (((*itr)->frequency < parent->frequency) && (itr != frequencies.begin()))
		{
			--itr;
		}
		sort(itr, frequencies.end(), sortByCharFreq);
	}
	one = *(frequencies.end() - 1);
	two = *(frequencies.end() - 2);
	return combineNodes(one, two);
}

void generateHuffmanTree(HuffNode *const root, const string &codes, const unsigned char ch){
	HuffNode *traverse = root;
	int i = 0;

	while(codes[i] != '\0'){
		if(codes[i] == '0'){
			if(!traverse->left){
				traverse->left = new HuffNode(0);
			}
			traverse = traverse->left;
		}
		else {
			if(!traverse->right){
				traverse->right = new HuffNode(0);
			}
			traverse = traverse->right;
		}
		++i;
	}
	traverse->ch = ch;
}

u_int32_t populateHuffmanTable(vector<HuffNode *> *frequencies, HuffNode *rootNode, string &value)
{
	u_int32_t temp = 0;

	if (rootNode)
	{
		value.push_back('0');
		temp = populateHuffmanTable(frequencies, rootNode->left, value);
		value.pop_back();

		if (!rootNode->left && !rootNode->right)
		{
			rootNode->compressed_length = value.size();
			rootNode->compressed = value;
			temp += value.size() * rootNode->frequency;
		}

		value.push_back('1');
		temp += populateHuffmanTable(frequencies, rootNode->right, value);
		value.pop_back();
	}
	return temp;
}
