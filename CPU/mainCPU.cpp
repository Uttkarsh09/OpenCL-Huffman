#include "configuration.hpp"
#include "decompress.hpp"
#include "compress.hpp"


using namespace std;

int main(void)
{
	compressController("../CPU/test/book.txt");
	decompressController("../CPU/test/book.txt.huff");
	return EXIT_SUCCESS;
}