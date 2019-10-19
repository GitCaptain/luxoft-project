/*
#include <string>
#include <thread>
#include <fstream>
#include <sys/mman.h>
#include <memory>
#include <iostream>
#include <vector>
#include <mutex>
using namespace std;

class FileException: public std::exception{

    std::string err_mes;
public:
    FileException(const std::string& err_mes): err_mes(err_mes){}

    const char* what() const noexcept override{
        return err_mes.c_str();
    }

};

std::vector<std::exception_ptr> threads_exceptions;
int thread_to_write = 0;
std::mutex thread_mutex;

void get_hash_and_write_thread(const std::shared_ptr<char[]>  &str, int thread_num, std::ofstream& output_file){
    try {
        std::hash<std::string> get_hash;
        auto hash = get_hash(std::string(str.get()));

        while (thread_num < thread_to_write)
            std::this_thread::yield();

        output_file << hash;
        thread_to_write++;
    }
    catch(...){

        thread_mutex.lock();
        threads_exceptions.emplace_back(std::current_exception());
        thread_mutex.unlock();

        while (thread_num < thread_to_write)
            std::this_thread::yield();
        if(thread_num == thread_to_write)
            thread_to_write++;
    }
}

int main(){
    std::ifstream input_file("in", std::ios::in | std::ios::binary);
    if(!input_file.is_open())
        throw FileException("Can't access input file");

    std::ofstream output_file("out");
    if(!output_file.is_open())
        throw FileException("Can't access output file");

    thread *t;
    while(!input_file.eof()){
        shared_ptr<char[]> buf(new char[1024]);
        input_file.read(buf.get(), 1024-1);
        get_hash_and_write_thread(std::ref(buf), 0, std::ref(output_file));
        t = new thread(get_hash_and_write_thread, std::ref(buf), 0, std::ref(output_file));
    }
    t->join();
}
*/

#include <string>
#include <thread>
#include <fstream>
#include <sys/mman.h>
#include <memory>
#include <iostream>
#include <vector>
#include <mutex>


class FileException: public std::exception{

    std::string err_mes;
public:
    FileException(const std::string& err_mes): err_mes(err_mes){}

    const char* what() const noexcept override{
        return err_mes.c_str();
    }

};


std::vector<std::exception_ptr> threads_exceptions;
int thread_to_write = 0;
std::mutex thread_mutex;

void get_hash_and_write_thread(const std::shared_ptr<char[]>  &str, int thread_num, std::ofstream& output_file){
    try {
        std::hash<std::string> get_hash;

        auto hash = get_hash(std::string(str.get()));

        while (thread_num < thread_to_write)
            std::this_thread::yield();

        output_file << hash;
        thread_to_write++;
    }
    catch(...){

        thread_mutex.lock();
        threads_exceptions.emplace_back(std::current_exception());
        thread_mutex.unlock();

        while (thread_num < thread_to_write)
            std::this_thread::yield();
        if(thread_num == thread_to_write)
            thread_to_write++;
    }
}

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
        std::thread t;
        while(!input_file.eof()){
            std::shared_ptr<char[]> buf(new char[block_size_in_bytes]);
            input_file.read(buf.get(), block_size_in_bytes-1);
            get_hash_and_write_thread(std::ref(buf), thread_id++, std::ref(output_file));
            t = std::thread(get_hash_and_write_thread, std::ref(buf), thread_id++, std::ref(output_file));
        }


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
