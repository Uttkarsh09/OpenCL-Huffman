__kernel void huffDecompress(
	__global char *compressed_buffer,
	__global char *decompressed_buffer,
	__global char *huff_tree, 
	__global char *gap_arr,
	__global int *global_counter,
	__global long *global_decompressed_offset,
	const int padding, 
	const int segment_size,
	const int total_segments
) {

	int global_id = get_global_id(0);

  	if (global_id > total_segments)
    	return;

  	ulong read_offset = (global_id * segment_size) + gap_arr[global_id];
  	ulong limit = (global_id < total_segments - 1)
                    	? (global_id * segment_size) + segment_size + gap_arr[global_id + 1]
                    	: (global_id * segment_size) + segment_size - padding;
	
	if(global_id != 0){
		++read_offset;
	}

	printf("%d. read_offset = %ld limit=%ld\n", global_id, read_offset, limit);

	int decomp_index = 1;
	int decomp_buffer_offset = global_id * segment_size;
	int decoded_length = 0;
	char decoded_characters[512];
	char decoded_character;

	while (read_offset <= limit) {
		char bit = ((compressed_buffer[read_offset / 8] >> (7 - (read_offset % 8))) & 1) + '0';

		if (bit == '1') {
			decomp_index = (decomp_index * 2) + 1;
		} 
		else {
			decomp_index *= 2;
		}

    	if (huff_tree[decomp_index] != 0 && huff_tree[decomp_index] != 1) {
      		decoded_character = huff_tree[decomp_index];
      		decoded_characters[decoded_length] = decoded_character;
			printf("%d. decoded_char = %c(%d)\n", global_id, decoded_character, (int)decoded_character);
      		decoded_length++;
      		decomp_index = 1;
    	}

    	read_offset++;
  	}

  	barrier(CLK_LOCAL_MEM_FENCE | CLK_GLOBAL_MEM_FENCE);
	
  	while (*global_counter != total_segments){
	  	if (*global_counter == global_id) {
			short i = 0;
			while (i < decoded_length) {
				decompressed_buffer[*global_decompressed_offset] = decoded_characters[i];
				++i;
				atomic_inc(global_decompressed_offset);
			}
			atomic_inc(global_counter);
		}
		barrier(CLK_LOCAL_MEM_FENCE | CLK_GLOBAL_MEM_FENCE);
	}

}
