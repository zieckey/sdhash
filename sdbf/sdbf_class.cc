// sdbf_class.cc 
// Authors: Candice Quates, Vassil Roussev
// implementation of sdbf object

#include "sdbf_class.h"
#include "sdbf_defines.h"

#define SDBF_VERSION 3 

#include <stdint.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <iomanip>


#define MILLISECOND_LOG

void z_dbgtrace( const char* filename, const char* funcname, int lineno, const char* fmt, ... )
{
    char s[1024*128] = {0};

    int millisec = 0;
    (void)millisec;
    time_t ctTime;
#ifdef MILLISECOND_LOG
    struct timeval tv;
    gettimeofday(&tv, NULL);
    ctTime = tv.tv_sec;
    millisec = tv.tv_usec/1000;
#else
    time( &ctTime );
#endif
    struct tm *pTime;
    pTime = localtime( &ctTime );
    int writen = snprintf(s, sizeof(s), "%4d/%.2d/%.2d %.2d:%.2d:%.2d"
#ifdef MILLISECOND_LOG
                " %0.3d"
#endif
                " %s:%s:%d - ",
                pTime->tm_year + 1900, pTime->tm_mon + 1, pTime->tm_mday,
                pTime->tm_hour, pTime->tm_min, pTime->tm_sec, 
#ifdef MILLISECOND_LOG
                millisec, 
#endif
                filename, funcname, lineno);

    if ( writen <= 0 )
    {
        fprintf( stderr, "snprintf return error, errno=%d\n", errno );
        return;
    }
    va_list ap;
    va_start(ap,fmt);
    int len = vsnprintf(s + writen, sizeof(s) - writen, fmt, ap);
    (void)(len);
    va_end(ap);
    fprintf(stdout, "%s\n", s);
}

#ifdef _TRACE
#define LogTrace(fmt, args...)  z_dbgtrace( __FILE__, __func__, __LINE__, fmt, ##args )
#else
#define LogTrace(fmt, args...)  {}
#endif

/** 
    \internal
    Initialize static configuration object with sensible defaults.
*/
sdbf_conf *sdbf::config = new sdbf_conf(1, FLAG_OFF, _MAX_ELEM_COUNT, _MAX_ELEM_COUNT_DD);

/** 
    Create new sdbf from file.  dd_block_size turns on "block" mode. 
    \param filename file to hash
    \param dd_block_size size of block to process file with. 0 is off.
*/
sdbf::sdbf(const char *filename, uint32_t dd_block_size) {
    processed_file_t *mfile = process_file( filename, MIN_FILE_SIZE, config->warnings);
    if( !mfile)
        throw -1 ; // cannot process file
    sdbf_create(filename);
    this->info=NULL;
    this->orig_file_size=mfile->size;
    if (!dd_block_size) {  // stream mode
        this->max_elem = config->max_elem;
        gen_chunk_sdbf( mfile->buffer,mfile->size, 32*MB);
    } else {  // block mode
        this->max_elem = config->max_elem_dd;
        uint64_t dd_block_cnt =  mfile->size/dd_block_size;
        if( mfile->size % dd_block_size >= MIN_FILE_SIZE)
            dd_block_cnt++;
        this->bf_count = dd_block_cnt;
        this->dd_block_size = dd_block_size;
        this->buffer = (uint8_t *)alloc_check( ALLOC_ZERO, dd_block_cnt*config->bf_size, "sdbf_hash_dd", "this->buffer", ERROR_EXIT);
        this->elem_counts = (uint16_t *)alloc_check( ALLOC_ZERO, sizeof( uint16_t)*dd_block_cnt, "sdbf_hash_dd", "this->elem_counts", ERROR_EXIT);
        gen_block_sdbf_mt( mfile->buffer, mfile->size, dd_block_size, config->thread_cnt);
    }
    free( mfile->buffer);
    compute_hamming();
    free(mfile);
} 

