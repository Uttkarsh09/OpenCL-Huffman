__kernel void huffDecompress(__global char *compressed_buffer,
                             __global char *decompressed_buffer,
                             __global char *huff_tree, __global char *gap_arr,
                             const int padding, const int segment_size,
                             const int total_segments) {

  int gid = get_global_id(0);

  if (gid > total_segments)
    return;

  ulong write_offset = gid * segment_size + gap_arr[gid];
  ulong limit = (gid < total_segments - 1)
                    ? write_offset + segment_size + gap_arr[gid + 1]
                    : write_offset + segment_size - padding;
  int decomp_index = 1;
  int decomp_buffer_offset = gid * segment_size;
  while (write_offset < limit) {

    char bit =
        ((compressed_buffer[write_offset / 8] >> (7 - (write_offset % 8))) &
         1) +
        '0';

    if (bit == '1') {
      decomp_index = (decomp_index * 2) + 1;
    } else {
      decomp_index *= 2;
    }

    if (huff_tree[decomp_index] != '0' && huff_tree[decomp_index] != 1) {

      char decoded_character = huff_tree[decomp_index];
      decompressed_buffer[decomp_buffer_offset] = decoded_character;
      decomp_buffer_offset++;
      decomp_index = 1;
    }

    write_offset++;
  }
}
