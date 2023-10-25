__kernel void huffmanCompress(
	__global char *input, 
	__global char *huffman_codes,
	__global char *compressed_data,
	__global ulong *prefix_sums,
	const int input_size
) {
	int gid = get_global_id(0);

  	if (gid < input_size) {
    	char symbol = input[gid];
		short i=0;

		do{
			compressed_data[(17*gid) + i] = huffman_codes[((int)symbol*17) + i];
			++i;
		} while(huffman_codes[((int)symbol*17) + i] != '\0');
		
		// here i basically accounts for the size of encoding + 1 after the loop ends
		// hence i = no. of digits
		prefix_sums[gid] = i;
	}
}
