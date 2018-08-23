/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#include <eosio/chain/exceptions.hpp>
#include <iostream>
#include <regex>
#include <fc/variant.hpp>
#include <fc/io/json.hpp>

#include <boost/asio.hpp>
#include <boost/format.hpp>
#include <boost/dll/runtime_symbol_info.hpp>
#include <boost/filesystem.hpp>
#include <boost/process.hpp>
#include <boost/process/spawn.hpp>
#include <boost/range/algorithm/find_if.hpp>
#include <boost/range/algorithm/sort.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/range/algorithm/copy.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <eosio/client_plugin/client_plugin.hpp>
#include <eosio/client_plugin/client_manager.hpp>


namespace eosio {
    using namespace client::http;
    static appbase::abstract_plugin& _client_plugin = app().register_plugin<client_plugin>();

    bool no_verify = false;
    auto   tx_expiration = fc::seconds(30);
    uint8_t  tx_max_cpu_usage = 0;
    uint32_t tx_max_net_usage = 0;

    chain::private_key_type client_prikey;

    client_plugin::client_plugin(){}
    client_plugin::~client_plugin(){}

    void client_plugin::set_program_options(options_description&, options_description& cfg) {
        cfg.add_options()
              ("client-private-key", bpo::value<string>()->default_value("5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3"),
               "client plugin's private key")
          ;
    }

    void client_plugin::plugin_initialize(const variables_map& options){
        try {
            if( options.count("client-private-key") )
             {
                client_prikey = private_key_type(options.at( "client-private-key" ).as<string>());
                ilog("Keosd use private key: ${priv}",("priv", client_prikey));
             }

        }FC_LOG_AND_RETHROW()

    }

    void client_plugin::plugin_startup(){
        ilog("initializing client plugin");

      // auto client_apis = app().get_plugin<client_plugin>().get_client_apis();
      // auto info = client_apis.get_info("http://127.0.0.1:8888");
      //   std::cout << fc::json::to_pretty_string(info) << std::endl;
      // string chain_url = "http://127.0.0.1:8888";
         // auto transaction_info = client_apis.get_transaction(chain_url,10022,"3331399d4c1d5ebe8aa67c9e429b9137417794448b5060c2112ae5624bd8c7ba");
         // std::cout << fc::json::to_pretty_string(transaction_info) << std::endl;

         // std::vector<string> v;
         // v.push_back("eosio");
         // client_apis.push_action(chain_url,"cactus","transfer","[\"eosio\",\"cactus\",\"100.0000 SYS\", \"m\"]",v);

         // /cleos push action cactus transfer '["eosio","cactus","25.0000 SYS", "m"]' -p eosio
    }

    void client_plugin::plugin_shutdown(){}

namespace client_apis{

     client_cactus::client_cactus(){
    }


    eosio::chain_apis::read_only::get_info_results client_cactus::get_info( const std::string& url) {
        auto info = call(url, get_info_func, fc::variant()).as<eosio::chain_apis::read_only::get_info_results>();
        return info;
    }

    fc::variant client_cactus::get_transaction( const std::string& url, uint32_t block_num_hint,
                                             string transaction_id_str) {
        transaction_id_type transaction_id;
        try {
            while( transaction_id_str.size() < 64 ) transaction_id_str += "0";
            transaction_id = transaction_id_type(transaction_id_str);
        } EOS_RETHROW_EXCEPTIONS(transaction_id_type_exception, "Invalid transaction ID: ${transaction_id}", ("transaction_id", transaction_id_str))
        auto arg= fc::mutable_variant_object( "id", transaction_id);
        if ( block_num_hint > 0 ) {
            arg = arg("block_num_hint", block_num_hint);
        }
        auto transaction_info =call(url, get_transaction_func, fc::variant(arg));
        return transaction_info;
    }

    void client_cactus::push_action(const std::string& url, string contract_account, string action, string data, const vector<string>& tx_permission ){
          fc::variant action_args_var;
          if( !data.empty() ) {
              try {
                  action_args_var = json_from_file_or_string(data, fc::json::relaxed_parser);
              } EOS_RETHROW_EXCEPTIONS(action_type_exception, "Fail to parse action JSON data='${data}'", ("data", data))
          }

          auto arg= fc::mutable_variant_object
                    ("code", contract_account)
                    ("action", action)
                    ("args", action_args_var);
          auto result = call(url, json_to_bin_func, arg);
          // wlog("result:${result}",("result",result));
          auto accountPermissions = get_account_permissions(tx_permission); //vector<string> tx_permission;

          send_actions(url, {eosio::chain::action{accountPermissions, contract_account, action, result.get_object()["binargs"].as<bytes>()}});
    }


