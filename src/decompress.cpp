#include "decompress.hpp"

void readHeaderSizeLength(ifstream &file_ptr, size_t &len){
	len = 0;
	char temp_ch;
	short size;
	
	file_ptr.get(temp_ch);
	size = (short)temp_ch;

	while(size){
		file_ptr.get(temp_ch);
		len = len + (pow(10, size-1) * (int)temp_ch);
		--size;
	}
}

void decompressController(string compressed_file_path){
	MyLogger *l = MyLogger::GetInstance("./logs/decompress_logs.logs");

	l->logIt(l->LOG_INFO, "Initialization done...");

	ifstream input_file_ptr(compressed_file_path);
	if(! input_file_ptr.is_open()){
		l->logIt(l->LOG_ERROR, "Failed to open file %s", compressed_file_path);
		exit(EXIT_FAILURE);
	}

// -------------------------------------------------------------------------------------------------------------------
// Reading Header
// -------------------------------------------------------------------------------------------------------------------

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Huffman Info ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	
	short unique_characters;
	char temp_char;
	input_file_ptr.get(temp_char);
	unique_characters = (short)temp_char;
	char huff_tree_arr[HUFF_TREE_MAX_NODE_COUNT];

	// Note: 0 is 1, 1 is 2 and so on... hence adding 1
	++unique_characters;

	char ch, len;
	string codes = "";
	int iterator=1;

	// TODO: If working properly remove the 'codes' variable
	for(short i=0 ; i<unique_characters ; i++){
		iterator=1;
		input_file_ptr.get(ch);
		input_file_ptr.get(len);
		codes = "";

		while(len > codes.size()){
			input_file_ptr.get(temp_char);
			codes.push_back(temp_char);

			if(temp_char == '1'){
				huff_tree_arr[iterator] = 1;
				iterator = (iterator*2) + 1;
			}
			else {
				iterator *= 2;
			}
		}

		l->logIt(l->LOG_INFO, "%c Code = %s len=%d", ch, codes.c_str(), len);
		huff_tree_arr[iterator] = ch;
	}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Gap Array ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	
	input_file_ptr.get(temp_char);
	short gap_array_size_length = (int)temp_char;
	short segment_size_length;
	uint segment_length;
	size_t gap_array_size;

	readHeaderSizeLength(input_file_ptr, gap_array_size);
	short gap_array[gap_array_size];

	for(uint i=0 ; i<gap_array_size ; i++){
		input_file_ptr.get(temp_char);
		gap_array[i] = (short) temp_char;
	}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Segment Size ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	
	segment_size_length = (short)temp_char;
	size_t segment_size;

	readHeaderSizeLength(input_file_ptr, segment_size);

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Padding ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	
	short padding;
	input_file_ptr.get(temp_char);
	padding = (short) temp_char;

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Uncompressed Data Size ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	
	size_t decompresseded_data_size;
	readHeaderSizeLength(input_file_ptr, decompresseded_data_size);

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Compressed Data ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	
	size_t start_position = input_file_ptr.tellg();
	size_t compressed_data_size;

	input_file_ptr.seekg(0, input_file_ptr.end);
	compressed_data_size = (size_t)input_file_ptr.tellg() - start_position;
	input_file_ptr.seekg(start_position);

	char *compressed_data = new char[compressed_data_size];
	input_file_ptr.read(compressed_data, compressed_data_size);
	
	if(!input_file_ptr){
		l->logIt(l->LOG_ERROR, "Could not read the whole file %s in %s:%d", compressed_file_path, __FILE__, __LINE__);
		exit(EXIT_FAILURE);
	}

// -------------------------------------------------------------------------------------------------------------------
// Decompression
// -------------------------------------------------------------------------------------------------------------------

	CLFW *clfw = new CLFW();
	int global_work_size = (compressed_data_size / SEGMENT_SIZE) + ((compressed_data_size % SEGMENT_SIZE) != 0);
	int local_work_size = 256;

	cl_mem compressed_data_buffer = clfw->ocl_create_buffer(CL_MEM_READ_ONLY, compressed_data_size * sizeof(char));
	cl_mem decompressed_data_buffer = clfw->ocl_create_buffer(CL_MEM_WRITE_ONLY, decompresseded_data_size * sizeof(char));




// -------------------------------------------------------------------------------------------------------------------

	delete[] compressed_data;
	compressed_data = nullptr;

	free(huff_tree_arr);
	// huff_tree_arr = nullptr;

	l->deleteInstance();
}