/**
    Generates a new sdbf, with a maximum size read from an open stream.
    dd_block_size enables block mode.
    \param name name of stream
    \param ifs open istream to read raw data from
    \param dd_block_size size of block to divide data with. 0 is off.
    \param msize amount of data to read and process
    \param info block of information about indexes
*/
sdbf::sdbf(const char *name, std::istream *ifs, uint32_t dd_block_size, uint64_t msize,index_info *info) { 
    uint64_t chunk_size;
    uint8_t *bufferinput;    

    bufferinput = (uint8_t*)alloc_check(ALLOC_ZERO, sizeof(uint8_t)*msize,"sdbf_hash_stream",
        "buffer input", ERROR_EXIT);
    ifs->read((char*)bufferinput,msize);
    chunk_size = ifs->gcount();
    if (chunk_size < MIN_FILE_SIZE) {
        free(bufferinput);
        throw -3; // too small
    }
    sdbf_create(name); // change to filename+offset / size
    this->info=info;
    this->orig_file_size=chunk_size;
    if (!dd_block_size) {  // single stream mode should not be used but we'll support it anyway
        this->max_elem = config->max_elem;
        gen_chunk_sdbf(bufferinput,msize, 32*MB);
    } else { // block mode
        this->max_elem = config->max_elem_dd;
        uint64_t dd_block_cnt =  msize/dd_block_size;
        if( msize % dd_block_size >= MIN_FILE_SIZE)
            dd_block_cnt++;
        this->bf_count = dd_block_cnt;
        this->dd_block_size = dd_block_size;
        this->buffer = (uint8_t *)alloc_check( ALLOC_ZERO, dd_block_cnt*config->bf_size, "sdbf_hash_dd", "this->buffer", ERROR_EXIT);
        this->elem_counts = (uint16_t *)alloc_check( ALLOC_ZERO, sizeof( uint16_t)*dd_block_cnt, "sdbf_hash_dd", "this->elem_counts", ERROR_EXIT);
        gen_block_sdbf_mt( bufferinput, msize, dd_block_size, config->thread_cnt);
    }
    compute_hamming();
    free(bufferinput);
}

/**
    Generates a new sdbf, from a char *string
    dd_block_size enables block mode.
    \param name name of stream
    \param str input to be hashed
    \param dd_block_size size of block to divide data with. 0 is off.
    \param length length of str to be hashed
    \param info block of information about indexes
*/
sdbf::sdbf(const char *name, char *str, uint32_t dd_block_size, uint64_t length,index_info *info) { 
    if (length < MIN_FILE_SIZE) 
        throw -3; // too small
    sdbf_create(name); 
    this->info=info;
    this->orig_file_size=length;
    if (!dd_block_size) {  // single stream mode should not be used but we'll support it anyway
        this->max_elem = config->max_elem;
        gen_chunk_sdbf((uint8_t*)str,length, 32*MB);
    } else { // block mode
        this->max_elem = config->max_elem_dd;
        uint64_t dd_block_cnt =  length/dd_block_size;
        if( length % dd_block_size >= MIN_FILE_SIZE)
            dd_block_cnt++;
        this->bf_count = dd_block_cnt;
        this->dd_block_size = dd_block_size;
        this->buffer = (uint8_t *)alloc_check( ALLOC_ZERO, dd_block_cnt*config->bf_size, "sdbf_hash_dd", "this->buffer", ERROR_EXIT);
        this->elem_counts = (uint16_t *)alloc_check( ALLOC_ZERO, sizeof( uint16_t)*dd_block_cnt, "sdbf_hash_dd", "this->elem_counts", ERROR_EXIT);
        gen_block_sdbf_mt( (uint8_t*)str, length, dd_block_size, config->thread_cnt);
    }
    compute_hamming();
}

