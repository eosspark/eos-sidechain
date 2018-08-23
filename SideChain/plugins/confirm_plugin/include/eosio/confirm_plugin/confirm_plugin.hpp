//
// Created by 盛峰 on 2018/8/1.
//
#pragma once
#include <appbase/application.hpp>
#include <eosio/chain_plugin/chain_plugin.hpp>


namespace eosio {

    using namespace appbase;


    class confirm_plugin : public appbase::plugin<confirm_plugin>
    {
        public:
            confirm_plugin();
            virtual ~confirm_plugin();

            APPBASE_PLUGIN_REQUIRES((chain_plugin))
            virtual void set_program_options(options_description&, options_description& cfg) override;

            void plugin_initialize(const variables_map& options);
            void plugin_startup();
            void plugin_shutdown();

        private:
            std::unique_ptr<class confirm_plugin_impl> my;

    };


}
