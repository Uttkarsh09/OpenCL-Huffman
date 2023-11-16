#include "decompress.hpp"

void readHeaderSizeLength(ifstream &file_ptr, size_t &len)
{
	len = 0;
	char temp_ch;
	short size;

	file_ptr.get(temp_ch);
	size = (short)temp_ch;

	while (size)
	{
		file_ptr.get(temp_ch);
		unsigned char digit = static_cast<unsigned char>(temp_ch);
		len = (len * 10) + (digit - '0');
		size--;
	}
}

void decompressController(string compressed_file_path)
{
	MyLogger *l = MyLogger::GetInstance("./logs/decompress_logs.log");

	l->logIt(l->LOG_INFO, "Initialization done...");

	ifstream input_file_ptr(compressed_file_path);
	if (!input_file_ptr.is_open())
	{
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
	memset(huff_tree_arr, 0, HUFF_TREE_MAX_NODE_COUNT);

	// Note: 0 is 1, 1 is 2 and so on... hence adding 1
	++unique_characters;

	char ch, len;
	string codes = "";
	int iterator = 1;

	// TODO: If working properly remove the 'codes' variable
	for (short i = 0; i < unique_characters; i++)
	{
		iterator = 1;
		input_file_ptr.get(ch);
		input_file_ptr.get(len);

		codes = "";

		while (len > codes.size())
		{
			input_file_ptr.get(temp_char);
			codes.push_back(temp_char);

			if (temp_char == '1')
			{
				huff_tree_arr[iterator] = 1;
				iterator = (iterator * 2) + 1;
			}
			else
			{
				iterator *= 2;
			}
		}
		l->logIt(l->LOG_DEBUG, "huffman_code[%d] (%c) {%d} = %s", (int)ch, ch, len, codes.c_str());
		huff_tree_arr[iterator] = ch;
	}
	// for (int i = 0; i < HUFF_TREE_MAX_NODE_COUNT; i++)
	// {
	// 	cout << i << "- " << huff_tree_arr[i] << endl;
	// }
	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Gap Array	 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	size_t gap_array_size;

	readHeaderSizeLength(input_file_ptr, gap_array_size);

	short *gap_array = new short[gap_array_size];

	for (cl_uint i = 0; i < gap_array_size; i++)
	{
		input_file_ptr.get(temp_char);
		gap_array[i] = (short)temp_char;
	}

	for (int i = 0; i < gap_array_size; i++)
	{
		l->logIt(l->LOG_DEBUG, "gap_array[%d] = %hu", i, gap_array[i]);
	}

	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Segment Size ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	size_t segment_size;

	readHeaderSizeLength(input_file_ptr, segment_size);
	l->logIt(l->LOG_DEBUG, "Segment Size = %zu", segment_size);
	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Padding ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	int padding;
	input_file_ptr.get(temp_char);
	padding = (int)temp_char;
	l->logIt(l->LOG_DEBUG, "padding = %zu", padding);

	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Uncompressed Data Size ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	size_t decompresseded_data_size;
	readHeaderSizeLength(input_file_ptr, decompresseded_data_size);
	l->logIt(l->LOG_DEBUG, "Decompressed Data Size = %zu", decompresseded_data_size);

	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Compressed Data ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	if (!input_file_ptr)
	{
		l->logIt(l->LOG_ERROR, "Could not read the whole file %s in %s:%d", compressed_file_path, __FILE__, __LINE__);
		exit(EXIT_FAILURE);
	}

	size_t start_position = input_file_ptr.tellg();
	size_t compressed_data_size_bytes;

	input_file_ptr.seekg(0, input_file_ptr.end);
	compressed_data_size_bytes = (size_t)input_file_ptr.tellg() - start_position;
	input_file_ptr.seekg(start_position);

	char *compressed_data = new char[compressed_data_size_bytes];
	input_file_ptr.read(compressed_data, compressed_data_size_bytes);
	l->logIt(l->LOG_INFO, "compressed data size calculated: %d", compressed_data_size_bytes);

	//  Logic to retrive the actual 0's and 1's

	// int write_offset = 0;
	// char bit = ((compressed_data[write_offset / 8] >> (7 - (write_offset % 8))) & 1) + '0';
	// cout << "BIT_ " << bit  << endl;

	// vector<char> compressed_bits;

	// for (size_t i = 0; i < compressed_data_size_bytes; ++i)
	// {
	// 	for (int j = 7; j >= 0; --j)
	// 	{
	// 		int bit = (compressed_data[i] >> j) & 1;
	// 		compressed_bits.push_back(static_cast<char>(bit + '0'));
	// 	}
	// }

	// l->logIt(l->LOG_DEBUG, "compressed bits size = %i", compressed_bits.size());

	// for (int i = 0; i < compressed_bits.size(); ++i)
	// {
	// 	std::cout << compressed_bits[i];
	// }
	// cout << "size - " << compressed_bits.size();

	// -------------------------------------------------------------------------------------------------------------------
	// Decompression
	// -------------------------------------------------------------------------------------------------------------------

	CLFW *clfw = new CLFW();
	clfw->ocl_initialize();

	// int global_work_size = (compressed_data_size_bytes / SEGMENT_SIZE) + ((compressed_data_size_bytes % SEGMENT_SIZE) != 0);
	int local_work_size = 256;
	int global_work_size = round_global_size(local_work_size, decompresseded_data_size);

	l->logIt(l->LOG_DEBUG, "writing global counter");
	cl_mem global_counter = clfw->ocl_create_buffer(CL_MEM_READ_WRITE, sizeof(cl_int));
	int zero = 0;
	clfw->ocl_write_buffer(global_counter, sizeof(cl_int), &zero);

	l->logIt(l->LOG_DEBUG, "writing global decompressed offset");
	cl_mem global_decompressed_offset = clfw->ocl_create_buffer(CL_MEM_READ_WRITE, sizeof(cl_long));
	clfw->ocl_write_buffer(global_decompressed_offset, sizeof(cl_int), &zero);

	l->logIt(l->LOG_DEBUG, "writing compressed data buffer");
	cl_mem compressed_data_buffer = clfw->ocl_create_buffer(CL_MEM_READ_ONLY, compressed_data_size_bytes * sizeof(char));
	clfw->ocl_write_buffer(compressed_data_buffer, compressed_data_size_bytes * sizeof(char), compressed_data);

	cl_mem decompressed_data_buffer = clfw->ocl_create_buffer(CL_MEM_WRITE_ONLY, decompresseded_data_size * sizeof(char));

	cl_mem huff_tree_arr_buffer = clfw->ocl_create_buffer(CL_MEM_READ_ONLY, HUFF_TREE_MAX_NODE_COUNT * sizeof(char));
	clfw->ocl_write_buffer(huff_tree_arr_buffer, HUFF_TREE_MAX_NODE_COUNT * sizeof(char), huff_tree_arr);

	cl_mem gap_array_buffer = clfw->ocl_create_buffer(CL_MEM_READ_ONLY, gap_array_size * sizeof(short));
	clfw->ocl_write_buffer(gap_array_buffer, gap_array_size * sizeof(short), gap_array);
	string ocl_decompression_kernel_path = "./src/oclKernels/Decompression.cl";
	clfw->ocl_create_program(ocl_decompression_kernel_path.c_str());

	int total_segments = (compressed_data_size_bytes * 8 / SEGMENT_SIZE) + (compressed_data_size_bytes * 8 % SEGMENT_SIZE != 0);

	int compressed_data_size_bits = (compressed_data_size_bytes * 8) - padding;

	// TODO: convert global_decompressed_offset variable to long = 'l' instead of 'i'
	clfw->ocl_create_kernel(
		"huffDecompress",
		"bbbbbbiii",
		compressed_data_buffer,
		decompressed_data_buffer,
		huff_tree_arr_buffer,
		gap_array_buffer,global_counter,
		global_decompressed_offset,
		compressed_data_size_bits,
		SEGMENT_SIZE,
		total_segments);

	clfw->ocl_execute_kernel(global_work_size, local_work_size);

	char *decompressed = new char[decompresseded_data_size];
	clfw->ocl_read_buffer(decompressed_data_buffer, decompresseded_data_size * sizeof(char), decompressed);

	ofstream outputFile("./test/decompressed.txt");
	if (outputFile.is_open())
	{
		for (int i = 0; i < decompresseded_data_size; i++)
		{
			outputFile << decompressed[i];
		}

		outputFile.close();
		cout << "Data written to the file successfully." << endl;
		l->logIt(l->LOG_INFO, "Data written to the file successfully.");
	}
	else
	{
		l->logIt(l->LOG_ERROR, "Unable to open the file!");
	}

	l->logIt(l->LOG_INFO, "Time Required For GPU(OpenCL) : %fms", clfw->ocl_gpu_time);

	clfw->ocl_release_buffer(compressed_data_buffer);
	clfw->ocl_release_buffer(decompressed_data_buffer);
	clfw->ocl_release_buffer(huff_tree_arr_buffer);
	clfw->ocl_release_buffer(gap_array_buffer);

	delete clfw;
	clfw = nullptr;

	// -------------------------------------------------------------------------------------------------------------------

	// free(huff_tree_arr);
	delete[] gap_array;
	gap_array = nullptr;

	l->deleteInstance();
}