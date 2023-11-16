__kernel void huffDecompress(__global char *compressed_buffer,
                             __global char *decompressed_buffer,
                             __global char *huff_tree, __global short *gap_arr,
                             __global int *global_counter,
                             __global int *global_decompressed_offset,
                             const int compressed_data_size_bits,
                             const int segment_size, const int total_segments) {

  int global_id = get_global_id(0);

  if (global_id >= total_segments)
    return;
  ulong read_offset;
  ulong limit =
      (global_id < total_segments - 1)
          ? (global_id * segment_size) + segment_size + gap_arr[global_id + 1]
          : compressed_data_size_bits;
  if (global_id == 0) {
    read_offset = 0;
  } else {
    read_offset = ((global_id - 1) * segment_size) + segment_size +
                  gap_arr[global_id] ;
  }
  int decomp_index = 1;
  int decomp_buffer_offset = global_id * segment_size;
  int decoded_length = 0;
  char decoded_characters[512];

  // printf("gap_arr[%d] = %d\n", global_id, gap_arr[global_id]);
  // printf("Total segments = %d\n", total_segments);
  // printf("segment_size = %d\n", segment_size);
  printf("read_offset = %ld limit = %ld", read_offset, limit);
  // printf("limit = %ld ", limit);

  while (read_offset <= limit) {
    char bit =
        ((compressed_buffer[read_offset / 8] >> (7 - (read_offset % 8))) & 1) +
        '0';

    if (bit == '1') {
      decomp_index = (decomp_index * 2) + 1;
    } else {
      decomp_index *= 2;
    }

    if (huff_tree[decomp_index] != 0 && huff_tree[decomp_index] != 1) {
      char decoded_character = huff_tree[decomp_index];
      decoded_characters[decoded_length] = decoded_character;
      // printf("decoded char - %c", decoded_characters[decoded_length]);
      decoded_length++;
      decomp_index = 1;
    }

    read_offset++;
  }
  //  read_offset = limit;

  barrier(CLK_LOCAL_MEM_FENCE | CLK_GLOBAL_MEM_FENCE);

  while (*global_counter != total_segments) {
    if (*global_counter == global_id) {
      // printf("global_decompressed_offset = %d ID =
      // %d",*global_decompressed_offset,global_id); printf("decoded_length =
      // %d",decoded_length);
      short i = 0;
      while (i < decoded_length) {
        decompressed_buffer[*global_decompressed_offset] =
            decoded_characters[i];
        // printf("decompressed_buffer[*global_decompressed_offset] : %c",
        //  decompressed_buffer[*global_decompressed_offset]);
        ++i;
        // atomic_add(global_decompressed_offset,1);

        *global_decompressed_offset = *global_decompressed_offset + 1;
      }
      atomic_inc(global_counter);

      // *global_counter = *global_counter + 1;
    }
    barrier(CLK_LOCAL_MEM_FENCE | CLK_GLOBAL_MEM_FENCE);
  }
}
