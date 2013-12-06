#define BOOST_TEST_MODULE BitSharesTest
#include <boost/test/unit_test.hpp>
#include <bts/wallet.hpp>
#include <bts/address.hpp>
#include <fc/exception/exception.hpp>
#include <fc/log/logger.hpp>
#include <fc/reflect/variant.hpp>
#include <bts/blockchain/asset.hpp>
#include <fc/crypto/hex.hpp>
#include <bts/blockchain/blockchain_db.hpp>
#include <bts/config.hpp>
#include <fc/io/json.hpp>
#include <fc/io/raw.hpp>
#include <fc/filesystem.hpp>
#include <bts/blockchain/blockchain_printer.hpp>
#include <bts/keychain.hpp>
#include <bts/bitname/bitname_db.hpp>
#include <bts/bitname/bitname_block.hpp>
#include <fstream>

using namespace bts;
using bts::blockchain::asset;
using bts::blockchain::price;
using namespace bts::blockchain;

BOOST_AUTO_TEST_CASE( bitname_db_test )
{
  return; // TODO: fix this test.
  try {
    fc::temp_directory temp_dir;
    bts::bitname::name_db chain;
    chain.open( temp_dir.path() / "chain" );

  // genesis block is added by default... 
  //  bts::bitname::name_block genesis = bts::bitname::create_genesis_block();
  //  chain.push_block( genesis );
  //  BOOST_REQUIRE_THROW( chain.push_block( genesis ), fc::exception );

  // fetch current head... 
    bts::bitname::name_block block1;
    block1.utc_sec = fc::time_point::now();
    block1.age = 1;
    block1.name_hash = 10001;
    block1.key = fc::ecc::private_key::generate().get_public_key();
    block1.trxs_hash = block1.calc_trxs_hash();
    block1.repute_points = 1;
    block1.prev = chain.head_block_id();

    auto target = chain.target_difficulty();
    ilog( "target: ${t}    block: ${b}", ("t",target)("b",block1.difficulty() ) );
    while( block1.difficulty() < target )
    {
      block1.nonce++;
      block1.utc_sec = fc::time_point::now();
      if( block1.nonce % 10000 == 0 )
      {
        ilog( "target: ${t}    block: ${b}", ("t",target)("b",block1.difficulty() ) );
      }
    }
    ilog( "target: ${t}    block: ${b}", ("t",target)("b",block1.difficulty() ) );

    chain.push_block( block1 );
    BOOST_REQUIRE_THROW( chain.push_block( block1 ), fc::exception );


    bts::bitname::name_block block2;
    block2.age = 2;
    block2.repute_points = 1;
    block2.utc_sec = fc::time_point::now();
    block2.name_hash = 2000;
    block2.key = fc::ecc::private_key::generate().get_public_key();
    
    for( uint32_t i = 0; i < 10; ++i )
    {
       bts::bitname::name_trx trx;
       trx.name_hash = i+1001;
       trx.age = 2;
       trx.repute_points = 1;
       trx.utc_sec = block2.utc_sec;
       trx.key = fc::ecc::private_key::generate().get_public_key();

       block2.name_trxs.push_back(trx);
    }
    block2.prev = block1.id();
    block2.trxs_hash = block2.calc_trxs_hash();
    auto test_mroot = block2.calc_trxs_hash();
    BOOST_REQUIRE( block2.trxs_hash == test_mroot );


    target = chain.target_difficulty();
    while( block2.difficulty() < target )
    {
      block2.nonce++;
      //ilog( "target: ${t}    block: ${b}", ("t",target)("b",block2.difficulty() ) );
    }
    //ilog( "target: ${t}    block: ${b}", ("t",target)("b",block2.difficulty() ) );


    chain.push_block( block2 );
    BOOST_REQUIRE_THROW( chain.push_block( block2 ), fc::exception );

    // add a new block identical to prior block, just updating the
    // linkages...this should fail because the renewal should increment.
    block2.prev  = block2.id();
    block2.age   = 3;
    block2.trxs_hash = block2.calc_trxs_hash();
    BOOST_REQUIRE_THROW( chain.push_block( block2 ), fc::exception );

    for( uint32_t i = 0; i < block2.name_trxs.size(); ++i )
    {
        block2.name_trxs[i].repute_points.value++;
    }
    block2.trxs_hash = block2.calc_trxs_hash();

    target = chain.target_difficulty();
    while( block2.difficulty() < target )
    {
      block2.nonce++;
      ilog( "target: ${t}    block: ${b}", ("t",target)("b",block2.difficulty() ) );
    }

    chain.push_block( block2 ); // it should work this time...
    BOOST_REQUIRE_THROW( chain.push_block( block2 ), fc::exception );
  }
  catch ( const fc::exception& e )
  {
    elog( "${e}", ("e",e.to_detail_string()) );
    throw;
  }
}

