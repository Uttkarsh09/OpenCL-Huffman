#ifndef HUFF_MACROS
#define HUFF_MACROS


#define MAX_HUFF_ENCODING_LENGTH 16
#define TESTING 1

#ifndef TESTING 
#define SEGMENT_SIZE 16
#else
#define SEGMENT_SIZE 512
#endif


#endif // HUFF_MACROS
