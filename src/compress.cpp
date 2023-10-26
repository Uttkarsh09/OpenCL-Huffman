#include "compress.hpp"

void writeHeaderSizeLength(ofstream &file_ptr, size_t length){
	cl_uchar segment_size_length = (unsigned char) to_string(length).size();
	cl_uint temp_segment_size = SEGMENT_SIZE;
	file_ptr.put(segment_size_length);
	string buffer = "";
	ushort last;
	
	while(temp_segment_size != 0){
		last = temp_segment_size % 10;
		buffer = to_string(last) + buffer;
		temp_segment_size /= 10;
	}
	
	file_ptr << buffer;
}

void compressController(string original_file_path){
	CLFW *clfw = new CLFW();
	const int local_size = 5;
	MyLogger *l = MyLogger::GetInstance("./logs/compress_logs.log");

	clfw->ocl_initialize();
	l->logIt(l->LOG_INFO, "Initialization done ...");

	// ? Read file
	ifstream file(original_file_path);
	if (!file.is_open()){
		l->logIt(l->LOG_ERROR, "Failed to open file %s", original_file_path);
		exit(EXIT_FAILURE);
	}

	// ? Read file content
	string file_ptr((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());

	if (file_ptr.empty()) {
		l->logIt(l->LOG_ERROR, "Failed to read contents of file %s in file %s:%d ", original_file_path, __FILE__, __LINE__);
		exit(EXIT_FAILURE);
	}
	
	// ? Read file size
	size_t file_size = file_ptr.size();
	l->logIt(l->LOG_INFO, "file size = %d", file_size);

	// ? Create buffers for file content and frequency
	cl_mem file_buffer = clfw->ocl_create_buffer(CL_MEM_READ_ONLY, file_size * sizeof(char));
	cl_mem frequency_buffer = clfw->ocl_create_buffer(CL_MEM_WRITE_ONLY, TOTAL_CHARS * sizeof(int));
	int initial_frequency[TOTAL_CHARS];

	memset(initial_frequency, 0, TOTAL_CHARS * sizeof(int));
	
	clfw->ocl_write_buffer(file_buffer, file_size, (char *)file_ptr.c_str());
	clfw->ocl_write_buffer(frequency_buffer, TOTAL_CHARS * sizeof(int), initial_frequency);

// -------------------------------------------------------------------------------------------------------------------
// Frequency Count
// -------------------------------------------------------------------------------------------------------------------
	puts("IN FREQ COUNT");

	string ocl_frequency_count_kernel_path = "./src/oclKernels/FrequencyCount.cl";
	clfw->ocl_create_program(ocl_frequency_count_kernel_path.c_str());
	clfw->ocl_create_kernel("countCharFrequency", "bbi", file_buffer, frequency_buffer, (int)file_size);
	clfw->ocl_execute_kernel(round_global_size(local_size, file_size), local_size);

	int char_frequencies[TOTAL_CHARS];
	clfw->ocl_read_buffer(frequency_buffer, TOTAL_CHARS * sizeof(int), char_frequencies);

	l->logIt(l->LOG_INFO, "Time Required For GPU(OpenCL) : %fms", clfw->ocl_gpu_time);
	
	// Get char with frequencies > 0
	vector<HuffNode*> total_frequencies;
	int unique_charactes = 0;

	for (int i=0; i < TOTAL_CHARS; i++){
		if (char_frequencies[i] > 0){
			total_frequencies.push_back(new HuffNode((char)i, char_frequencies[i]));
			++unique_charactes;
		}
	}
	l->logIt(l->LOG_INFO, "Unique characters = %d", unique_charactes);
	l->logIt(l->LOG_INFO, "Character frequencies before sorting");
	for (HuffNode *node : total_frequencies){
		l->logIt(l->LOG_INFO, "Character: %c, frequency: %d", node->ch, node->frequency);
	}

	clfw->ocl_release_buffer(frequency_buffer);

// -------------------------------------------------------------------------------------------------------------------
// Huffman Tree & Codes
// -------------------------------------------------------------------------------------------------------------------
	puts("IN HUFFMAN TREES & CODES");

	HuffNode *rootNode = generateHuffmanTree(total_frequencies);
	int compressed_file_size_bits = 0;
	string tempStr = "";

	compressed_file_size_bits = populateHuffmanTable(&total_frequencies, rootNode, tempStr);

	l->logIt(l->LOG_INFO, "Original File Size -> %d bytes", file_size);
	l->logIt(l->LOG_INFO, "compressed file size = %d bits", compressed_file_size_bits);
	l->logIt(l->LOG_INFO, "i.e %d bytes + %d bits", compressed_file_size_bits/8, compressed_file_size_bits%8);

	// cout << "After populateHuffmanTable" << endl;
	// for (HuffNode *node : total_frequencies)
	// 	cout << "Character: " << node->ch << ", compressed: " << node->compressed << ", Frequency: " << node->frequency << endl;

	char huffman_codes[TOTAL_CHARS][MAX_HUFF_ENCODING_LENGTH+1];

	memset(huffman_codes, '\0', TOTAL_CHARS * sizeof(char) * (MAX_HUFF_ENCODING_LENGTH+1));

	for (HuffNode *node : total_frequencies){
		strcpy(
			huffman_codes[(int)node->ch], 	// destination
			node->compressed.c_str()		// source
		);
	}

	for (int i = 0; i < TOTAL_CHARS; i++){
		if(huffman_codes[i][0] != '\0'){
			l->logIt(l->LOG_DEBUG, "huffman_codes[%d] = %s", i, huffman_codes[i]);
		}
	}

// -------------------------------------------------------------------------------------------------------------------
// Encoding and Prefix Sum
// -------------------------------------------------------------------------------------------------------------------
	puts("IN ENCODING & PREFIX SUM");

	cl_mem huffman_codes_buffer = clfw->ocl_create_buffer(CL_MEM_READ_ONLY, TOTAL_CHARS * sizeof(char) * (MAX_HUFF_ENCODING_LENGTH+1));
	cl_mem compressed_output_buffer = clfw->ocl_create_buffer(CL_MEM_WRITE_ONLY, file_size * sizeof(char) * (MAX_HUFF_ENCODING_LENGTH+1));
	cl_mem prefix_sums_buffer = clfw->ocl_create_buffer(CL_MEM_READ_WRITE, file_size * sizeof(cl_ulong));
	string ocl_compression_kernel_path = "./src/oclKernels/Compression.cl";
	string ocl_parallel_prefix_sum_kernel_path = "./src/oclKernels/ParallelPrefixSum.cl";
	char *compressed_output = new char [file_size * (MAX_HUFF_ENCODING_LENGTH + 1)];
	cl_ulong *prefix_sums = new cl_ulong [file_size];

	clfw->ocl_write_buffer(huffman_codes_buffer, sizeof(char) * TOTAL_CHARS * (MAX_HUFF_ENCODING_LENGTH+1), huffman_codes);
	clfw->ocl_create_program(ocl_compression_kernel_path.c_str());
	clfw->ocl_create_kernel(
		"huffmanCompress",
		"bbbbi",
		file_buffer,
		huffman_codes_buffer,
		compressed_output_buffer,
		prefix_sums_buffer,
		(int)file_size
	);
	clfw->ocl_execute_kernel(round_global_size(local_size, file_size), local_size);
	clfw->ocl_read_buffer(compressed_output_buffer, file_size * sizeof(char) * (MAX_HUFF_ENCODING_LENGTH+1), compressed_output);
	
	// Parallel Prefix Kernel
	// clfw->ocl_create_program(ocl_parallel_prefix_sum_kernel_path.c_str());
	// clfw->ocl_create_kernel(
	// 	"parallelPrefixSum",
	// 	"bi",
	// 	prefix_sums_buffer,
	// 	(int)file_size
	// );
	// clfw->ocl_execute_kernel(round_global_size(4, file_size), 4);
	
	// Note: prefix_sum only contains size of encoded bits.
	clfw->ocl_read_buffer(prefix_sums_buffer, file_size * sizeof(cl_ulong), prefix_sums);

	// for (size_t i = 0; i < file_size; i++){
		// l->logIt(l->LOG_INFO, "%c->%s prefixSum->%d", file_ptr[i], compressed_output[i], prefix_sums[i]);
	// }
	clfw->ocl_release_buffer(prefix_sums_buffer);
	clfw->ocl_release_buffer(huffman_codes_buffer);
	clfw->ocl_release_buffer(file_buffer);

// -------------------------------------------------------------------------------------------------------------------
// Gap Arrays
// -------------------------------------------------------------------------------------------------------------------
	puts("IN GAP ARRAYS");

	// Note: (compressed_file_size_bits%SEGMENT_SIZE != 0) adds 1 if not properly divisible.
	cl_uint gap_array_size = (compressed_file_size_bits / SEGMENT_SIZE) + (compressed_file_size_bits%SEGMENT_SIZE != 0);
	cl_uint prefix_sums_size = file_size;
	cl_uint gap_idx = 1;
	cl_ulong sum = 0, next_limit = 0;
	short *gap_array = new short[sizeof(short) * gap_array_size];

	gap_array[0] = 0;
	next_limit = SEGMENT_SIZE * gap_idx;

	for(cl_uint i=0 ; i<prefix_sums_size-1 ; i++){
		sum += prefix_sums[i];
		// l->logIt(l->LOG_INFO, "sum = %d", sum);
		if(sum >= next_limit){
			gap_array[gap_idx] = sum - next_limit;
			// l->logIt(l->LOG_INFO, "gap_array[%d] = %d", gap_idx, gap_array[gap_idx]);
			++gap_idx;
			next_limit = SEGMENT_SIZE * gap_idx;
		}
	}

	// for(cl_uint i=0 ; i<gap_array_size ; i++){
	// 	l->logIt(l->LOG_INFO, "gap_arr[%d]->%d", i, gap_array[i]);
	// }

// -------------------------------------------------------------------------------------------------------------------
// Writing Compressed file
// -------------------------------------------------------------------------------------------------------------------
	puts("IN WRITING COMPRESSED FILE");

    char unique_characters = 255;
	string output_file_name = original_file_path + ".huff";
	string output_header_buffer = "";
	ofstream output_file;
	
	output_file.open(output_file_name);
	
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Huffman Info ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	
	// ? UniqueCharacter start from -1 {0 means 1, 1 means 2, to conserve memory}
	for(size_t i=0 ; i<total_frequencies.size() ; i++){
		output_header_buffer.push_back((int)total_frequencies[i]->ch);
		output_header_buffer.push_back(total_frequencies[i]->compressed.length());
		output_header_buffer += total_frequencies[i]->compressed;
		++unique_characters;
	}
	
	output_file.put((unsigned char)unique_characters);
	output_file << output_header_buffer;
	output_header_buffer = "";

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Gap Array ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	writeHeaderSizeLength(output_file, gap_array_size);
	
	for(cl_uint i=0 ; i<gap_array_size ; i++){
		output_header_buffer.push_back(gap_array[i]);
	}

	output_file << output_header_buffer;
	output_header_buffer = "";

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Segment Size ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	writeHeaderSizeLength(output_file, SEGMENT_SIZE);

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Padding ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	short padding = (8 - ((compressed_file_size_bits) & (7))) & (7);
	output_file.put((unsigned char)padding);

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Data Size ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	writeHeaderSizeLength(output_file, file_size);

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Compressed Data ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	unsigned char ch = 0;
	short counter = 0;
	bool write_remaining = false;
	short next_bit;
	const short maxEncodingLength = MAX_HUFF_ENCODING_LENGTH+1;

	for(size_t i=0 ; i<file_size ; i++){
		short j = 0;
		
		while(compressed_output[(i*maxEncodingLength) + j] != '\0'){
			write_remaining = true;
			next_bit = (int)compressed_output[(i*maxEncodingLength)+j] - 48;
			ch = ch << 1;
			ch ^= next_bit;
			++counter;

			if(counter == 8){
				write_remaining = false;
				output_header_buffer += (unsigned char) ch;
				ch ^= ch;
				counter = 0;
			}

			++j;
		}
	}

	l->logIt(l->LOG_CRITICAL, "are padding and remaining bits matching = %d", (padding == 8-counter));
	l->logIt(l->LOG_CRITICAL, "padding: %d counter: %d", padding, counter);

	if(write_remaining == true){
		ch = ch << (padding);
		output_header_buffer += (unsigned char) ch;
	}

	output_file << output_header_buffer;
	output_header_buffer = "";
	output_file.close();

// -------------------------------------------------------------------------------------------------------------------

	delete[] gap_array;
	delete[] compressed_output;
	delete[] prefix_sums;

	gap_array = nullptr;
	compressed_output = nullptr;
	prefix_sums = nullptr;

	clfw->ocl_release_buffer(compressed_output_buffer);
	clfw->ocl_uninitialize();
	l->deleteInstance();

	delete clfw;
	clfw = nullptr;
}