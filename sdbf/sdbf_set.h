#ifndef __SDBF_SET_H
#define __SDBF_SET_H

#include <ostream>
#include <string>
#include <vector>
#include <boost/thread/thread.hpp>
#include "sdbf_class.h"
#include "bloom_filter.h"
#include "util.h"


/// sdbf_set class
class sdbf_set {

    friend std::ostream& operator<<(std::ostream& os, const sdbf_set& s); ///< output operator
    friend std::ostream& operator<<(std::ostream& os, const sdbf_set *s); ///< output operator

public:
	/// creates blank sdbf_set
	sdbf_set(); 

	/// creates blank sdbf_set with index
	sdbf_set(bloom_filter *index); 

    /// loads an sdbf_set from a file
    sdbf_set(const char *fname); 


    /// Add by weizili
    /// loads an sdbf_set from a memory buffer
    sdbf_set(const char *buffer, size_t buffer_length); 

    /// destructor
    ~sdbf_set();

    /// accessor method for individual hashes
    class sdbf* at(uint32_t pos); 

    /// adds a single hash to this set
    void add(class sdbf *hash);

    /// adds the items in another set to this set
    void add(sdbf_set *hashset);


    /// Returns the number of sdbfs in this set
    uint64_t size( ) ; 

    /// Computes the data size of this set
    uint64_t input_size( ) ; 

    uint64_t filter_count();

	/// Compares all objects in a set to each other
	std::string compare_all(int32_t threshold); 

	///queries one set for the contents of another
	std::string compare_to(sdbf_set *other,int32_t threshold, uint32_t sample_size); 

	/// return a string which contains the output-encoded sdbfs in this set
	std::string to_string() const;

	/// return a string which contains the results of this set's index seraching
	std::string index_results() const;


	/// is this empty?
	int empty();

	/// retrieve name of set
	std::string name() const;

	/// name this set.
	void set_name(std::string name);

	/// setup bloom filter vector
	void vector_init();

    /// Add by weizili
    /// free a sdbf_set
    static void destory(sdbf_set* &set);

public:
    /// index for this set 
	class bloom_filter *index;
    /// giant bloom filter vector for this set
	std::vector<class bloom_filter*> *bf_vector;

private:

	std::vector<class sdbf*> items;
	std::string setname;
	boost::mutex add_hash_mutex;

};

#endif
