__kernel 
void countCharFrequency(
	__global const char *input_data,
	__global int *char_frequency,
	const int input_length
) {
  int gid = get_global_id(0);

  if (gid >= input_length) {
    return;
  }

  char c = input_data[gid];

  if (c >= 0 && c < 256) {
	// ! FAILING
    atomic_inc(&char_frequency[c]);
  }
  
}