BOOST_AUTO_TEST_CASE( keychain_test )
{
  try {
    auto priv  = fc::ecc::private_key::generate().get_secret();
    auto chain = fc::sha256::hash(  "world", 5 );
   
    extended_private_key epk( priv, chain );
    extended_private_key c1 = epk.child( 1, true );
   
    extended_public_key  epubk( epk.get_public_key(), chain );
    extended_public_key  pub_c1 = epubk.child( 1 );
   
    ilog( "ext_priv_key: ${epk}", ("epk",epk) );
    ilog( "ext_pub_key: ${epubk}", ("epubk",epubk) );

    ilog( "priv_child: ${c1}", ("c1",c1) );
    ilog( "priv_child.pub: ${c1}", ("c1",c1.get_public_key()) );
    ilog( "pub_child:      ${pub_c1}", ("pub_c1",pub_c1) );

    BOOST_REQUIRE( c1.get_public_key() == pub_c1.pub_key );

    keychain wal;
    wal.set_seed( fc::sha512::hash( "hello", 5 ) );
    BOOST_REQUIRE( wal.get_private_account( "test", 1 ).get_public_key() ==
                   wal.get_public_account( "test", 1 ).pub_key );

    BOOST_REQUIRE( wal.get_private_trx( "test", 1, 2 ).get_public_key() ==
                   wal.get_public_trx( "test", 1, 2 ).pub_key );

    BOOST_REQUIRE( wal.get_private_trx_address( "test", 1, 3, 4 ).get_public_key() ==
                   wal.get_public_trx_address( "test", 1, 3, 4 ) );

  }
  catch ( const fc::exception& e )
  {
    elog( "${e}", ("e",e.to_detail_string()) );
    throw;
  }
}

/*
BOOST_AUTO_TEST_CASE( mining_reward_rate )
{
  uint64_t supply = 0;
  for( uint32_t i = 0; i < BLOCKS_WITH_REWARD; ++i )
  {
     supply += bts::blockchain::calculate_mining_reward(i);
     if( i == 0 ) supply /= 2; // dividends from gensis block do not count toward supply
  }
  ilog( "INITIAL_REWARD ${i}", ("i",INITIAL_REWARD) );
  ilog( "BLOCKS_WITH_REWARD ${i}", ("i",BLOCKS_WITH_REWARD) );
  ilog( "REWARD_DELTA_PER_BLOCK ${i}", ("i",REWARD_DELTA_PER_BLOCK) );
  ilog( "LAST_BLOCK_REWARD ${i}", ("i",calculate_mining_reward(BLOCKS_WITH_REWARD)) );
  ilog( "LAST_BLOCK_REWARD+1 ${i}", ("i",calculate_mining_reward(BLOCKS_WITH_REWARD+1)) );
  ilog( "end supply: ${e}  target supply ${t}", ("e",supply)("t",MAX_BITSHARE_SUPPLY) );
  ilog( "error: ${e}       error per block: ${epp}", ("e", (supply-MAX_BITSHARE_SUPPLY) )("epp", (supply-MAX_BITSHARE_SUPPLY)/BLOCKS_WITH_REWARD ) );
  BOOST_REQUIRE( supply == MAX_BITSHARE_SUPPLY );
  BOOST_REQUIRE( 0 == calculate_mining_reward(BLOCKS_WITH_REWARD+1));
  BOOST_REQUIRE( 0 == calculate_mining_reward(BLOCKS_WITH_REWARD));
}
*/

