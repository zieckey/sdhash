// sdbf_set.cc
// author: candice quates
// set function implementation 

#include "sdbf_class.h"
#include "sdbf_defines.h"
#include "bloom_filter.h"
#include "util.h"
#include "sdbf_set.h"

#include <boost/regex.hpp>
#include <boost/filesystem.hpp>
#include <boost/thread/thread.hpp>

#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <fstream>
using namespace std;

namespace fs = boost::filesystem;


/** 
   Creates empty sdbf_set
*/
sdbf_set::sdbf_set() {
    setname="default";    
    index = NULL;
    bf_vector=new vector<bloom_filter*>();
}

/** 
    Creates empty sdbf_set with an index
    \param index to insert new items into
*/
sdbf_set::sdbf_set(bloom_filter *index) {
    setname="default";    
    this->index=index;
    bf_vector=new vector<bloom_filter*>();
}

/** 
    Loads all sdbfs from a file into a new set
    \param fname name of sdbf file
*/
sdbf_set::sdbf_set(const char *fname) {
    if (fs::is_regular_file(fname)) {
        FILE *in = fopen( fname, "r");
        if (in!=NULL) {  // if fail to open leave set empty
            setname=(string)fname;
            int bar=getc( in);
            if (!feof(in)) {
                ungetc(bar,in);
            }
            while( !feof( in)) {
                class sdbf *sdbfm = new sdbf( in);
                items.push_back( sdbfm);
                getc( in);
                int bar=getc( in);
                if (!feof(in)) {
                    ungetc(bar,in);
                }
            }
        }
        fclose(in);
    }
    // right now we cannot read-in an index.  
    // but we can set one later
    index = NULL;
    // we can create a bf-ptr-full vector
    bf_vector=new vector<bloom_filter*>();
    vector_init();
}

sdbf_set::~sdbf_set() {
    if (bf_vector->size() > 0) {
	for (int i=0;i<bf_vector->size();i++)
	    delete bf_vector->at(i);
    }
    delete bf_vector;
}

/**
    Accessor method for a single sdbf* in this set
    \param pos position 0 to size()
    \returns sdbf* or NULL if position not valid
*/
class sdbf*
sdbf_set::at(uint32_t pos) {
    if (pos < items.size()) 
        return items.at(pos);    
    else 
        return NULL;
}

/** 
    Adds a single hash to this set
    \param hash an existing sdbf hash
*/
void 
sdbf_set::add(sdbf *hash) {
    this->add_hash_mutex.lock();
    items.push_back(hash);
    this->add_hash_mutex.unlock();
}

/**
    Adds all items in another set to this set
    \param hashset sdbf_set* to be added
*/
void 
sdbf_set::add(sdbf_set *hashset) {
    // for all in hashset->items, add to this->items
    for (std::vector<sdbf*>::const_iterator it = hashset->items.begin(); it!=hashset->items.end() ; ++it)  {
        items.push_back(*it);
    }    
}

/**
    Computes the data size of this set, from the
    input_size() values of its' content sdbf hashes.
    \returns uint64_t total of input sizes
*/
uint64_t 
sdbf_set::input_size()  {
    uint64_t size=0;
    for (std::vector<sdbf*>::const_iterator it = items.begin(); it!=items.end() ; ++it)  {
        size+=(*it)->input_size();
    }
    return size;
}

/** 
    Number of items in this set    
    \returns uint64_t number of items in this set
*/
uint64_t
sdbf_set::size() {
    return items.size();
}

/** 
    Checks empty status of container
    \returns int 1 if empty, 0 if non-empty
*/
int
sdbf_set::empty() {
    if (items.size() > 0) 
        return 0;
    else 
        return 1;
}
/** 
   Generates a string representing the indexing results of this set
*/
std::string 
sdbf_set::index_results() const {
    std::stringstream builder;
    for (std::vector<sdbf*>::const_iterator it = items.begin(); it!=items.end() ; ++it)  {
        builder << (*it)->get_index_results() ;    
    }
    //builder << this->index;
    return builder.str();
}


/**
    Generates a string which contains the output-encoded sdbfs in this set
    \returns std::string containing sdbfs.
*/
std::string 
sdbf_set::to_string() const {
    std::stringstream builder;
    for (std::vector<sdbf*>::const_iterator it = items.begin(); it!=items.end() ; ++it)  {
        builder << *it ;    
    }
    //builder << this->index;
    return builder.str();
}

/** 
    Write this sdbf_set to stream 
    \param os target output stream
    \param *s set to write out
    \returns ostream& output stream
*/
ostream& operator<<(ostream& os, const sdbf_set& s) {
    os << s.to_string();
    return os;
}

/**
    Write this sdbf_set to stream 
    \param os target output stream
    \param *s set to write out
    \returns ostream& output stream
*/
ostream& operator<<(ostream& os, const sdbf_set *s) {
    os << s->to_string();
    return os;
}

/**
    Retrieve name of this set
    \returns string name
*/
std::string
sdbf_set::name() const {
    return setname;
}

/**
    Change name of this set
    \param name of  string
*/
void
sdbf_set::set_name(std::string name) {
    setname=name;
}

/**
    Compares each sdbf object in target to every other sdbf object in target
    and returns the results as a list stored in a string 

    \param threshold output threshold, defaults to 1
    \returns std::string result listing
*/
std::string 
sdbf_set::compare_all(int32_t threshold) { 
    int32_t score = 0;
    uint32_t map_on = 0;
    std::stringstream out;
    out.fill('0');
    int end = this->items.size();
    for (int i = 0; i < end ; i++) {
        for (int j = i; j < end ; j++) {
            if (i == j) continue;
            score = this->items.at(i)->compare(this->items.at(j),map_on,0);
            if (score >= threshold)  {
                out << this->items.at(i)->name() << "|" << this->items.at(j)->name() ;
                out << "|" << setw (3) << score << std::endl;
            }
        }            
    }
    return out.str();
}

/**
    Compares each sdbf object in other to each object in this set, and returns
    the results as a list stored in a string.

    \param other set
    \param threshold output threshold, defaults to 1
    \param sample_size size of bloom filter sample. send 0 for no sampling
    \returns std::string result listing
*/
std::string
sdbf_set::compare_to(sdbf_set *other,int32_t threshold,uint32_t sample_size) {
    int32_t score = 0;
    uint32_t map_on = 0;
    std::stringstream out;
    out.fill('0');
    int tend = other->size();
    int qend = this->size();
    for (int i = 0; i < qend ; i++) {
        for (int j = 0; j < tend ; j++) {
            score = this->items.at(i)->compare(other->items.at(j),map_on,sample_size);
            if (score >= threshold) {
                out << this->items.at(i)->name() << "|" << other->items.at(j)->name() ;
                out << "|" << setw (3) << score << std::endl;
            }
        }
    }
    return out.str();
}

uint64_t
sdbf_set::filter_count() {
    return bf_vector->size();	
}

/**
   Sets up bloom filter vector for index searching.  
   Should be called by server process when done hashing to a set
*/
void
sdbf_set::vector_init() {
    for (int i=0;i<items.size(); i++)
        for (int n=0; n< items.at(i)->filter_count() ; n++) {
	    uint8_t *data=items.at(i)->clone_filter(n);
	    bloom_filter *tmp=new bloom_filter(data,256);
	    string *tmpstr= new string(items.at(i)->name());
	    tmp->set_name(*tmpstr);
	    bf_vector->push_back(tmp);
	    free(data);
	    delete tmpstr;
	}
}

