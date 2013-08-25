#include <bts/proof_of_work.hpp>
#include <fc/crypto/sha512.hpp>
#include <fc/crypto/sha256.hpp>
#include <fc/crypto/aes.hpp>
#include <fc/crypto/salsa20.hpp>
#include <fc/crypto/city.hpp>
#include <string.h>

#include <fc/io/raw.hpp>
#include <fc/crypto/ripemd160.hpp>
#include <utility>
#include <fc/log/logger.hpp>

#define BUF_SIZE (8*1024*1024)
#define BLOCK_SIZE (32) // bytes

namespace bts  {

pow_hash proof_of_work( const fc::sha256& in)
{
   unsigned char* buf = new unsigned char[BUF_SIZE];
   pow_hash out;
   try {
     out = proof_of_work( in, buf );
   } catch ( ... )
   {
    delete [] buf;
    throw;
   }
   delete[] buf;
   return out;
}


/**
 *  This proof-of-work is computationally difficult even for a single hash,
 *  but must be so to prevent optimizations to the required memory foot print.
 *
 *  The maximum level of parallelism achievable per GB of RAM is 8, and the highest
 *  end GPUs now have 4 GB of ram which means they could in theory support 32 
 *  parallel execution of this proof-of-work.     
 *
 *  On GPU's you only tend to get 1 instruction per 4 clock cycles in a single
 *  thread context.   Modern super-scalar CPU's can get more than 1 instruction
 *  per block and CityHash is specifically optomized to take advantage of this. 
 *  In addition to getting more done per-cycle, CPU's have close to 4x the clock
 *  frequency.
 *
 *  Based upon these characteristics alone, I estimate that a CPU can execute the
 *  serial portions of this algorithm at least 16x faster than a GPU which means
 *  that an 8-core CPU should easily compete with a 128 core GPU. Fortunately,
 *  a 128 core GPU would require 16 GB of RAM.  Note also, that most GPUs have 
 *  less than 128 'real' cores that are able to handle conditionals. 
 *
 *  Further more, GPU's are not well suited for branch misprediction and code
 *  must be optimized to avoid branches as much as possible.  
 *
 *  Lastly this algorithm takes advantage of a hardware instruction that is
 *  unlikely to be included in GPUs (Intel CRC32).  The lack of this hardware
 *  instruction alone is likely to give the CPU an order of magnitude advantage
 *  over the GPUs.
 */
pow_hash proof_of_work( const fc::sha256& seed, unsigned char* buffer )
{
   auto key = fc::sha256(seed);
   auto iv  = fc::city_hash128((char*)&seed,sizeof(seed));
   memset( buffer, 0, BUF_SIZE );

   uint64_t* read_pos  = (uint64_t*)(buffer);
   uint64_t* write_pos = (uint64_t*)(buffer+32);

   fc::aes_encoder enc( key, iv );
   for( uint32_t i = 0; i < BUF_SIZE / (BLOCK_SIZE/2); ++i )
   {
      uint64_t wrote = enc.encode( (char*)read_pos, BLOCK_SIZE, (char*)write_pos ); 
      FC_ASSERT( wrote == BLOCK_SIZE );
      read_pos  =  (uint64_t*)( buffer + (write_pos[0]) % ( BUF_SIZE - BLOCK_SIZE ) );
      write_pos =  (uint64_t*)( buffer + (write_pos[1]) % ( BUF_SIZE - BLOCK_SIZE ) );
   }
   auto midstate =  fc::city_hash_crc_256( (char*)buffer, BUF_SIZE ); 
   return fc::ripemd160::hash((char*)&midstate, sizeof(midstate) );
}


}  // namespace bts