BOOST_AUTO_TEST_CASE( fixed_math )
{
  try{
   asset usd( 30000, asset::usd );
   asset bts( 5000, asset::bts );
   auto  usd_bts = usd / bts;
   auto  bts_usd = bts / usd;
   
   ilog( "usd: ${u}  bts: ${b}", ("u",usd)("b",bts) );
   ilog( "usd_bts = ${ub}   bts_usd = ${bu}", ("ub", usd_bts)("bu",bts_usd) );
    
   asset usd56( 5, asset::usd );
   asset bts43( 43, asset::bts );

   auto bts43_in_usd =  bts43 * bts_usd;
   auto back_to_bts = bts43_in_usd * bts_usd;
   ilog( " ${b} * ${p}  =>  ${a}", ("b", bts43)("p",bts_usd)("a",bts43_in_usd) );
   ilog( " ${b} * ${p}  =>  ${a}", ("b", bts43_in_usd)("p",bts_usd)("a",back_to_bts) );
   BOOST_CHECK( back_to_bts.amount.high_bits() == bts43.amount.high_bits() );
   {
   asset usd( 5, asset::usd );
   asset bts( 3, asset::bts );
   auto  usd_bts = usd / bts;
   auto  bts_usd = bts / usd;
   
   ilog( "usd: ${u}  bts: ${b}", ("u",usd)("b",bts) );
   ilog( "usd_bts = ${ub}   bts_usd = ${bu}", ("ub", usd_bts)("bu",bts_usd) );
   }

  } catch ( const fc::exception& e )
  {
    elog( "${e}", ("e",e.to_detail_string()) );
    throw;
  }
}

#if 0
/**
 *  Test the process of validating the block chain given
 *  a known initial condition and fixed transactions. 
 */