/**
    Reads an already generated sdbf from open file.  
    Throws exceptions in case of bad formatting.
    \param in FILE* open formatted as list of sdbfs
*/
sdbf::sdbf(FILE *in) {

   char *b64, fmt[64];
    uint8_t  buffer[16*KB];
    char sdbf_magic[16], hash_magic[8];
    uint32_t colon_cnt, read_cnt, hash_cnt, b64_len;
    int d_len;
    uint32_t version, name_len;
    uint64_t i;

    if( feof( in))
        throw -1; //end of file - quit

    sdbf_create(NULL);

    for( i=0, colon_cnt=3; i<MAX_MAGIC_HEADER && !feof(in); i++) {
        buffer[i] = fgetc( in);
		if (i==4 && strncmp((char*)buffer,"sdbf",4) )
			throw -2;
        if( buffer[i] == DELIM_CHAR) {
            buffer[i] = 0x20;
            colon_cnt--;
            if( !colon_cnt)
                break;
        }
    }
    if( feof( in))
        throw -3 ; // end of file prematurely
    buffer[i] = 0;
    LogTrace("read header=[%s]", buffer);
    sscanf( (char*)buffer, "%s %d %d", sdbf_magic, &version, &name_len);
    if( (strcmp( sdbf_magic, MAGIC_STREAM) && strcmp( sdbf_magic, MAGIC_DD)) || version != 3) {
        if (config->warnings)
            fprintf( stderr, "ERROR: Unsupported format '%s:%02d'. Expecting '%s:03' or '%s:03'\n", sdbf_magic, version, MAGIC_STREAM, MAGIC_DD);
        throw -2 ; // unsupported format - caller should exit
    }
    fmt[0] = '%';
    sprintf( fmt+1, "%dc", name_len);
    this->filenamealloc=true;
    this->hashname = (char*)alloc_check( ALLOC_ZERO, name_len+2, "sdbf_from_stream", "this->hashname", ERROR_EXIT);
    read_cnt = fscanf( in, fmt, this->hashname);
    read_cnt = fscanf( in, ":%ld:%4s:%d:%d:%x:%d:%d", &(this->orig_file_size), hash_magic, &(this->bf_size), &(this->hash_count), &(this->mask), &(this->max_elem), &(this->bf_count));
    LogTrace("orig_file_size=%ld hash_magic=%4s bf_size=%d hash_count=%d mask=%x max_elem=%d bf_count=%d", (this->orig_file_size), hash_magic, (this->bf_size), (this->hash_count), (this->mask), (this->max_elem), (this->bf_count));
    this->buffer = (uint8_t *)alloc_check( ALLOC_ZERO, this->bf_count*this->bf_size, "sdbf_from_stream", "this->buffer", ERROR_EXIT);
    // DD fork
    if( !strcmp( sdbf_magic, MAGIC_DD)) {
        read_cnt = fscanf( in, ":%d", &(this->dd_block_size));
        this->elem_counts = (uint16_t *)alloc_check( ALLOC_ZERO, this->bf_count*sizeof(uint16_t), "sdbf_from_stream", "this->elem_counts", ERROR_EXIT);
        for( i=0; i<this->bf_count; i++) {
            read_cnt = fscanf( in, ":%2x:%344s", &hash_cnt, buffer);
            this->elem_counts[i] = (uint16_t)hash_cnt;
            d_len = b64decode_into( buffer, 344, this->buffer + i*this->bf_size);
            if( d_len != 256) {
                if (config->warnings)
                    fprintf( stderr, "ERROR: Unexpected decoded length for BF: %d. name: %s, BF#: %d\n", d_len, this->hashname, (int)i);
                throw -2; // unsupported format - caller should exit
            }
        }
    // Stream fork
    } else {
        read_cnt = fscanf( in, ":%d:", &(this->last_count));
        b64_len = this->bf_count*this->bf_size;
        b64_len = 4*(b64_len/3 +1*(b64_len % 3 > 0 ? 1 : 0));
        sprintf( &fmt[1], "%ds", b64_len);
        b64 = (char*)alloc_check( ALLOC_ZERO, b64_len+2, "sdbf_from_stream", "b64", ERROR_EXIT);
        read_cnt = fscanf( in, fmt, b64);
        LogTrace("b64_len=%d b64=[%s]", b64_len, b64);
		free(this->buffer);
        this->buffer =(uint8_t*) b64decode( (char*)b64, (int)b64_len, &d_len);
        if( d_len != this->bf_count*this->bf_size) {
            if (config->warnings)
                fprintf( stderr, "ERROR: Incorrect base64 decoding length. Expected: %d, actual: %d\n", this->bf_count*this->bf_size, d_len);
            free (b64); // cleanup in case of wanting to go on
            throw -2; // unsupported format, caller should exit
        }
        free( b64);
    }
    compute_hamming();
    this->info=NULL;
}
/**
    Reads an already generated sdbf from the memory buffer
    No throws exceptions in any case
    \return true if successfully loaded 
    \param  formatted_sdbf_buffer
        sdbf:03:12:README.alpha:1197:sha1:256:5:7ff:160:1:19:AAAAAAAAAAAAAEBAAgQAAAAAAAEQAQAAAAIAAAACAAAIAAAAAAAAAAAAAAAgEAEAAAAgJAAAABAQAACAAAAAAIAAIEAIIAIACJAAgAAAAIAEACIAIAAKAAAAAAAhAAAAAAAAAAIoAAAAAAAAIAAAgAAAAQAAACAAACAAAAAABQAAAAAAAAAgAABAAAQAICAgAAAAAAAAAQACAIAAAAAABoABAAAACAEAAAAAEEACQABAAAAEAAACAABA
*/
bool sdbf::load_sdbf(const char* formatted_sdbf_buffer, size_t buffer_len) {

    char *b64, fmt[64];
    uint8_t  buffer[16*KB];
    char sdbf_magic[16], hash_magic[8];
    uint32_t colon_cnt, read_cnt, hash_cnt, b64_len;
    int d_len;
    uint32_t version, name_len;
    uint64_t i;

    if(!formatted_sdbf_buffer || !buffer_len) {
      return false;
    }

    const char* readpp = formatted_sdbf_buffer;
    const char* end    = formatted_sdbf_buffer + buffer_len;

    for( i=0, colon_cnt=3; i<MAX_MAGIC_HEADER && i < buffer_len; i++) {
        buffer[i] = *readpp++;
		if (i==4 && strncmp((char*)buffer,"sdbf",4) )
			throw -2;
        if( buffer[i] == DELIM_CHAR) {
            buffer[i] = 0x20;
            colon_cnt--;
            if( !colon_cnt)
                break;
        }
    }
    if(readpp >= end)
        return false; // end of file prematurely

    buffer[i] = 0; //buffer="sdbf 03 12 "
    sscanf( (char*)buffer, "%s %d %d", sdbf_magic, &version, &name_len);
    LogTrace("read header=[%s] sdbf_magic=%s version=%u name_len=%u", buffer, sdbf_magic, version, name_len);
    if( (strcmp( sdbf_magic, MAGIC_STREAM) && strcmp( sdbf_magic, MAGIC_DD)) || version != 3) {
        if (config->warnings)
            fprintf( stderr, "ERROR: Unsupported format '%s:%02d'. Expecting '%s:03' or '%s:03'\n", sdbf_magic, version, MAGIC_STREAM, MAGIC_DD);
        return false; // unsupported format - caller should exit
    }
    fmt[0] = '%';
    sprintf( fmt+1, "%dc", name_len);
    this->filenamealloc=true;
    this->hashname = (char*)alloc_check( ALLOC_ZERO, name_len+2, "sdbf_from_stream", "this->hashname", ERROR_EXIT);
    //////////////////////////////////////
    //TODO
    {
        // readpp="README.alpha:1197:sha1:256:5:7ff:160:1:19:AAAAAAAAAAAAAEBAAgQAAAAAAAEQAQ..."
        LogTrace("Reading hashname, readpp=%s", readpp);
        read_cnt = sscanf( readpp, fmt, this->hashname);
        readpp = readpp + name_len;
        LogTrace("Reading all, read_cnt=%u hashname=%s readpp=%s", read_cnt, this->hashname, readpp);
        read_cnt = sscanf( readpp, ":%ld:%4s:%d:%d:%x:%d:%d", &(this->orig_file_size), hash_magic, &(this->bf_size), &(this->hash_count), &(this->mask), &(this->max_elem), &(this->bf_count));
        LogTrace("read_cnt=%u orig_file_size=%ld hash_magic=%4s bf_size=%d hash_count=%d mask=%x max_elem=%d bf_count=%d", read_cnt, (this->orig_file_size), hash_magic, (this->bf_size), (this->hash_count), (this->mask), (this->max_elem), (this->bf_count));

        for (colon_cnt = 8; *readpp && colon_cnt > 0; ++i) {
            if (*readpp++ == DELIM_CHAR) {
                colon_cnt--;
            }
        }

        LogTrace("readpp=[%s]", readpp);
    }
    //{
    //    //////////////////////////////////////
    //    LogTrace("Reading hashname, readpp=%s", readpp);
    //    read_cnt = sscanf( readpp, fmt, this->hashname);
    //    readpp = readpp + read_cnt;
    //    LogTrace("Reading all, read_cnt=%u hashname=%s readpp=%s", read_cnt, this->hashname, readpp);
    //    read_cnt = sscanf( readpp, ":%ld:%4s:%d:%d:%x:%d:%d", &(this->orig_file_size), hash_magic, &(this->bf_size), &(this->hash_count), &(this->mask), &(this->max_elem), &(this->bf_count));
    //    readpp = readpp + read_cnt;
    //    LogTrace("read_cnt=%u orig_file_size=%ld hash_magic=%4s bf_size=%d hash_count=%d mask=%x max_elem=%d bf_count=%d", read_cnt, (this->orig_file_size), hash_magic, (this->bf_size), (this->hash_count), (this->mask), (this->max_elem), (this->bf_count));
    //}
    this->buffer = (uint8_t *)alloc_check( ALLOC_ZERO, this->bf_count*this->bf_size, "sdbf_from_stream", "this->buffer", ERROR_EXIT);

    // DD fork
    if( !strcmp( sdbf_magic, MAGIC_DD)) {
        //TODO fix this
        return false;
//        read_cnt = sscanf( readpp, ":%d", &(this->dd_block_size));
//        readpp = readpp + read_cnt;
//        this->elem_counts = (uint16_t *)alloc_check( ALLOC_ZERO, this->bf_count*sizeof(uint16_t), "sdbf_from_stream", "this->elem_counts", ERROR_EXIT);
//        for( i=0; i<this->bf_count; i++) {
//            read_cnt = sscanf( readpp, ":%2x:%344s", &hash_cnt, buffer);
//            readpp = readpp + read_cnt;
//            this->elem_counts[i] = (uint16_t)hash_cnt;
//            d_len = b64decode_into( buffer, 344, this->buffer + i*this->bf_size);
//            if( d_len != 256) {
//                if (config->warnings)
//                    fprintf( stderr, "ERROR: Unexpected decoded length for BF: %d. name: %s, BF#: %d\n", d_len, this->hashname, (int)i);
//                throw -2; // unsupported format - caller should exit
//            }
//        }
    // Stream fork
    } else {
        read_cnt = sscanf( readpp, "%d:", &(this->last_count));
        LogTrace("this->last_count=%u readpp=[%s] ", this->last_count, readpp);
        readpp = strchr(readpp + 1, ':'); readpp++;
        LogTrace("readpp=[%s]", readpp);
        b64_len = this->bf_count*this->bf_size;
        b64_len = 4*(b64_len/3 +1*(b64_len % 3 > 0 ? 1 : 0));
        sprintf( &fmt[1], "%ds", b64_len);
        b64 = (char*)alloc_check( ALLOC_ZERO, b64_len+2, "sdbf_from_stream", "b64", ERROR_EXIT);
        read_cnt = sscanf( readpp, fmt, b64);
        readpp = readpp + read_cnt;
        LogTrace("b64_len=%d b64=[%s]", b64_len, b64);
		free(this->buffer);
        this->buffer =(uint8_t*) b64decode( (char*)b64, (int)b64_len, &d_len);
        if( d_len != this->bf_count*this->bf_size) {
            if (config->warnings)
                fprintf( stderr, "ERROR: Incorrect base64 decoding length. Expected: %d, actual: %d\n", this->bf_count*this->bf_size, d_len);
            free (b64); // cleanup in case of wanting to go on
            return false; // unsupported format, caller should exit
        }
        free( b64);
    }
    compute_hamming();
    this->info=NULL;
    return true;
}

