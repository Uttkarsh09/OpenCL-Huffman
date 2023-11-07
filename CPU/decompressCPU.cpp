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

void huffDecompressCPU(const char *compressed_buffer, const char *huff_tree, int limit)
{
    int decomp_index = 1;
    int decoded_length = 0;
    string decoded_characters = "";

    int read_offset = 0;
    int decomp_buffer_offset = 0;

    while (read_offset < limit)
    {
        char bit = ((compressed_buffer[read_offset / 8] >> (7 - (read_offset % 8))) & 1) + '0';

        if (bit == '1')
        {
            decomp_index = (decomp_index * 2) + 1;
        }
        else
        {
            decomp_index *= 2;
        }

        if (huff_tree[decomp_index] != 0 && huff_tree[decomp_index] != 1)
        {
            decoded_characters += huff_tree[decomp_index];
            decomp_index = 1;
        }

        read_offset++;
    }

    ofstream outputFile("../CPU/test/decompressed.txt");
    if (outputFile.is_open())
    {
        outputFile << decoded_characters;
        outputFile.close();
        cout << "Data written to the file successfully." << endl;
    }
    else
    {
        cout << "Unable to open the file!";
    }
}

void decompressController(string compressed_file_path)
{
    MyLogger *l = MyLogger::GetInstance("../CPU/logs/decompressCPU_logs.log");

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
    puts("READING HUFFMAN INFO");

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

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Gap Array	 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    puts("READING GAP ARRAY");

    size_t gap_array_size;

    readHeaderSizeLength(input_file_ptr, gap_array_size);

    short *gap_array = new short[gap_array_size];

    for (int i = 0; i < gap_array_size; i++)
    {
        input_file_ptr.get(temp_char);
        gap_array[i] = (short)temp_char;
    }

    for (int i = 0; i < gap_array_size; i++)
    {
        l->logIt(l->LOG_DEBUG, "gap_array[%d] = %hu", i, gap_array[i]);
    }

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Segment Size ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    puts("READING SEGMENT SIZE");

    size_t segment_size;

    readHeaderSizeLength(input_file_ptr, segment_size);
    l->logIt(l->LOG_DEBUG, "Segment Size = %zu", segment_size);
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Padding ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    puts("READING PADDING");

    int padding;
    input_file_ptr.get(temp_char);
    padding = (int)temp_char;
    l->logIt(l->LOG_DEBUG, "padding = %zu", padding);

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Uncompressed Data Size ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    puts("READING UNCOMPRESSED DATA SIZE");

    size_t decompresseded_data_size;
    readHeaderSizeLength(input_file_ptr, decompresseded_data_size);
    l->logIt(l->LOG_DEBUG, "Decompressed Data Size = %zu", decompresseded_data_size);

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Compressed Data ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    puts("READING COMPRESSED DATA");

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

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Decoding Data ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    int limit = (compressed_data_size_bytes * 8) - padding;
    huffDecompressCPU(compressed_data, huff_tree_arr, limit);

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
    delete[] gap_array;
    gap_array = nullptr;
    l->deleteInstance();
    puts("EXITING......");
}