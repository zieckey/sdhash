// Header file for bloom_filter object
//
#ifndef _BLOOM_FILTER_H 
#define _BLOOM_FILTER_H


#include <stdint.h>
#include <string>
//#include <strings.h>

using namespace std;

/**
	bloom_filter:  a Bloom filter class.
*/
/// bloom_filter class
class bloom_filter {

public:
    /// base constructor
    bloom_filter(uint64_t size, uint16_t hash_count, uint64_t max_elem, double max_fp); 

    /// construct from file - not add to master or fold up. 
    bloom_filter(string indexfilename);

    /// construct bloom filter from buffer 
    bloom_filter(uint8_t* data,uint64_t size);
    
    /// destructor
    ~bloom_filter();

    /// insert SHA1 hash
    bool insert_sha1(uint32_t *sha1);
    
    /// query SHA1 hash
    bool query_sha1(uint32_t *sha1);

    /// return element count
    uint64_t elem_count();
    /// return estimate of false positive rate
    double est_fp_rate();    
    /// return bits per element
    double bits_per_elem();
 
    /// name associated with bloom filter
    string name() const;
    /// change name associated with bloom filter
    void set_name(string name);
    /// fold a large bloom filter onto itself
    void fold(uint32_t times);
    /// add another same-sized bloom filter to this one
    int add(bloom_filter *other);
    /// write bloom filter to .idx file
    int write_out(string filename);

private:
    /// actual query/insert function
    bool query_and_set(uint32_t *sha1, bool mode_set);
    /// compress blob
    char* compress() ;
    /// decompress blob and assign to bf
    int32_t decompress(char* src);
public:
    static const uint32_t BIT_MASKS_32[];
    static const uint32_t BITS[];

private:
    uint64_t  max_elem;      // Max number of elements
    double    max_fp;        // Max FP rate
    
    uint8_t  *bf;            // Beginning of the BF 
    uint64_t  bf_size;       // BF size in bytes (==m/8)
    uint64_t  bf_elem_count; // Actual number of elements inserted
    uint16_t  hash_count;    // Number of hash functions used (k)
    uint64_t  bit_mask;      // Bit mask
    uint64_t  comp_size;     // size of compressed bf to be read
    string    setname;       // name associated with bloom filter
    bool      created;       // set if we allocated the bloom filter ourselves

};

#endif
