__kernel void huffmanCompress(
	__global char *input_buffer, 
	__global uchar *huffman_codes,
	__global char *compressed_buffer,
	__global short *prefix_sums_buffer,
	__global int *global_counter,
	__global long *global_compressed_bits_written,
	const int segment_size,
	const int total_segments
) {
	uint global_id = get_global_id(0);

	if(global_id > total_segments)
		return;

	ulong read_offset = global_id * segment_size;
	ulong limit = read_offset + segment_size;
	ulong sum = 0;
	uchar encoded_characters[512 * 16], raw_char, compressed_char=0;
	uchar bits_to_shift=7;
	ushort i, length = 0, prefix_offset = read_offset;
	uint bits_written = 0;
	bool write_remaining = true;

	while(read_offset <= limit){
		raw_char = input_buffer[read_offset];
		i = 0;

		while(huffman_codes[(raw_char * 17) + i] != '\0' && i<=16){
			write_remaining = true;
			compressed_char = compressed_char | ((huffman_codes[(raw_char * 17) + i] - '0') << bits_to_shift);
			++bits_written;
			bits_to_shift = (bits_to_shift + 7) & 7;

			if(bits_to_shift == 7){
				write_remaining = false;
				encoded_characters[length] = compressed_char;
				compressed_char ^= compressed_char;
				++length;
			}
			++i;
		}
		sum += i;
		prefix_sums_buffer[prefix_offset++] = sum;
		++read_offset;
	}

	if(write_remaining == true){
		encoded_characters[length] = compressed_char;
	}

	i=0;

	barrier(CLK_LOCAL_MEM_FENCE | CLK_GLOBAL_MEM_FENCE);

	while(*global_counter != total_segments){
		if(*global_counter == global_id){
			length = 0;
			bits_to_shift = 7 - (*global_compressed_bits_written % 8);
			read_offset = *global_compressed_bits_written / 8;	
			compressed_char = compressed_buffer[read_offset];

			while(i <= bits_written){
				compressed_char = compressed_char | ((encoded_characters[i/8] - '0') << bits_to_shift);
				bits_to_shift = (bits_to_shift + 7) & 7;
				++length;

				if(bits_to_shift == 7){
					compressed_buffer[read_offset] = compressed_char;
					compressed_char ^= compressed_char;
					read_offset++;
				}
				++i;
			}

			*global_counter = *global_counter + 1;
			*global_compressed_bits_writteny = *global_compressed_bits_written + bits_written;

			for(int j=0 ; j<segment_size ; j++){
				prefix_sums_buffer[(segment_size * global_id) + j] += prefix_sums_buffer[segment_size * (global_id-1)];
			}
		}
		
		barrier(CLK_LOCAL_MEM_FENCE | CLK_GLOBAL_MEM_FENCE);
	}
}