/**
    Destroys this sdbf
*/
sdbf::~sdbf() {
    if (buffer)
        free(buffer);
    if (hamming)
        free(hamming);
    if (elem_counts)
        free(elem_counts);
    if (filenamealloc)
	free(hashname);
} 

/**
    Returns the name of the file or data this sdbf represents.
    \returns char* of file name
*/
const char *
sdbf::name() {
    return (char*)this->hashname;
} 

/** 
    Returns the size of the hash data for this sdbf
    \returns uint64_t length value
*/
uint64_t
sdbf::size() {
    return (this->bf_size)*(this->bf_count);
}

/** 
    Returns the size of the data that the hash was generated from.
    \returns uint64_t length value
*/
uint64_t
sdbf::input_size() {
    return this->orig_file_size;
}


/**
 * Compares this sdbf to other passed sdbf, returns a confidence score
    \param other sdbf* to compare to self
    \param map_on turns on a heat map
    \param sample sets the number of BFs to sample - 0 uses all
    \returns int32_t confidence score
*/
int32_t
sdbf::compare( sdbf *other, uint32_t map_on, uint32_t sample) {
    if (config->warnings)
        cerr << this->name() << " vs " << other->name() << endl;

    return sdbf_score( this, other, map_on, sample);
}

/** 
    Write this sdbf to stream 
*/
ostream& operator<<(ostream& os, const sdbf& s) {
    os << s.to_string();
    return os;
}