BOOST_AUTO_TEST_CASE( blockchain_build )
{
  try 
  {
//    for( uint32_t i = 0; i < 1000; ++i )
    {
     fc::ecc::private_key k1 = fc::ecc::private_key::generate_from_seed( fc::sha256::hash( "block1", 6 ) );
     bts::address a1 = k1.get_public_key();
     fc::ecc::private_key k2 = fc::ecc::private_key::generate_from_seed( fc::sha256::hash( "block2", 6 ) );
     bts::address a2 = k2.get_public_key();
     fc::ecc::private_key k3 = fc::ecc::private_key::generate_from_seed( fc::sha256::hash( "block3", 6 ) );
     bts::address a3 = k3.get_public_key();
     fc::ecc::private_key k4 = fc::ecc::private_key::generate_from_seed( fc::sha256::hash( "addre4", 6 ) );
     bts::address a4 = k4.get_public_key();
     fc::ecc::private_key k5 = fc::ecc::private_key::generate_from_seed( fc::sha256::hash( "addre5", 6 ) );
     bts::address a5 = k5.get_public_key();
     fc::ecc::private_key k6 = fc::ecc::private_key::generate_from_seed( fc::sha256::hash( "addre6", 6 ) );
     bts::address a6 = k6.get_public_key();
     fc::ecc::private_key k7 = fc::ecc::private_key::generate_from_seed( fc::sha256::hash( "addre7", 6 ) );
     bts::address a7 = k7.get_public_key();
     
     fc::temp_directory temp_dir;
     bts::blockchain::blockchain_db chain;
     chain.open( temp_dir.path() / "chain" );
    
     auto genesis = create_genesis_block();
     ilog( "genesis block: \n${s}", ("s", fc::json::to_pretty_string(genesis) ) );
     chain.push_block( genesis );

     // build next block... 
     auto block1 = chain.generate_next_block( a1, std::vector<signed_transaction>() );
     ilog( "next block: \n${s}", ("s", fc::json::to_pretty_string(block1) ) );
     chain.push_block( block1 );

     auto block2 = chain.generate_next_block( a2, std::vector<signed_transaction>() );
     ilog( "next block: \n${s}", ("s", fc::json::to_pretty_string(block2) ) );
     chain.push_block( block2 );

     std::vector<signed_transaction> new_trx;
     new_trx.resize(1);

     new_trx[0].inputs.push_back( 
        trx_input( output_reference( block1.trxs[0].id(), 0 ) ) );
     new_trx[0].outputs.push_back( 
        trx_output( claim_by_signature_output( address( a4 ) ), 100000000, asset::bts ) );
     new_trx[0].outputs.push_back( 
        trx_output( claim_by_signature_output( address( a5 ) ), 314000000, asset::bts ) );

     new_trx[0].sign( k1 );
     new_trx.push_back( new_trx[0] ); // try it twice!
     new_trx[1].outputs.resize(1);
     new_trx[1].sigs.clear();
     new_trx[1].sign( k1 );

     wlog( "trx0: ${eval}", ("eval", chain.evaluate_signed_transaction( new_trx[0] ) ) );
     wlog( "trx1: ${eval}", ("eval", chain.evaluate_signed_transaction( new_trx[1] ) ) );

     auto block3 = chain.generate_next_block( a3, new_trx );
     ilog( "next block: \n${s}", ("s", fc::json::to_pretty_string(block3) ) );
     chain.push_block( block3 );
     
     // this should throw now that the exception has been added to the block chain and the
     // input marked as spent
     BOOST_REQUIRE_THROW( chain.evaluate_signed_transaction(new_trx[0]), fc::exception  );

     new_trx.clear();
     new_trx.resize(1);
     new_trx[0].inputs.push_back( 
        trx_input( output_reference( block3.trxs[1].id(), 0 ) ) );

     new_trx[0].outputs.push_back( 
        trx_output( claim_by_signature_output( address( a7 ) ), 50000000, asset::bts ) );

     new_trx[0].sign( k4 );

     auto block4 = chain.generate_next_block( a6, new_trx );
     ilog( "next block: \n${s}", ("s", fc::json::to_pretty_string( block4 ) ) );
     chain.push_block( block4 );
     BOOST_REQUIRE_THROW( chain.evaluate_signed_transaction( new_trx[0] ), fc::exception  );

     wlog( "================ Bid Test =======================" );
        // 100 BitUSD / 10,000,000 BTS
        auto bid_price = bts::blockchain::asset( 100, asset::usd ) / bts::blockchain::asset( 10000000, asset::bts );
     
        std::vector<signed_transaction> bid_trxs;
        bid_trxs.resize( 1 );
        
        bid_trxs[0].inputs.push_back( // input 50000000 bts
           trx_input( output_reference( block4.trxs[1].id(), 0 ) ) );

        bid_trxs[0].outputs.push_back( 
           trx_output( claim_by_bid_output( address(a7), bid_price, 10000000 /*min bts*/ ), 50000000, asset::bts ) ); 
        
        bid_trxs[0].sign( k7 );

         auto block5 = chain.generate_next_block( a6, bid_trxs );
         ilog( "next block: \n${s}", ("s", fc::json::to_pretty_string( block5 ) ) );
         chain.push_block( block5 );

         bid_trxs.clear();
         bid_trxs.resize(1);

         bid_trxs[0].inputs.push_back(
            trx_input( output_reference( block2.trxs[0].id(), 0 ) ) );

         bid_trxs[0].outputs.push_back( 
            trx_output( claim_by_long_output( address(a2), bid_price, 10000000 /*min bts*/ ), 1500000000l, asset::bts ) ); 
        
         bid_trxs[0].sign( k2 );
     
         auto block6 = chain.generate_next_block( a3, bid_trxs );
         ilog( "next block: \n${s}", ("s", fc::json::to_pretty_string( block6 ) ) );
         chain.push_block( block6 );

         bid_trxs.clear();
         auto block7 = chain.generate_next_block( a4, bid_trxs );
         ilog( "next block: \n${s}", ("s", fc::json::to_pretty_string( block7 ) ) );
         chain.push_block( block7 );

         auto block8 = chain.generate_next_block( a4, bid_trxs );
         ilog( "next block: \n${s}", ("s", fc::json::to_pretty_string( block8 ) ) );
         chain.push_block( block8 );

         auto block9 = chain.generate_next_block( a5, bid_trxs );
         ilog( "next block: \n${s}", ("s", fc::json::to_pretty_string( block9 ) ) );
         chain.push_block( block9 );

         bid_trxs.resize(2);

         bid_trxs[0].inputs.push_back(
            trx_input( output_reference( block8.trxs[0].id(), 0 ) ) );

         bid_trxs[0].outputs.push_back( 
            trx_output( claim_by_long_output( address(a4), bid_price, 20000000 /*min bts*/ ), 1500000000l, asset::bts ) ); 
        
         bid_trxs[0].sign( k4 );
         
         bid_trxs[1].inputs.push_back( 
            trx_input( output_reference( block6.trxs[0].id(), 0 ) ) );
         
         bid_trxs[1].outputs.push_back( 
            trx_output( claim_by_bid_output( address(a6), bid_price, 30000000 /*min bts*/ ), 90000000, asset::bts ) ); 
         
         bid_trxs[1].sign( k3 );

         auto block10 = chain.generate_next_block( a5, bid_trxs );
         ilog( "next block: \n${s}", ("s", fc::json::to_pretty_string( block10 ) ) );
         chain.push_block( block10 );

         bid_trxs.clear();
         auto block11 = chain.generate_next_block( a5, bid_trxs );
         ilog( "next block: \n${s}", ("s", fc::json::to_pretty_string( block11 ) ) );
         chain.push_block( block11 );
     /*
     ilog( "\n${block}", ("block", 
     ilog( "\n${block}", ("block", bts::blockchain::pretty_print( block1, chain ) ) );
     ilog( "\n${block}", ("block", bts::blockchain::pretty_print( block2, chain ) ) );
     ilog( "\n${block}", ("block", bts::blockchain::pretty_print( block3, chain ) ) );
     ilog( "\n${block}", ("block", bts::blockchain::pretty_print( block4, chain ) ) );
     */
     std::ofstream html( "chain.html" );
     html << bts::blockchain::pretty_print( genesis, chain );
     html << bts::blockchain::pretty_print( block1, chain );
     html << bts::blockchain::pretty_print( block2, chain );
     html << bts::blockchain::pretty_print( block3, chain );
     html << bts::blockchain::pretty_print( block4, chain );
     html << bts::blockchain::pretty_print( block5, chain );
     html << bts::blockchain::pretty_print( block6, chain );
     html << bts::blockchain::pretty_print( block7, chain );
     html << bts::blockchain::pretty_print( block8, chain );
     html << bts::blockchain::pretty_print( block9, chain );
     html << bts::blockchain::pretty_print( block10, chain );
     html << bts::blockchain::pretty_print( block11, chain );
  
     html << chain.dump_market( asset::usd, asset::bts );
    
      /*
     for( uint32_t i = 0; i < 100; ++i )
     {
         fc::ecc::private_key k7 = fc::ecc::private_key::generate_from_seed( fc::sha256::hash( (char*)&i, 4 ) );
         bts::address a7 = k7.get_public_key();
         auto blockN = chain.generate_next_block( a7, std::vector<signed_transaction>() );
         chain.push_block( blockN );
     }
     */
   } // loop...
  } 
  catch ( const fc::exception& e )
  {
     elog( "${e}", ("e",e.to_detail_string()) );
     throw;
  }
}
#endif

