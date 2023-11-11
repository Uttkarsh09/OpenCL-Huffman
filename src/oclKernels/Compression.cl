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
	uchar bits_to_shift=7, target_bit=0, enc_char_target_bit=0;
	ushort i, length = 0, prefix_offset = read_offset;
	uint bits_written = 0;
	bool write_remaining = true;

	if(input_file_size < limit){
		limit = input_file_size;
	}

	// printf("%d. offset=%ld limit=%ld\n", global_id, read_offset, limit);
	// printf("input_file_size = %ld\n", input_file_size);
	// printf("Total segments = %d\n", total_segments);
	// printf("segment_size = %d\n", segment_size);
	// printf("limit = %d\n", limit);

	while(read_offset <= limit){
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
		sum += i;
		// printf("%d. bits_written=%d\n", global_id, i);
		prefix_sums_buffer[prefix_offset] = sum;
		++read_offset;
		++prefix_offset;
	}

	if(write_remaining == true){
		// printf("%d. ~~~~~~~~ remaining compressed writing~~~~~~~~%d\n", global_id, compressed_char);
		bits_to_shift = (bits_to_shift + 7) & 7;
		// printf("%d. bits to shift = %d\n", global_id, bits_to_shift);
		encoded_characters[length] = compressed_char;
	}

	// printf("%d. bits_written = %d\n", global_id, bits_written);

	i=0;

	barrier(CLK_GLOBAL_MEM_FENCE);

	ulong write_offset = 0;
	uchar encoded_char_bits_to_shift = 7;

	while((*global_counter) != total_segments){
		// printf("%d. global_counter = %d total_segments = %d\n", global_id, *global_counter, total_segments);
		if((*global_counter) == global_id){
			printf("%d. global_counter=%d total_Segments=%d\n", global_id, *global_counter, total_segments);
			// printf("global_compressed_bits_written = %lld\n", *global_compressed_bits_written);
			length = 0;
			write_remaining = true;
			bits_to_shift = 7 - ((*global_compressed_bits_written) % 8);
			write_offset = (*global_compressed_bits_written) / 8;
			compressed_char = compressed_buffer[write_offset];
			
			// printf("%d. start Bits to shift  = %d\n", global_id, bits_to_shift);
			// printf("%d. start global_compressed_bits_written = %d\n", global_id, *global_compressed_bits_written);
			// printf("%d. start write_offset = %ld\n", global_id, write_offset);
			// printf("%d. start compressed_char = %d\n\n", global_id, compressed_char);

			while(i < bits_written){
				write_remaining = true;

				if(bits_to_shift == encoded_char_bits_to_shift){
					target_bit = (1 << bits_to_shift) & encoded_characters[i/8];
					compressed_char = compressed_char | target_bit;
				}
				else if(bits_to_shift > encoded_char_bits_to_shift){
					enc_char_target_bit = 1 << encoded_char_bits_to_shift;
					enc_char_target_bit = enc_char_target_bit & encoded_characters[i/8];
					target_bit = enc_char_target_bit << bits_to_shift - encoded_char_bits_to_shift;
					compressed_char = compressed_char | target_bit;
					// printf("shifting to left = %d\n", encoded_char_bits_to_shift - bits_to_shift);
				}
				else {
					enc_char_target_bit = 1 << encoded_char_bits_to_shift;
					enc_char_target_bit = enc_char_target_bit & encoded_characters[i/8];
					target_bit = enc_char_target_bit >> encoded_char_bits_to_shift - bits_to_shift;
					compressed_char = compressed_char | target_bit;
					// printf("shifting to right = %d\n", encoded_char_bits_to_shift - bits_to_shift);
					// printf("enc_char_target_bit = %d\n", enc_char_target_bit);
				}
				
				// printf("i = %d %d\n", i, encoded_characters[i/8]);
				// printf("target_bit = %d\n", target_bit);
				// printf("compressed_char = %d\n", compressed_char);
				// printf("Bits to shift  = %d\n", bits_to_shift);
				// printf("encoded_char_bits_to_shift  = %d\n\n", encoded_char_bits_to_shift);
				
				bits_to_shift = (bits_to_shift + 7) & 7;
				encoded_char_bits_to_shift = (encoded_char_bits_to_shift + 7) & 7;
				++length;

				if(bits_to_shift == 7){
					write_remaining = false;
					compressed_buffer[write_offset] = compressed_char;
					// printf("%d. ~~~~~~~~ writing~~~~~~~~%c(%d)\n", global_id, compressed_char, (int)compressed_char);
					compressed_char ^= compressed_char;
					++write_offset;
					// printf("%d. write_offset = %d\n", global_id write_offset);
				}
				++i;
			}

			if(write_remaining){
				// printf("%d. remaining - writing %c(%d)\n", global_id, compressed_char, (int)compressed_char);
				compressed_buffer[write_offset] = compressed_char;
			}
			
			// printf("increasing global_counter\n");
			atomic_inc(global_counter);
			atomic_add(global_compressed_bits_written, bits_written);
			printf("global_compressed_bits_written=%ld\n", *global_compressed_bits_written);

			// printf("%d. last Bits to shift  = %d\n", global_id, bits_to_shift);
			// printf("%d. last write_offset = %ld\n", global_id, write_offset);
			// printf("%d. Last written compressed char = %d\n", global_id, compressed_char);
			// printf("%d. last global_compressed_bits_written = %d\n\n\n", global_id, *global_compressed_bits_written);

			write_offset = (global_id * segment_size) + (global_id!=0); // ? (global_id!=0) will add 1 when global_id is not 0
			limit = write_offset + segment_size;
			printf("1");

			if(input_file_size < limit){
				limit = input_file_size;
			}
			printf("2");

			if(global_id != 0){
				printf("3");
				int size = limit - write_offset;
				// printf("Previous = %ld\n", prefix_sums_buffer[segment_size * global_id]);

				for(int j=write_offset ; j<=limit ; j++){
					prefix_sums_buffer[j] += prefix_sums_buffer[segment_size * global_id];
					// printf("%d. prefix_sum[%d] = %ld\n", global_id, j, prefix_sums_buffer[j]);
				}
				printf("4");
			}
			printf("5\n");
		}
		
		barrier(CLK_GLOBAL_MEM_FENCE);
	}
}
