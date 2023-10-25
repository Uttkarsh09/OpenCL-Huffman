#include "decompress.hpp"

void decodeHeaderSizeLength(ifstream &file_ptr, short size, uint &len){
	len = 0;
	char temp_ch;

	while(size){
		file_ptr.get(temp_ch);
		len = len + (pow(10, size-1) * (int)temp_ch);
		--size;
	}
}

void decompressController(string compressed_file_path){
	CLFW *clfw = nullptr;
	clfw = new CLFW();
	MyLogger *l = MyLogger::GetInstance("./logs/decompress_logs.logs");

	clfw->ocl_initialize();
	l->logIt(l->LOG_INFO, "Initialization done...");

	ifstream file_ptr(compressed_file_path);
	if(! file_ptr.is_open()){
		l->logIt(l->LOG_ERROR, "Failed to open file %s", compressed_file_path);
		exit(EXIT_FAILURE);
	}

// -------------------------------------------------------------------------------------------------------------------
// Reading Header
// -------------------------------------------------------------------------------------------------------------------

	short unique_characters;
	char temp_char;
	file_ptr.get(temp_char);
	unique_characters = (short)temp_char;

	// Note: 0 is 1, 1 is 2 and so on... hence adding 1
	++unique_characters;

	char ch, len;
	string codes = "";
	HuffNode *root = new HuffNode(0);
	
	for(short i=0 ; i<unique_characters ; i++){
		file_ptr.get(ch);
		file_ptr.get(len);
		codes = "";

		while(len > codes.size()){
			file_ptr.get(temp_char);
			codes.push_back(temp_char);
		}

		l->logIt(l->LOG_INFO, "%c Code = %s len=%d", ch, codes.c_str(), len);

		generateHuffmanTree(root, codes, ch);
	}

	file_ptr.get(temp_char);
	short gap_array_size_length = (int)temp_char;
	short segment_size_length;
	uint segment_length;
	uint gap_array_size;

	decodeHeaderSizeLength(file_ptr, gap_array_size_length, gap_array_size);
	short gap_array[gap_array_size];

	for(uint i=0 ; i<gap_array_size ; i++){
		file_ptr.get(temp_char);
		gap_array[i] = (short) temp_char;
	}

	file_ptr.get(temp_char);
	segment_size_length = (short)temp_char;
	uint segment_size;

	decodeHeaderSizeLength(file_ptr, segment_size_length, segment_size);

	short padding;
	file_ptr.get(temp_char);
	padding = (short) temp_char;

	

// -------------------------------------------------------------------------------------------------------------------
// Decompression
// -------------------------------------------------------------------------------------------------------------------



}