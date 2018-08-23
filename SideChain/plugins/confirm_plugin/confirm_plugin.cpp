//
// Created by 盛峰 on 2018/8/1.
//

#include <eosio/confirm_plugin/confirm_plugin.hpp>
#include <eosio/chain/controller.hpp>
#include <eosio/chain_plugin/chain_plugin.hpp>
#include <eosio/history_plugin/history_plugin.hpp>
#include <eosio/chain/action.hpp>
#include <eosio/client_plugin/client_plugin.hpp>

namespace eosio {
    namespace chain {
        struct cactus_transfer {
            account_name from;
            account_name to;
            asset quantity;

            cactus_transfer() = default;

            cactus_transfer(const account_name &from, const account_name &to, const asset &quantity) : from(from),
                                                                                                       to(to),
                                                                                                       quantity(quantity) {}

            static name get_account() {
                return N(cactus);
            }

            static name get_name() {
                return N(transfer);
            }
        };
    }
}
FC_REFLECT( eosio::chain::cactus_transfer, (from)(to)(quantity))

#ifndef DEFINE_INDEX
#define DEFINE_INDEX(object_type, object_name, index_name) \
	struct object_name \
			: public chainbase::object<object_type, object_name> { \
			OBJECT_CTOR(object_name) \
			id_type id; \
			uint32_t block_num; \
			transaction_id_type trx_id; \
			action_data data; \
            block_timestamp_type block_time; \
	}; \
	\
	struct by_block; \
	struct by_trx; \
	using index_name = chainbase::shared_multi_index_container< \
		object_name, \
		indexed_by< \
				ordered_unique<tag<by_id>, member<object_name, object_name::id_type, &object_name::id>>, \
				ordered_unique<tag<by_trx>, member<object_name, transaction_id_type, &object_name::trx_id>>, \
				ordered_non_unique<tag<by_block>, member<object_name, uint32_t, &object_name::block_num>> \
		> \
	>;
#endif

namespace eosio {

    using namespace chain;

//	using boost::signals2::
    using action_data = vector<char>;

    static appbase::abstract_plugin &_confirm_plugin = app().register_plugin<confirm_plugin>();


    DEFINE_INDEX(transaction_summary_object_type, transaction_summary_object, transaction_summary_multi_index)
    DEFINE_INDEX(transaction_executed_object_type, transaction_executed_object, transaction_executed_multi_index)

}

CHAINBASE_SET_INDEX_TYPE(eosio::transaction_summary_object, eosio::transaction_summary_multi_index)
CHAINBASE_SET_INDEX_TYPE(eosio::transaction_executed_object, eosio::transaction_executed_multi_index)

namespace eosio {
    class confirm_plugin_impl {
        public:
            chain_plugin *chain_plug = nullptr;
            optional<boost::signals2::scoped_connection> accepted_transaction_connection;
            optional<boost::signals2::scoped_connection> irreversible_block_connection;


            void catch_from_cactus_transfer(const transaction_metadata_ptr &trx) {
                auto &chain = chain_plug->chain();
                auto &db = chain.db();
                auto block_num = chain.pending_block_state()->block_num;
                auto dpos_irreversible_blocknum = chain.pending_block_state()->dpos_irreversible_blocknum;

                for (const auto action : trx->trx.actions) {
//                    wlog("by shengfeng: ${act}, ${name}", ("act", action.account)("name", action.name));
                    if (action.account == chain::cactus_transfer::get_account() &&
                        action.name == chain::cactus_transfer::get_name()) {
                        const auto transaction_summary = db.create<transaction_summary_object>([&](auto &tso) {
                            tso.block_num = block_num;
                            tso.trx_id = trx->id;
                            tso.data = action.data;

                            //问题一  执行一次合约 进入两次 会存放两条相同的数据 转账一次 拔毛一次  我们的合约暂时不考虑拔毛
                            wlog("捕获一条转账 ${block_num}",("block_num",block_num));
//                            wlog("进入if ${trx->id}",("trx->id",trx->id));
//                            wlog("进入if ${action.data}",("action.data",action.data));
                        });
                        break;
                    }
                }
            }


