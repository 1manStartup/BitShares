#pragma once
#include <bts/addressbook/contact.hpp>
#include <fc/crypto/elliptic.hpp>
#include <fc/crypto/sha512.hpp>

#include <unordered_map>

namespace fc { class path; }

namespace bts { namespace addressbook {

  namespace detail { class addressbook_impl; }

  /**
   *  Provides indexes for effecient lookup of contacts
   *  and abstracts the storage of the addressbook on
   *  disk.
   */
  class addressbook 
  {
     public:
        addressbook();
        ~addressbook();

        void open( const fc::path& abook_dir, const fc::uint512& key );

        /**
         * @return a list of all known bitname_label's that can be used to lookup
         *         contacts.
         */
        std::vector<std::string> get_known_bitnames()const;

        const std::unordered_map<uint32_t,contact>& get_contacts()const;
        fc::optional<contact> get_contact_by_bitname( const std::string& bitname_label    )const;
        std::string           get_bitname_by_address( const fc::ecc::public_key& bitname_key )const;
        void                  store_contact( const contact& contact_to_store );

     private:
        std::unique_ptr<detail::addressbook_impl> my;
  };

  typedef std::shared_ptr<addressbook> addressbook_ptr;

} } // bts::addressbook