    void client_cactus::send_actions(const std::string& url, std::vector<chain::action>&& actions, int32_t extra_kcpu, packed_transaction::compression_type compression) {
          auto result = push_actions( url, move(actions), extra_kcpu, compression);
          std::cout << fc::json::to_pretty_string( result ) << std::endl;
    }

    fc::variant client_cactus::push_actions(const std::string& url, std::vector<chain::action>&& actions, int32_t extra_kcpu, packed_transaction::compression_type compression) {
        signed_transaction trx;
        trx.actions = std::forward<decltype(actions)>(actions);
        return push_transaction(url, trx, extra_kcpu, compression);
    }



    fc::variant client_cactus::push_transaction( const std::string& url, signed_transaction& trx, int32_t extra_kcpu, packed_transaction::compression_type compression) {
        auto info = get_info(url);
        trx.expiration = info.head_block_time + tx_expiration;

        // Set tapos, default to last irreversible block
        trx.set_reference_block(info.last_irreversible_block_id);

        trx.max_cpu_usage_ms = tx_max_net_usage;
        trx.max_net_usage_words = (tx_max_net_usage + 7)/8;

        sign_transaction_local(trx, client_prikey, info.chain_id);
        return call(url, push_txn_func, packed_transaction(trx, compression));
    }

    void client_cactus::sign_transaction_local(signed_transaction& trx, const private_key_type& private_key, const chain_id_type& chain_id) {
         optional<signature_type> sig = private_key.sign(trx.sig_digest(chain_id, trx.context_free_data));
         if (sig) {
            trx.signatures.push_back(*sig);
         }
    }

    fc::variant client_cactus::json_from_file_or_string(const string& file_or_str, fc::json::parse_type ptype = fc::json::legacy_parser)
    {
        std::regex r("^[ \t]*[\{\[]");
       if ( !regex_search(file_or_str, r) && fc::is_regular_file(file_or_str) ) {
          return fc::json::from_file(file_or_str, ptype);
       } else {
          return fc::json::from_string(file_or_str, ptype);
       }
    }

    vector<chain::permission_level> client_cactus::get_account_permissions(const vector<string>& permissions) {
        auto fixedPermissions = permissions | boost::adaptors::transformed([](const string& p) {
            vector<string> pieces;
            split(pieces, p, boost::algorithm::is_any_of("@"));
            if( pieces.size() == 1 ) pieces.push_back( "active" );
                return chain::permission_level{ .actor = pieces[0], .permission = pieces[1] };
        });
        vector<chain::permission_level> accountPermissions;
        boost::range::copy(fixedPermissions, back_inserter(accountPermissions));
        return accountPermissions;
    }

    template<typename T>
    fc::variant client_cactus::call( const std::string& url,
                      const std::string& path,
                      const T& v ) {
        static http_context context = create_http_context();

        auto urlpath = parse_url(url) + path;
        connection_param *cp = new connection_param(context, urlpath, false);

        return do_http_call( *cp, fc::variant(v), false, false );

    }

//
   // template<typename T>
   //  fc::variant client_cactus::call( const std::string& url,
   //                    const std::string& path,
   //                    const T& v ) {
   //      fc::variant re;
   //      static http_context context = create_http_context();
   //      auto urlpath = parse_url(url) + path;
   //      connection_param *cp = new connection_param(context, urlpath, false);
   //      client_write(*cp,  fc::variant(v), [&](fc::variant result){
   //       re = result;
   //      });
   //      return re;
   //  }


   // void client_cactus::client_write(connection_param& cp, fc::variant postdata, std::function<void(fc::variant&)> callback) {
   //    write_queue.push_back({cp, postdata, callback});
   //    if(out_queue.empty())
   //       do_client_write();
   // }


   // void client_cactus::do_client_write() {
   //    if(write_queue.empty() || !out_queue.empty())
   //       return;

   //    while (write_queue.size() > 0) {
   //       auto& m = write_queue.front();
   //       out_queue.push_back(m);
   //       write_queue.pop_front();
   //    }

   //    while (out_queue.size() > 0){
   //       auto& outer = out_queue.front();
   //       auto re = do_http_call( outer.cp, outer.postdata, true, true );
   //       outer.callback(re);
   //       out_queue.pop_front();
   //    }

   // }



}

}

