%module sdbf_class
%{
#include "../../sdbf/sdbf_class.h"
#include "../../sdbf/sdbf_conf.h"
#include "../../sdbf/sdbf_defines.h"
#include "../../sdbf/sdhash_set.h"
#include <stdint.h>
#include <stdio.h>
#include <unordered_set> 
//typedef unsigned int uint32_t;
//typedef long long unsigned int uint64_t;
//typedef unsigned long int uint64_t;

%}

#define KB 1024
%include "cpointer.i"
%include "std_string.i"

%pointer_functions(int, intp);

// note these are the linux x86_64 types
typedef unsigned int uint32_t;
typedef int int32_t;
typedef unsigned long int uint64_t;
typedef long int int64_t;

FILE *fopen(const char *filename, const char *mode);
int fclose(FILE *);
int feof(FILE *);

class sdbf_conf {

public:
    sdbf_conf(uint32_t thread_cnt, uint32_t warnings, uint32_t max_elem_ct, uint32_t max_elem_ct_dd ); // defaults
    // later: rc file for this
    ~sdbf_conf(); // destructor
};


class sdbf {
    friend std::ostream& operator<<(std::ostream& os, const sdbf& s); 
    friend std::ostream& operator<<(std::ostream& os, const sdbf *s); 

public:
    sdbf(FILE *in); // from stream
    sdbf(char *filename, uint32_t dd_block_size); // to create from file
    sdbf(char *name, std::istream *ifs, uint32_t dd_block_size, uint64_t msize) ;

    ~sdbf(); // destructor

    char *get_name();  // object name
    uint64_t get_size();  // object size
    uint64_t get_real_size();  // source object size

    // matching algorithm, take other object and run match
    uint32_t compare(sdbf *other, uint32_t map_on, uint32_t sample);

    void to_stream(FILE *out); // write self to stream

    string tostring() const;
public:
    static class sdbf_conf *myconf;  // global configuration object

%extend {
    void print_sdbf(sdbf *printme) {
        printme->to_stream(stdout);
    }
}

};
