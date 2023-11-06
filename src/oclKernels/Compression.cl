#pragma OPENCL EXTENSION cl_khr_int64_base_atomics : enable

__kernel void huffmanCompress(
	__global char *input_buffer, 
	__global uchar *huffman_codes,
	__global uchar *compressed_buffer,
	__global ulong *prefix_sums_buffer,
	__global uint *global_counter,
	__global uint *global_compressed_bits_written,
	const long input_file_size,
	const int segment_size,
	const int total_segments
) {
	uint global_id = get_global_id(0);

	if(global_id >= total_segments)
		return;

	ulong read_offset = (global_id * segment_size) + (global_id!=0); // ? (global_id!=0) will add 1 when global_id is not 0
	ulong limit = read_offset + segment_size, sum=0;

	uchar encoded_characters[512 * 16], raw_char, compressed_char=0;
	uchar bits_to_shift=7, target_bit;
	ushort i, length = 0, prefix_offset = read_offset;
	uint bits_written = 0;
	bool write_remaining = true;

	if(input_file_size < limit){
		limit = input_file_size;
	}

	printf("%d. offset=%ld limit=%ld\n", global_id, read_offset, limit);
	// printf("input_file_size = %ld\n", input_file_size);
	// printf("Total segments = %d\n", total_segments);
	// printf("segment_size = %d\n", segment_size);
	printf("limit = %d\n", limit);

	while(read_offset <= limit){
		// ! ERROR
		raw_char = input_buffer[read_offset];
		i = 0;

		while(huffman_codes[(raw_char * 17) + i] != '\0' && i<=16){
			write_remaining = true;
			compressed_char = compressed_char | ((huffman_codes[(raw_char * 17) + i] - '0') << bits_to_shift);
			// printf("raw_char=%c bit=%d bits_to_shift=%d\n", raw_char, (huffman_codes[(raw_char * 17) + i] - '0'), bits_to_shift);
			// printf("compressed_char = %d\n", compressed_char);
			++bits_written;
			++i;
			bits_to_shift = (bits_to_shift + 7) & 7;

			if(bits_to_shift == 7){
				// printf("~~~~~~~~ writing~~~~~~~~%d\n", compressed_char);
				write_remaining = false;
				encoded_characters[length] = compressed_char;
				compressed_char ^= compressed_char;
				++length;
			}
		}
		// ! ERROR
		sum += i;
		// printf("%d. bits_written=%d\n", global_id, i);
		prefix_sums_buffer[prefix_offset] = sum;
		++read_offset;
		++prefix_offset;
	}

	if(write_remaining == true){
		encoded_characters[length] = compressed_char;
	}

	i=0;

	barrier(CLK_GLOBAL_MEM_FENCE);

	while((*global_counter) != total_segments){
		// printf("global_counter = %d global_id = %d total_segments = %d\n", *global_counter, global_id, total_segments);
		
		if((*global_counter) == global_id){
			// printf("IN here for global_counter=%d and global_id=%d\n", *global_counter, global_id);
			// printf("global_compressed_bits_written = %lld\n", *global_compressed_bits_written);
			length = 0;
			write_remaining = true;
			bits_to_shift = 7 - ((*global_compressed_bits_written) % 8);
			// printf("%d. start Bits to shift  = %d\n", global_id, bits_to_shift);
			
			read_offset = (*global_compressed_bits_written) / 8;

			// printf("%d. start global_compressed_bits_written = %d\n", global_id, *global_compressed_bits_written);
			// printf("%d. start read_offset = %ld\n", global_id, read_offset);
			
			compressed_char = compressed_buffer[read_offset];
			// printf("%d. start compressed_char = %d\n\n", global_id, compressed_char);

			while(i <= bits_written){
				write_remaining = true;
				// printf("i = %d %d", i, encoded_characters[i/8]);
				target_bit = (1 << bits_to_shift) & encoded_characters[i/8];
				// printf("target_bit = %d\n", target_bit);
				compressed_char = compressed_char | target_bit;
				// printf("compressed_char = %d\n", compressed_char);
				// printf("Bits to shift  = %d\n", bits_to_shift);
				bits_to_shift = (bits_to_shift + 7) & 7;
				++length;

				if(bits_to_shift == 7){
					write_remaining = false;
					compressed_buffer[read_offset] = compressed_char;
					// printf("~~~~~~~~ writing~~~~~~~~%d\n", compressed_char);
					compressed_char ^= compressed_char;
					++read_offset;
					// printf("read offset = %d\n", read_offset);
				}
				++i;
			}

			if(write_remaining){
				// printf("remaining - writing %d\n", compressed_char);
				compressed_buffer[read_offset] = compressed_char;
			}
			
			atomic_inc(global_counter);
			atomic_add(global_compressed_bits_written, bits_written);

			// printf("%d. last Bits to shift  = %d\n", global_id, bits_to_shift);
			// printf("%d. last read_offset = %ld\n", global_id, read_offset);
			// printf("%d. Last written compressed char = %d\n", global_id, compressed_char);
			// printf("%d. last global_compressed_bits_written = %d\n\n\n", global_id, *global_compressed_bits_written);

			read_offset = (global_id * segment_size) + (global_id!=0); // ? (global_id!=0) will add 1 when global_id is not 0
			limit = read_offset + segment_size;

			if(input_file_size < limit){
				limit = input_file_size;
			}

			if(global_id != 0){
				int size = limit - read_offset;
				printf("Previous = %ld\n", prefix_sums_buffer[segment_size * global_id]);

				for(int j=read_offset ; j<=limit ; j++){
					prefix_sums_buffer[j] += prefix_sums_buffer[segment_size * global_id];
					// printf("%d. prefix_sum[%d] = %ld\n", global_id, j, prefix_sums_buffer[j]);
				}
			}
		}
		
		barrier(CLK_GLOBAL_MEM_FENCE);
	}
}
