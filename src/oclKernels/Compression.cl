#pragma OPENCL EXTENSION cl_khr_int64_base_atomics : enable

__kernel void huffmanCompress(
	__global char *input_buffer, 
	__global uchar *huffman_codes,
	__global uchar *compressed_buffer,
	__global ulong *prefix_sums_buffer,
	__global uint *global_bits_written,
	const long input_file_size,
	const int segment_size,
	const int total_segments
) {
	uint global_id = get_global_id(0);

	if(global_id >= total_segments)
		return;

	ulong read_offset = (global_id * segment_size); // ? (global_id!=0) will add 1 when global_id is not 0
	ulong limit = read_offset + segment_size;
	uchar encoded_characters[512 * 16], raw_char, compressed_char=0;
	uchar bits_to_shift=7, target_bit=0, enc_char_target_bit=0;
	ushort i, length = 0, prefix_offset = read_offset;
	uint bits_written = 0;
	bool write_remaining = true;
	ulong bits_written_till_now = 0;

	if(input_file_size < limit){
		limit = input_file_size;
	}

	// printf("%d. read_offset=%7ld limit=%7ld\n", global_id, read_offset, limit);
	// printf("%d. \n", global_id);


	while(read_offset < limit){
		raw_char = input_buffer[read_offset];
		i = 0;
		// printf("\nnew char=%d\n", raw_char);
		while(huffman_codes[(raw_char * 17) + i] != '\0'){
			write_remaining = true;
			compressed_char = compressed_char | ((huffman_codes[(raw_char * 17) + i] - '0') << bits_to_shift);
			// printf("raw_char=%c bit=%d bits_to_shift=%d\n", raw_char, (huffman_codes[(raw_char * 17) + i] - '0'), bits_to_shift);
			// printf("compressed_char = %d\n", compressed_char);
			// printf("%d. moved_bit: %d\n", global_id, i);
			++bits_written;
			++i;
			bits_to_shift = (bits_to_shift + 7) & 7;

			if(bits_to_shift == 7){
				// printf("%d. ~~~~~~~~ compressed writing ~~~~~~~~%d\n", global_id, compressed_char);
				write_remaining = false;
				encoded_characters[length] = compressed_char;
				compressed_char ^= compressed_char;
				++length;
			}
		}
		prefix_sums_buffer[prefix_offset] = bits_written;
		// printf("%d. prefix_sums[%d]=%d\n", global_id, prefix_offset, bits_written);
		++read_offset;
		++prefix_offset;
	}

	if(write_remaining == true){
		bits_to_shift = (bits_to_shift + 7) & 7;
		encoded_characters[length] = compressed_char;
	}


	global_bits_written[global_id] = bits_written;
	barrier(CLK_GLOBAL_MEM_FENCE | CLK_LOCAL_MEM_FENCE);

	if(global_id != 0){
		// printf("%d. \n", global_id);
		for(int j=0 ; j<global_id ; j++){
			bits_written_till_now += global_bits_written[j];
		}
	} 
	else {
		bits_written_till_now = 0;
	}

	i=0;
	ulong write_offset = 0;
	uchar encoded_char_bits_to_shift = 7;
	bits_to_shift = 7 - (bits_written_till_now % 8);
	write_offset = bits_written_till_now / 8;
	compressed_char = compressed_buffer[write_offset];

	// printf("%d. bits_to_shift=%d\n", global_id, bits_to_shift);
	printf("%d. write_offset=%ld bits_written_till_now=%ld bytes_written=%ld\n", global_id, write_offset, bits_written_till_now, bits_written/8);

	// TODO: If the output is synchronized then simply copy the array contents
	while(i < bits_written){
		write_remaining = true;

		if(bits_to_shift == encoded_char_bits_to_shift){
			target_bit = (1 << encoded_char_bits_to_shift) & encoded_characters[i/8];
			compressed_char |= target_bit;
		}
		else {
			enc_char_target_bit = 1 << encoded_char_bits_to_shift;
			enc_char_target_bit = enc_char_target_bit & encoded_characters[i/8];
		
			// a-b and b-a is more verbose to the condition, than abs(a-b)
			if(bits_to_shift > encoded_char_bits_to_shift){
				target_bit = enc_char_target_bit << (bits_to_shift - encoded_char_bits_to_shift);
			}
			else {
				target_bit = enc_char_target_bit >> (encoded_char_bits_to_shift - bits_to_shift);
			}
			
			compressed_char |= target_bit;
		}
		
		encoded_char_bits_to_shift = (encoded_char_bits_to_shift + 7) & 7;
		bits_to_shift = (bits_to_shift + 7) & 7;

		if(bits_to_shift == 7){
			write_remaining = false;
			compressed_buffer[write_offset] |= compressed_char;
			compressed_char ^= compressed_char;
			++write_offset;
		}

		++i;
	}

	if(write_remaining){
		compressed_buffer[write_offset] |= compressed_char;
	}

	// printf("%d. Stopping writing at: %ld \nbts: %d\n", global_id, write_offset, bits_to_shift);
	printf("%-2d. Done\n", global_id);
}
