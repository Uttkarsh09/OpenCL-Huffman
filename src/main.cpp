#include "configuration.hpp"
#include "compress.hpp"

using namespace std;

int main(void)
{
	compressController("./test/book.txt");
	return EXIT_SUCCESS;
}