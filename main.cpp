#include <functional>
#include <string>
#include <thread>
#include <string.h>
#include <fstream>
#include <sys/mman.h>
#include <memory>
#include <iostream>
#include <vector>
#include <sstream>

class FileException: public std::exception{

    std::string err_mes;
public:
    FileException(const std::string& err_mes): err_mes(err_mes){}

    std::string what(){
        return err_mes;
    }

};

int thread_to_write = 0;
void get_hash_and_write_thread(const std::string &str, int thread_num, std::shared_ptr<std::FILE> output_file){
    try {
        std::hash<std::string> get_hash;

        auto hash = get_hash(str);

        std::stringstream converter;
        converter << hash;
        std::string string_hash;
        converter >> string_hash;

        while (thread_num != thread_to_write)
            std::this_thread::yield();

        std::fwrite(string_hash.c_str(), string_hash.length(), 1, output_file.get());
        thread_to_write++;
    }
    catch(...){

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

        auto tmp_input_file_ptr = std::fopen(argv[1], "r");
        if(tmp_input_file_ptr == nullptr)
            throw FileException("Can't access input file");
        std::unique_ptr<std::FILE, decltype(&close_file)> input_file(tmp_input_file_ptr, &close_file);


        auto tmp_output_file_ptr = std::fopen(argv[2], "w");
        if(tmp_output_file_ptr == nullptr)
            throw FileException("Can't access output file");
        std::shared_ptr<std::FILE> output_file(tmp_output_file_ptr, &close_file);

        unsigned long block_size_in_bytes = BYTES_PER_MEGABYTE;

        if(argc >= 4) {
            block_size_in_bytes = strtoul(argv[3], nullptr, 10);
            if(block_size_in_bytes == 0)
                throw std::invalid_argument("Third argument must be positive integer value (block size in bytes)");
        }

        int size_read;
        int thread_id = 0;
        do{
            auto buf = std::unique_ptr<char*>(new char*[block_size_in_bytes]);
            size_read = std::fread(buf.get(), block_size_in_bytes, 1, input_file.get());
            std::thread(get_hash_and_write_thread, std::string(*buf), thread_id++, output_file);
        }while(size_read == block_size_in_bytes);

        if(!std::feof(input_file.get()))
            throw FileException("Some error occurred while reading file");

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
