#include "configuration.hpp"
#include "decompress.hpp"
#include "compress.hpp"


using namespace std;

int main(void)
{
	compressController("./test/numbers.txt");
	// decompressController("./test/numbers.txt.huff");

	// compressController("./test/random.txt");
	// compressController("./test/and_then_there_were_none.txt");
	// compressController("./test/processed.txt");
	// compressController("./test/small.txt");
	
	// compressController("./test/book.txt");
	// decompressController("./test/book.txt.huff");
	return EXIT_SUCCESS;
}