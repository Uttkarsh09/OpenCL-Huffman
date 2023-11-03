#include "configuration.hpp"
#include "decompress.hpp"
#include "compress.hpp"


using namespace std;

int main(void)
{
	compressController("./test/testnew.txt");
	// decompressController("./test/book.txt.huff");
	return EXIT_SUCCESS;
}