/**
    Write sdbf to stream 
*/
ostream& operator<<(ostream& os, const sdbf *s) {
    os << s->to_string();
    return os;
}

/**
    Encode this sdbf and return it as a string.
    \returns std::string containing sdbf suitable for display or writing to file
*/
string
sdbf::to_string () const { // write self to stream
    std::stringstream hash;
    // Stream version
    if( !this->elem_counts) {
        hash.fill('0');
        hash << MAGIC_STREAM << ":" << setw (2) << SDBF_VERSION << ":";    
        hash << (int)strlen((char*)this->hashname) << ":" << this->hashname << ":" << this->orig_file_size << ":sha1:";    
        hash << this->bf_size << ":" << this->hash_count<< ":" << hex << this->mask << ":" << dec;    
        hash << this->max_elem << ":" << this->bf_count << ":" << this->last_count << ":";    
        uint64_t qt = this->bf_count/6, rem = this->bf_count % 6;
        uint64_t i, pos=0, b64_block = 6*this->bf_size;
        for( i=0,pos=0; i<qt; i++,pos+=b64_block) {
            char *b64 = b64encode( (char*)this->buffer + pos, b64_block);
            hash << b64;
            free( b64);
        }
        if( rem>0) {
            char *b64 = b64encode( (char*)this->buffer + pos, rem*this->bf_size);
            hash << b64;
            free( b64);
        }
    } else { // block version
        hash.fill('0');
        hash << MAGIC_DD << ":" << setw (2) << SDBF_VERSION << ":";    
        hash << (int)strlen((char*)this->hashname) << ":" << this->hashname << ":" << this->orig_file_size << ":sha1:";    
        hash << this->bf_size << ":" << this->hash_count<< ":" << hex << this->mask << ":" << dec;    
        hash << this->max_elem << ":" << this->bf_count << ":" << this->dd_block_size ;
        int i;
        for( i=0; i<this->bf_count; i++) {
            char *b64 = b64encode( (char*)this->buffer+i*this->bf_size, this->bf_size);
            hash << ":" << setw (2) << hex << this->elem_counts[i];
            hash << ":" << b64;    
            free(b64);
        }
    }
    hash << endl;
    return hash.str();
}

