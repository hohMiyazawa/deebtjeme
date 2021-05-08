#include "file_io.hpp"

int main(int argc, char *argv[]){
	if(argc < 3){
		printf("not enough arguments\n");
		print_usage();
		return 1;
	}

	size_t in_size;
	uint8_t* in_bytes = read_file(argv[1], &in_size);
	printf("read %d bytes\n",(int)in_size);
	size_t output_size = 0;
	uint8_t* output_bytes = new uint8_t[in_size];

	int numberParse = 1;
	
	for(size_t i=0;i<in_size;i++){
	}
	delete[] in_bytes;
	write_file(argv[2],output_bytes,output_size);
	delete[] output_bytes;

	return 0;
}