            void deal_with_cactus_transfer(const block_state_ptr &irb) {
                auto &chain = chain_plug->chain();
                auto &db = chain.db();

                auto block_num = chain.head_block_num();
                auto irreversible_block_num = chain.last_irreversible_block_num();
                auto irb_dpos_irreversible_blocknum = irb->dpos_irreversible_blocknum;
                auto irb_block_num = irb->block_num;
                //问题二：&irb的块高度和不可撤销块的高度  vs  chain中获取的块高度和不可撤销块的高度不一致
//                wlog("deal_with_cactus_transfer block_num ${block_num}  by shengfeng ",("block_num",block_num));
//                wlog("deal_with_cactus_transfer irreversible_block_num ${block_num}  by shengfeng",("block_num",irreversible_block_num));
                wlog("deal_with_cactus_transfer irb_dpos_irreversible_blocknum ${block_num}  by shengfeng",("block_num",irb_dpos_irreversible_blocknum));
                wlog("deal_with_cactus_transfer irb_block_num ${block_num}  by shengfeng",("block_num",irb_block_num));


                const auto &tsmi = db.get_index<transaction_summary_multi_index, by_block>();
                vector<transaction_summary_object> irreversible_transactions;

                auto itr = tsmi.begin();
                while (itr != tsmi.end()) {
                    if (itr->block_num <= irreversible_block_num) {
                        //history
                        history_apis::read_only::get_transaction_params params;
                        params.id = itr->trx_id;
//                        optional<uint32_t>  block_num_hint(3);  //用于模拟失败测试
//                        params.block_num_hint = block_num_hint; //用于模拟失败测试
                        params.block_num_hint = itr->block_num;

                        wlog("参数1 params.id= ${params.id}",("params.id",params.id));
                        wlog("参数2 params.block_num_hint= ${params.id}",("params.id",params.block_num_hint));
                        auto ro_api = app().get_plugin<history_plugin>().get_read_only_api();

                        try {
                            auto result =ro_api.get_transaction(params);
                            wlog("根据2个参数 成功查到数据 ${result}",("result",result));
                            db.remove(*itr);
                        }catch(exception &exce) {

                            std::cout << "itr->trx_id" << itr->trx_id << std::endl;
                            std::cout << "itr->block_num" << itr->block_num << std::endl;
                            wlog("出现异常");
                            auto data = fc::raw::unpack<cactus_transfer>(itr->data);
                            wlog("by sf tx-id: ${num}==${id},data【${from} -> ${to} ${quantity}】",
                                 ("num", itr->block_num)("id", itr->trx_id)("from", data.from)
                                         ("to", data.to)("quantity", data.quantity));
//                            db.create<transaction_executed_object>([&](auto &teo) {
//                                teo.block_num = itr->block_num;
//                                teo.trx_id = itr->trx_id;
//                                teo.data = itr->data;
//                            });
//                            app().find_plugin<client_plugin>()->get_client_apis().push_action("192.168.")
                            db.remove(*itr);
                        }
                    }
                    ++itr;
                }
            }
        };


    static confirm_plugin_impl *my_impl;

    confirm_plugin::confirm_plugin()
            : my(new confirm_plugin_impl) {
        my_impl = my.get();
    }

    confirm_plugin::~confirm_plugin() {}


    void confirm_plugin::set_program_options(boost::program_options::options_description &,
                                             boost::program_options::options_description &cfg) {

    }

    void confirm_plugin::plugin_initialize(const boost::program_options::variables_map &options) {
        try {
            my->chain_plug = app().find_plugin<chain_plugin>();
            auto& chain = my->chain_plug->chain();

            chain.db().add_index<transaction_summary_multi_index>();
            chain.db().add_index<transaction_executed_multi_index>();

            my->accepted_transaction_connection.emplace(
                    chain.accepted_transaction.connect([&](const transaction_metadata_ptr &trx) {
                        my->catch_from_cactus_transfer(trx);
                    }));
            my->irreversible_block_connection.emplace(
                    chain.irreversible_block.connect([&](const block_state_ptr &irb) {
                        my->deal_with_cactus_transfer(irb);
            }));


        } FC_LOG_AND_RETHROW()


    }

    void confirm_plugin::plugin_startup() {
    }

    void confirm_plugin::plugin_shutdown() {
        my->accepted_transaction_connection.reset();
        my->irreversible_block_connection.reset();
    }
}