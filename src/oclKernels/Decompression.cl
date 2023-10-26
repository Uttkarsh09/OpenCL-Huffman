__kernel void huffDecompress(
	__global char *compressed_buffer,
	__global char *decompressed_buffer,
	__global char *huff_tree,
	__global char *gap_arr,
	const short padding,
	const short segment_size,
	const uint total_segments
) {
	int gid = get_global_id(0);

	if(gid > total_segments){
		return;
	}

	ulong write_offset = (gid * segment_size) + gap_arr[gid];
	ulong limit;
	
	// Note: Purposely not merging the if condition, it is more verbose
	if(gid < total_segments){	
		limit = write_offset + segment_size + gap_arr[gid+1];
	} else {
		// ? for last segment
		limit = write_offset + segment_size;
	}
	
	int i = 1;
	while(write_offset <= limit){
		
	}

}