string
sdbf::get_index_results() const{
    return index_results;
}
/** 
    Clones a copy of a single bloom filter in
    this sdbf.  
    
    Warning: 256-bytes long, not terminated, may contain nulls.

    \param position index of bloom filter
    \returns uint8_t* pointer to 256-byte long bloom filter
*/
uint8_t*
sdbf::clone_filter(uint32_t position) {
    if (position < this->bf_count) {
        uint8_t *filter=(uint8_t*)alloc_check(ALLOC_ZERO,bf_size*sizeof(uint8_t),"single_bloom_filter","return buffer",ERROR_EXIT);
        memcpy(filter,this->buffer + position*bf_size,bf_size);
        return filter;    
    } else {
        return NULL;
    }
}

sdbf::sdbf() {
    sdbf_create(NULL);
}
        
/** \internal
 * Create and initialize an sdbf structure ready for stream mode.
 */
void 
sdbf::sdbf_create(const char *name) {
    this->hashname = (char*)name;
    this->bf_size = config->bf_size;
    this->hash_count = 5;
    this->mask = config->BF_CLASS_MASKS[0];
    this->max_elem = config->max_elem;
    this->bf_count = 1;
    this->last_count = 0;
    this->elem_counts= 0;
    this->dd_block_size = 0;
    this->orig_file_size = 0;
    this->hamming = NULL;
    this->buffer = NULL;
    this->info=NULL;
    this->filenamealloc=false;
}


/** \internal
 * Pre-compute Hamming weights for each BF and adds them to the SDBF descriptor.
 */ 
int 
sdbf::compute_hamming() {
    uint32_t pos, bf_count = this->bf_count;
    this->hamming = (uint16_t *) alloc_check( ALLOC_ZERO, bf_count*sizeof( uint16_t), "compute_hamming", "this->hamming", ERROR_EXIT);
        
    uint64_t i, j;
    uint16_t *buffer16 = (uint16_t *)this->buffer;
    for( i=0,pos=0; i<bf_count; i++) {
        for( j=0; j<BF_SIZE/2; j++,pos++) {
            this->hamming[i] += config->bit_count_16[buffer16[pos]];
        }
    }
    return 0;
}

/** \internal
   get element count for comparisons 
*/
int32_t 
sdbf::get_elem_count( sdbf *mine,uint64_t index) {
    if( !mine->elem_counts) {
        return (index < mine->bf_count-1) ? mine->max_elem : mine->last_count; 
    // DD fork
    } else {
        return mine->elem_counts[index];
    }

}

uint32_t
sdbf::filter_count() {
    return bf_count;
}

