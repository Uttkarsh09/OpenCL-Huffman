#include "configuration.hpp"
#include "decompress.hpp"
#include "compress.hpp"


using namespace std;

int main(void)
{
	compressController("./test/numbers.txt");
	decompressController("./test/numbers.txt.huff");
	return EXIT_SUCCESS;
}