BOOST_AUTO_TEST_CASE( bts_address )
{
  try 
  {
     auto priv = fc::ecc::private_key::generate();
     bts::address addr =  priv.get_public_key();
     std::string addrstr = addr;
     bts::address addr2 = addrstr;
     bts::address addr3 =  priv.get_public_key();
     
     BOOST_REQUIRE( addr == addr2 );
     BOOST_REQUIRE( addr3 == addr2 );
     BOOST_REQUIRE_THROW( bts::address("invalid"), fc::exception);
     BOOST_REQUIRE_THROW( bts::address("a23"), fc::exception);
     ilog( "address ${adr}", ("adr", addrstr ) );
     ilog( "address ${adr}", ("adr", addr ) );
  } catch( fc::exception& e )
  {
    elog( "${e}", ("e",e.to_detail_string()) );
    throw;
  }
}
BOOST_AUTO_TEST_CASE( wallet_test )
{
/* TODO: this test is slow...
   bts::wallet w;
   w.set_seed( fc::sha256::hash( "helloworld", 10 ) );

   bts::wallet w2;
   w2.set_master_public_key( w.get_master_public_key() );

   for( uint32_t i = 0; i < 10; ++i )
   {
      BOOST_CHECK(  bts::address(w.get_public_key(i))                   == w2.get_public_key(i) );
      BOOST_CHECK(  bts::address(w.get_private_key(i).get_public_key()) == w2.get_public_key(i) );
   }
   */
}

