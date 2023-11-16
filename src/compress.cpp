#include "compress.hpp"

void writeHeaderSizeLength(ofstream &file_ptr, size_t number)
{
	string numString = to_string(number);
	cl_uchar segment_size_length = (unsigned char)numString.size();
	file_ptr.put(segment_size_length);
	for (char digit : numString)
	{
		file_ptr.put(digit);
	}
}

void compressController(string original_file_path)
{
	CLFW *clfw = new CLFW();
	const int local_size = 32;
	MyLogger *l = MyLogger::GetInstance("./logs/compress_logs.log");

	clfw->ocl_initialize();
	l->logIt(l->LOG_INFO, "Initialization done ...");

	// ? Read file
	ifstream file(original_file_path);
	if (!file.is_open())
	{
		l->logIt(l->LOG_ERROR, "Failed to open file %s", original_file_path);
		exit(EXIT_FAILURE);
	}

	// ? Read file content
	string file_ptr((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());

	if (file_ptr.empty())
	{
		l->logIt(l->LOG_ERROR, "Failed to read contents of file %s in file %s:%d ", original_file_path, __FILE__, __LINE__);
		exit(EXIT_FAILURE);
	}

	// ? Read file size
	size_t file_size = file_ptr.size();
	// l->logIt(l->LOG_INFO, "file size = %d", file_size);

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

	l->logIt(l->LOG_INFO, "Time Required For frequency count : %fms", clfw->ocl_gpu_time);

	// Get char with frequencies > 0
	vector<HuffNode *> total_frequencies;
	int unique_charactes = 0;

	for (int i = 0; i < TOTAL_CHARS; i++)
	{
		if (char_frequencies[i] > 0)
		{
			total_frequencies.push_back(new HuffNode((char)i, char_frequencies[i]));
			++unique_charactes;
		}
	}

	// l->logIt(l->LOG_INFO, "Unique characters = %d", unique_charactes);
	// l->logIt(l->LOG_INFO, "Character frequencies before sorting");
	// for (HuffNode *node : total_frequencies)
	// {
	// 	l->logIt(l->LOG_INFO, "Character: %c, frequency: %d", node->ch, node->frequency);
	// }

	clfw->ocl_release_buffer(frequency_buffer);

// -------------------------------------------------------------------------------------------------------------------
// Huffman Tree & Codes
// -------------------------------------------------------------------------------------------------------------------
	puts("IN HUFFMAN TREES & CODES");

	HuffNode *rootNode = generateHuffmanTree(total_frequencies);
	cl_long compressed_file_size_bits = 0, compressed_file_size_bytes = 0;
	string tempStr = "";
	char huffman_codes[TOTAL_CHARS][MAX_HUFF_ENCODING_LENGTH + 1];

	compressed_file_size_bits = populateHuffmanTable(&total_frequencies, rootNode, tempStr);
	compressed_file_size_bytes = (compressed_file_size_bits / 8) + ((compressed_file_size_bits % 8) != 0);

	l->logIt(l->LOG_INFO, "Original File Size -> %d bytes", file_size);
	l->logIt(l->LOG_INFO, "compressed file size = %d bits", compressed_file_size_bits);
	l->logIt(l->LOG_INFO, "compressed file size = %d bytes", compressed_file_size_bytes);
	l->logIt(l->LOG_INFO, "i.e %d bytes + %d bits", compressed_file_size_bits / 8, compressed_file_size_bits % 8);

	memset(huffman_codes, '\0', TOTAL_CHARS * sizeof(char) * (MAX_HUFF_ENCODING_LENGTH + 1));

	for (HuffNode *node : total_frequencies){
		strcpy(
			huffman_codes[(int)node->ch], // destination
			node->compressed.c_str()	  // source
		);
	}

	for (int i = 0; i < TOTAL_CHARS; i++){	
		if (huffman_codes[i][0] != '\0'){
			l->logIt(l->LOG_INFO, "huffman_codes[%d] (%c) = %s", i, (char)i, huffman_codes[i]);
		}
	}

// -------------------------------------------------------------------------------------------------------------------
// Encoding and Prefix Sum
// -------------------------------------------------------------------------------------------------------------------
	puts("IN ENCODING & PREFIX SUM");

	cl_mem huffman_codes_buffer = clfw->ocl_create_buffer(CL_MEM_READ_ONLY, TOTAL_CHARS * sizeof(char) * (MAX_HUFF_ENCODING_LENGTH + 1));
	cl_mem compressed_buffer = clfw->ocl_create_buffer(CL_MEM_READ_WRITE, compressed_file_size_bytes);
	cl_mem prefix_sums_buffer = clfw->ocl_create_buffer(CL_MEM_READ_WRITE, file_size * sizeof(ulong));
	cl_mem global_counter = clfw->ocl_create_buffer(CL_MEM_READ_WRITE, sizeof(uint));
	string ocl_compression_kernel_path = "./src/oclKernels/Compression.cl";
	string ocl_parallel_prefix_sum_kernel_path = "./src/oclKernels/ParallelPrefixSum.cl";
	ulong *prefix_sums = new ulong[file_size];
	int total_segments = (file_size / SEGMENT_SIZE) + ((file_size % SEGMENT_SIZE) != 0);
	int zero = 0;
	unsigned char *compressed_output = NULL;
	cl_mem global_bits_written = clfw->ocl_create_buffer(CL_MEM_READ_WRITE, total_segments);

	clfw->host_alloc_mem((void**)&compressed_output, "uchar", compressed_file_size_bytes);

	clfw->ocl_write_buffer(global_counter, sizeof(uint), &zero);
	clfw->ocl_write_buffer(huffman_codes_buffer, sizeof(char) * TOTAL_CHARS * (MAX_HUFF_ENCODING_LENGTH + 1), huffman_codes);
	clfw->ocl_create_program(ocl_compression_kernel_path.c_str());
	
	l->logIt(l->LOG_INFO, "Creating the kernel");
	l->logIt(l->LOG_INFO, "file size = %ld", file_size);

	clfw->ocl_create_kernel(
		"huffmanCompress",
		"bbbbblii",
		file_buffer,
		huffman_codes_buffer,
		compressed_buffer,
		prefix_sums_buffer,
		global_bits_written,
		global_counter,
		file_size,
		SEGMENT_SIZE,
		total_segments
	);

	l->logIt(l->LOG_DEBUG, "total segments=%d", total_segments);
	l->logIt(l->LOG_DEBUG, "rounded=%d", round_global_size(local_size, total_segments));
	
	l->logIt(l->LOG_DEBUG, "Starting Kernel Execution");
	StopWatchInterface* timer = NULL;
	sdkCreateTimer(&timer);
	sdkStartTimer(&timer);
	float compressionTime = 0.0f;
	
	clfw->ocl_execute_kernel(round_global_size(local_size, total_segments), local_size);
	
	sdkStopTimer(&timer);
	compressionTime = sdkGetTimerValue(&timer);

	l->logIt(l->LOG_DEBUG, "reading compressed output");
	clfw->ocl_read_buffer(compressed_buffer, compressed_file_size_bytes, compressed_output);

	// for(int i=0 ; i<compressed_file_size_bytes ; i++){
	// 	l->logIt(l->LOG_DEBUG, "compressed values = %d", compressed_output[i]);
	// }

	// Note: prefix_sum only contains size of encoded bits.
	l->logIt(l->LOG_DEBUG, "reading prefix sum buffer");
	clfw->ocl_read_buffer(prefix_sums_buffer, file_size * sizeof(ulong), prefix_sums);

	// for (size_t i = 0; i < file_size; i++)
	// l->logIt(l->LOG_INFO, "%c->%s prefixSum->%d", file_ptr[i], compressed_output[i], prefix_sums[i]);

	clfw->ocl_release_buffer(file_buffer);
	clfw->ocl_release_buffer(prefix_sums_buffer);
	clfw->ocl_release_buffer(huffman_codes_buffer);
	clfw->ocl_release_buffer(compressed_buffer);
	clfw->ocl_release_buffer(global_bits_written);
	clfw->ocl_release_buffer(global_counter);

// -------------------------------------------------------------------------------------------------------------------
// Calculating Prefix Sum
// -------------------------------------------------------------------------------------------------------------------
	ulong limit, offset;

	// for(int i=0 ; i<512 ; i++){
	// 	l->logIt(l->LOG_CRITICAL, "prefix_sums[%d]=%d", i, prefix_sums[i]);
	// }

	for(int i=1 ; i<total_segments ; i++){
		offset = i*SEGMENT_SIZE;
		limit = (file_size<(offset+SEGMENT_SIZE)) 
				? SEGMENT_SIZE - ((offset + SEGMENT_SIZE) - file_size)
				: SEGMENT_SIZE;
		// l->logIt(l->LOG_DEBUG, "offset=%ld", offset);
		// l->logIt(l->LOG_DEBUG, "limit=%ld\n", limit);

		for(int j=0 ; j<limit ; j++){
			prefix_sums[offset+j] += prefix_sums[(SEGMENT_SIZE * i)-1];
			// l->logIt(l->LOG_DEBUG, "prefix_sums[%d]=%d", offset+j, prefix_sums[offset+j]);
		}
	}

// -------------------------------------------------------------------------------------------------------------------
// Gap Arrays
// -------------------------------------------------------------------------------------------------------------------
	puts("\nIN GAP ARRAYS");

	// Note: (compressed_file_size_bits%SEGMENT_SIZE != 0) adds 1 if not properly divisible.
	cl_uint gap_array_size = (compressed_file_size_bits / SEGMENT_SIZE) + (compressed_file_size_bits % SEGMENT_SIZE != 0);
	cl_uint prefix_sums_size = file_size;
	cl_uint gap_idx = 1;
	cl_ulong next_limit = 0;
	short *gap_array = new short[gap_array_size];

	gap_array[0] = 0;
	next_limit = SEGMENT_SIZE * gap_idx;

	l->logIt(l->LOG_DEBUG, "gap_array_size=%d", gap_array_size);

	for (unsigned int i = 0; i < prefix_sums_size - 1; i++)
	{
		if (prefix_sums[i] >= next_limit)
		{
			gap_array[gap_idx] = prefix_sums[i] - next_limit;
			// l->logIt(l->LOG_DEBUG, "gap_array[%d] = %ld", gap_idx, gap_array[gap_idx]);
			++gap_idx;
			next_limit = SEGMENT_SIZE * gap_idx;
		}
	}

	delete[] prefix_sums;
	prefix_sums = nullptr;
// -------------------------------------------------------------------------------------------------------------------
// Writing Compressed file
// -------------------------------------------------------------------------------------------------------------------
	puts("IN WRITING COMPRESSED FILE");

	char unique_characters = 255;
	string output_file_name = original_file_path + ".huff";
	string output_header_buffer = "";
	ofstream output_file;

	puts("Opening Output file");
	output_file.open(output_file_name, ios::out | ios::trunc);
	
	if(!output_file.is_open()){
		l->logIt(l->LOG_ERROR, "COULD NOT OPEN OUTPUT FILE");
		exit(EXIT_FAILURE);
	}
	puts("Opened Output file");

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Huffman Info ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	puts("WRITING HUFFMAN INFO");
	// ? UniqueCharacter start from -1 {0 means 1, 1 means 2, to conserve memory}
	for (size_t i = 0; i < total_frequencies.size(); i++)
	{
		output_header_buffer.push_back((int)total_frequencies[i]->ch);
		output_header_buffer.push_back(total_frequencies[i]->compressed.length());
		output_header_buffer += total_frequencies[i]->compressed;
		++unique_characters;
	}

	output_file.put((unsigned char)unique_characters);
	output_file << output_header_buffer;
	output_header_buffer = "";

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Gap Array ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	puts("WRITING GAP ARRAY");
	writeHeaderSizeLength(output_file, gap_array_size);

	for (cl_uint i = 0; i < gap_array_size; i++)
	{
		output_header_buffer.push_back(gap_array[i]);
	}

	output_file << output_header_buffer;
	output_header_buffer = "";

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Segment Size ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	puts("WRITING SEGMENT SIZE");
	
	writeHeaderSizeLength(output_file, SEGMENT_SIZE);

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Padding ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	puts("WRITING PADDING");

	short padding = (8 - ((compressed_file_size_bits) & (7))) & (7);
	output_file.put((unsigned char)padding);

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Data Size ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	puts("WRITING DATA SIZE");

	writeHeaderSizeLength(output_file, file_size);

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Compressed Data ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	puts("WRITING COMPRESSED DATA");

	for(int i=0 ; i<compressed_file_size_bytes ; i++){
		output_header_buffer += compressed_output[i];
		// l->logIt(l->LOG_DEBUG, "writing %d", compressed_output[i]);
	}

	output_file << output_header_buffer;
	output_header_buffer = "";
	output_file.close();

// -------------------------------------------------------------------------------------------------------------------
	l->logIt(l->LOG_DEBUG, "Time taken from compression: %fms", compressionTime);

	puts("DONE WRITING");

	sdkDeleteTimer(&timer);
	timer = NULL;

	delete[] gap_array;
	delete[] compressed_output;

	gap_array = nullptr;
	compressed_output = nullptr;

	clfw->ocl_uninitialize();
	l->deleteInstance();

	delete clfw;
	clfw = nullptr;

	puts("Finished compression successfully !");
}