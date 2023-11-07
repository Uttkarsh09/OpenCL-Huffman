#include "compress.hpp"
#include <string>

void writeHeaderSizeLength(ofstream &file_ptr, size_t number)
{
    string numString = to_string(number);
    unsigned char segment_size_length = (unsigned char)numString.size();
    file_ptr.put(segment_size_length);
    for (char digit : numString)
    {
        file_ptr.put(digit);
    }
}

void compressController(string original_file_path)
{
    MyLogger *l = MyLogger::GetInstance("../CPU/logs/compressCPU_logs.log");
    l->logIt(l->LOG_INFO, "Initialization done...");

    ifstream file(original_file_path);
    if (!file.is_open())
    {
        l->logIt(l->LOG_ERROR, "Failed to open file %s", original_file_path);
        exit(EXIT_FAILURE);
    }

    string file_ptr((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());

    if (file_ptr.empty())
    {
        l->logIt(l->LOG_ERROR, "Failed to read contents of file %s in file %s:%d ", original_file_path, __FILE__, __LINE__);
        exit(EXIT_FAILURE);
    }
    size_t file_size = file_ptr.size();
    l->logIt(l->LOG_INFO, "file size = %d", file_size);

    // cout<<file_ptr;
    string file_content = file_ptr;

    // -------------------------------------------------------------------------------------------------------------------
    // Frequency Count
    // -------------------------------------------------------------------------------------------------------------------

    puts("IN FREQ COUNT");

    int char_frequencies[TOTAL_CHARS];
    memset(char_frequencies, 0, TOTAL_CHARS * sizeof(int));

    for (char ch : file_content)
    {
        char_frequencies[static_cast<unsigned char>(ch)]++;
    }

    // for (int i = 0; i < TOTAL_CHARS; i++)
    // {
    //     cout << i << " - " << char_frequencies[i] << endl;
    // }

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
    l->logIt(l->LOG_INFO, "Unique characters = %d", unique_charactes);
    l->logIt(l->LOG_INFO, "Character frequencies before sorting");
    for (HuffNode *node : total_frequencies)
    {
        l->logIt(l->LOG_INFO, "Character: %c, frequency: %d", node->ch, node->frequency);
    }
    // -------------------------------------------------------------------------------------------------------------------
    // Huffman Tree & Codes
    // -------------------------------------------------------------------------------------------------------------------
    puts("IN HUFFMAN TREES & CODES");

    HuffNode *rootNode = generateHuffmanTree(total_frequencies);
    long compressed_file_size_bits = 0, compressed_file_size_bytes = 0;
    string tempStr = "";

    compressed_file_size_bits = populateHuffmanTable(&total_frequencies, rootNode, tempStr);
    compressed_file_size_bytes = (compressed_file_size_bits / 8) + ((compressed_file_size_bits % 8) != 0);

    l->logIt(l->LOG_INFO, "Original File Size -> %d bytes", file_size);
    l->logIt(l->LOG_INFO, "compressed file size = %d bits", compressed_file_size_bits);
    l->logIt(l->LOG_INFO, "compressed file size = %d bytes", compressed_file_size_bytes);
    l->logIt(l->LOG_INFO, "i.e %d bytes + %d bits", compressed_file_size_bits / 8, compressed_file_size_bits % 8);

    char huffman_codes[TOTAL_CHARS][MAX_HUFF_ENCODING_LENGTH + 1];

    memset(huffman_codes, '\0', TOTAL_CHARS * sizeof(char) * (MAX_HUFF_ENCODING_LENGTH + 1));

    for (HuffNode *node : total_frequencies)
    {
        strcpy(
            huffman_codes[(int)node->ch], // destination
            node->compressed.c_str()      // source
        );
    }

    for (int i = 0; i < TOTAL_CHARS; i++)
    {
        if (huffman_codes[i][0] != '\0')
        {
            l->logIt(l->LOG_INFO, "huffman_codes[%d] (%c) = %s", i, (char)i, huffman_codes[i]);
        }
    }

    // -------------------------------------------------------------------------------------------------------------------
    // Encoding and Prefix Sum
    // -------------------------------------------------------------------------------------------------------------------
    puts("IN ENCODING & PREFIX SUM");
    // NOTE: prefix_sums will contain the number of bits for each char not the actual sum
    long *prefix_sums = new long[file_size];
    int offset = 0;
    for (char ch : file_content)
    {
        string huffCode = huffman_codes[ch];
        prefix_sums[offset] = huffCode.size();
        offset++;
    }

    // for (int i = 0; i < file_size; i++)
    // {
    //     cout << prefix_sums[i] << " ";
    // }
    // -------------------------------------------------------------------------------------------------------------------
    // Gap Arrays
    // -------------------------------------------------------------------------------------------------------------------
    puts("IN GAP ARRAYS");

    // Note: (compressed_file_size_bits%SEGMENT_SIZE != 0) adds 1 if not properly divisible.
    unsigned int gap_array_size = (compressed_file_size_bits / SEGMENT_SIZE) + (compressed_file_size_bits % SEGMENT_SIZE != 0);
    unsigned int prefix_sums_size = file_size;
    unsigned int gap_idx = 1;
    long next_limit = 0, sum = 0;
    short *gap_array = new short[gap_array_size];

    gap_array[0] = 0;
    next_limit = SEGMENT_SIZE * gap_idx;

    l->logIt(l->LOG_DEBUG, "gap_array_size=%d", gap_array_size);

    for (unsigned int i = 0; i < prefix_sums_size - 1; i++)
    {
        sum += prefix_sums[i];
        if (sum >= next_limit)
        {
            gap_array[gap_idx++] = sum - next_limit;
            next_limit = SEGMENT_SIZE * gap_idx;
        }
    }

    for (unsigned int i = 0; i < gap_array_size; i++)
    {
        l->logIt(l->LOG_INFO, "gap_arr[%d]->%d", i, gap_array[i]);
    }
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

    if (!output_file.is_open())
    {
        l->logIt(l->LOG_ERROR, "COULD NOT OPEN OUTPUT FILE");
        exit(EXIT_FAILURE);
    }
    puts("Opening Output file");
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

    for (unsigned int i = 0; i < gap_array_size; i++)
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

    string compressedData = "";
    unsigned char ch = 0;
    short counter = 0;
    bool write_remaining = false;
    short next_bit;
    for (char chh : file_content)
    {
        string huffCode = huffman_codes[chh];

        for (int i = 0; i < huffCode.size(); i++)
        {
            write_remaining = true;
            next_bit = (int)huffCode[i] - 48;
            ch = ch << 1;
            ch ^= next_bit;
            ++counter;

            if (counter == 8)
            {
                write_remaining = false;
                compressedData += (unsigned char)ch;
                ch ^= ch;
                counter = 0;
            }
        }
    }
    if (write_remaining == true)
    {
        ch = ch << (padding);
        compressedData += (unsigned char)ch;
    }
    output_file << compressedData;
    output_file.close();

    // -------------------------------------------------------------------------------------------------------------------
    puts("DONE WRITING");

    delete[] gap_array;
    delete[] prefix_sums;
    gap_array = nullptr;
    prefix_sums = nullptr;

    l->deleteInstance();
}