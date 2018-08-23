/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#pragma once
#include <appbase/application.hpp>
#include <iostream>
#include <eosio/chain_plugin/chain_plugin.hpp>
#include <eosio/http_client_plugin/http_client_plugin.hpp>
#include <fc/io/json.hpp>
#include <eosio/client_plugin/client_manager.hpp>
namespace eosio {

using namespace appbase;

 namespace client {
  namespace http {
      class client_manager;
   }
 }

using namespace client::http;
using namespace eosio::chain;
using boost::signals2::signal;


namespace client_apis{

   class client_cactus{

      public:
      client_cactus();
      virtual ~client_cactus(){}


      eosio::chain_apis::read_only::get_info_results  get_info( const std::string& url);
      fc::variant get_transaction( const std::string& url, uint32_t block_num_hint,
                                 string transaction_id_str);
      void push_action(const std::string& url, string contract, string action, string data,
                        const vector<string>& tx_permission );
      void send_actions(const std::string& url, std::vector<chain::action>&& actions, int32_t extra_kcpu = 1000, packed_transaction::compression_type compression = packed_transaction::none);
      fc::variant push_actions(const std::string& url, std::vector<chain::action>&& actions, int32_t extra_kcpu, packed_transaction::compression_type compression = packed_transaction::none);
      fc::variant push_transaction( const std::string& url, signed_transaction& trx, int32_t extra_kcpu = 1000, packed_transaction::compression_type compression = packed_transaction::none);


      private:
      struct queued_write {
         connection_param& cp;
         fc::variant postdata;
         std::function<void(fc::variant&)> callback;
      };
      deque<queued_write>     write_queue;
      deque<queued_write>     out_queue;

      template<typename T>
      fc::variant call( const std::string& url,
                     const std::string& path,
                     const T& v );

      fc::variant json_from_file_or_string(const string& file_or_str, fc::json::parse_type ptype);
      vector<chain::permission_level> get_account_permissions(const vector<string>& permissions);
      void sign_transaction_local(signed_transaction& trx,  const private_key_type& private_key, const chain_id_type& chain_id);
      // void client_write(connection_param& cp, fc::variant postdata, std::function<void(fc::variant&)> callback);
      // void do_client_write();

   };

}



   class client_plugin : public appbase::plugin<client_plugin> {
      public:
         client_plugin();
         virtual ~client_plugin();

         APPBASE_PLUGIN_REQUIRES((chain_plugin))
         virtual void set_program_options(options_description&, options_description& cfg) override;

         void plugin_initialize(const variables_map& options);
         void plugin_startup();
         void plugin_shutdown();

         client_apis::client_cactus  get_client_apis()const {
            return client_apis::client_cactus(); }

      private:

         std::unique_ptr<client_apis::client_cactus> client_cactus_ptr;

   };



}
