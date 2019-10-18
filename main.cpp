#include <string>
#include <thread>
#include <fstream>
#include <sys/mman.h>
#include <memory>
#include <iostream>
#include <vector>

class FileException: public std::exception{

    std::string err_mes;
public:
    FileException(const std::string& err_mes): err_mes(err_mes){}

    const char* what() const noexcept override{
        return err_mes.c_str();
    }

};


std::vector<std::exception_ptr> thread_exceptions;
int thread_to_write = 0;
void get_hash_and_write_thread(const std::string &str, int thread_num, std::ofstream& output_file){
    try {
        std::hash<std::string> get_hash;

        auto hash = get_hash(str);

        while (thread_num != thread_to_write)
            std::this_thread::yield();

        output_file << hash;
        thread_to_write++;
    }
    catch(...){
        if(thread_to_write == thread_num)
            thread_to_write++;
    }
}

void close_file(std::FILE* fp){
    std::fclose(fp);
}

std::vector<std::exception_ptr> threads_exceptions;

int main(int argc, char **argv){

    const int BYTES_PER_MEGABYTE = 1024;

    try{
        if(argc < 3)
            throw std::invalid_argument("Paths to input file and output file should be given");

        std::ifstream input_file(argv[1], std::ios::in | std::ios::binary);
        if(!input_file.is_open())
            throw FileException("Can't access input file");

        std::ofstream output_file(argv[2]);
        if(!output_file.is_open())
            throw FileException("Can't access output file");

        unsigned long block_size_in_bytes = BYTES_PER_MEGABYTE;

        if(argc >= 4) {
            block_size_in_bytes = strtoul(argv[3], nullptr, 10);
            if(block_size_in_bytes == 0)
                throw std::invalid_argument("Third argument must be positive integer value (block size in bytes)");
        }

        int thread_id = 0;
        do{
            std::unique_ptr<char[]> buf(new char[block_size_in_bytes]);
            input_file.read(buf.get(), block_size_in_bytes);
            std::thread(get_hash_and_write_thread, std::string(buf.get()), thread_id++, output_file);
        }while(!input_file.eof());


        while(thread_to_write < thread_id)
            std::this_thread::yield();

        for(auto &te: threads_exceptions){
            std::rethrow_exception(te);
        }

    }
    catch (std::exception& e){
        std::cout << e.what() << std::endl;
    }
    catch(...){
        std::cout << "Something went wrong.\n";
    }

    return 0;
}
