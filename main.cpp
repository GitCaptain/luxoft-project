#include <string>
#include <thread>
#include <fstream>
#include <sys/mman.h>
#include <memory>
#include <iostream>
#include <vector>
#include <mutex>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sstream>
#include <errno.h>
#include <string.h>

class FileException: public std::exception{

    std::string err_mes;
public:
    FileException(const std::string& err_mes): err_mes(err_mes){}

    const char* what() const noexcept override{
        return err_mes.c_str();
    }

};

class MappedFile{
protected:
    int fd;
    bool _eof;
    struct stat status;
    void *address;

    void mapFile(int prot){
        address = mmap(nullptr, status.st_size, prot, MAP_PRIVATE, fd, 0);
        if(address == MAP_FAILED)
            throw FileException("Cant map file");
    }

private:

    void init(){
        if(fstat(fd, &status) < 0)
            throw FileException("Cant get file info");
        _eof = false;
        address = nullptr;
    }

public:

    MappedFile(char* path, int flags, mode_t mode){
        fd = open(path, flags, mode);
        init();
    }

    MappedFile(char* path, int flags){
        fd = open(path, flags);
        init();
    }


    off_t get_size(){
        return status.st_size;
    }

    bool is_open(){
        return fd >= 0;
    }

    bool eof(){
        return _eof;
    }

    ~MappedFile(){
        if(fd >= 0)
            close(fd);
        if(munmap(address, status.st_size))
            throw FileException("Cant unmap file");
    }
};

class InputMappedFile: public MappedFile{

public:

    InputMappedFile(char* path): MappedFile(path, O_RDONLY){}

    void mapFile(){
        MappedFile::mapFile(PROT_READ);
    }

    std::string read(int sz){
        std::unique_ptr<char[]> data(new char[sz+1]);
        int got = ::read(fd, data.get(), sz);
        if(got < sz)
            _eof = true;
        if(sz && !got)
            throw FileException("Cant read file");
        return std::string(data.get());
    }
};

class OutputMappedFile: public MappedFile{
public:
    OutputMappedFile(char* path): MappedFile(path, O_RDWR|O_CREAT|O_TRUNC, S_IRWXU){}

    void mapFile(){
        MappedFile::mapFile(PROT_WRITE);
    }

    void write(const std::string &str){
        ::write(fd, str.c_str(), str.length());
    }


    void update_size(off_t size){
        if((status.st_size = lseek(fd, size, SEEK_SET)) < 0)
            throw FileException("Cant update file size");
    }

};


std::vector<std::exception_ptr> threads_exceptions;
int thread_to_write = 0;
std::mutex thread_mutex;

//void get_hash_and_write_thread(std::string str, const int thread_num, OutputMappedFile &output_file){
void get_hash_and_write_thread(std::string str, const int thread_num, std::ofstream &output_file){
    try {

        std::hash<std::string> get_hash;
        auto hash = get_hash(str);

        std::stringstream converter;

        std::string hash_str;
        converter << hash;
        converter >> hash_str;

        while (thread_num > thread_to_write)
            std::this_thread::yield();

        //output_file.write(hash_str);
        output_file << hash_str;
        thread_to_write++;
    }
    catch(...){

        thread_mutex.lock();
        threads_exceptions.emplace_back(std::current_exception());
        thread_mutex.unlock();

        while (thread_num > thread_to_write)
            std::this_thread::yield();
        if(thread_num == thread_to_write) {
            thread_to_write++;
        }
    }
}


int main(int argc, char **argv){

    const int BYTES_PER_MEGABYTE = 1024*1024;

    try{
        if(argc < 3)
            throw std::invalid_argument("Paths to input file and output file should be given");

        InputMappedFile input_file(argv[1]);
        if(!input_file.is_open())
            throw FileException("Can't access input file");
        input_file.mapFile();

        /*
        OutputMappedFile output_file(argv[2]);
        if(!output_file.is_open())
            throw FileException("Can't access output file");
        output_file.update_size(input_file.get_size());
        output_file.mapFile();
        */

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
        while(!input_file.eof()){
            std::string data = input_file.read(block_size_in_bytes);
            std::thread t(get_hash_and_write_thread, data, thread_id, std::ref(output_file));
            t.detach();
            thread_id++;
        }

        while(thread_to_write < thread_id)
            std::this_thread::yield();

        for(auto &te: threads_exceptions){
            std::rethrow_exception(te);
        }
    }
    catch (std::exception& e){
        std::cout << e.what() << std::endl;
        perror("");
    }
    catch(...){
        std::cout << "Something went wrong.\n";
    }
    return 0;
}
