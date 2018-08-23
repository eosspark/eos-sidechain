/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/time.hpp>

#include <eosiolib/asset.hpp>
#include <eosiolib/contract.hpp>
#include <eosiolib/crypto.h>

#include <string>
#include <vector>
#include <set>
using std::string;
using std::vector;
using std::set;
using namespace eosio;

namespace cactus {
    class msig : public eosio::contract {
    public:
        using contract::contract;


        msig(account_name self)
                : eosio::contract(self), mtranses(_self, _self), wits(_self, _self), sntr(_self, _self), cts(_self, _self), chars(_self, _self),
                  witness_set(), senator_set(), wits_required_confs(), sntr_required_confs(){
            auto idx = chars.find( false );
            if (idx == chars.end()){
                idx = chars.emplace(_self, [&](auto &a){
                   a.pending     = false;
                   a.witness_set = {N(yc),N(sf)};
                   a.senator_set = {N(t),N(u),N(v),N(w),N(x),N(y),N(z)};
                });

            }
            set<account_name> seta(idx->witness_set.begin(), idx->witness_set.end());
            set<account_name> setb(idx->senator_set.begin(), idx->senator_set.end());
            this->witness_set = seta;
            this->senator_set = setb;
            wits_required_confs = (uint32_t) (witness_set.size() * 2 / 3) + 1;
            sntr_required_confs = (uint32_t) (senator_set.size() * 2 / 3) + 1;
        }


        //@abi action
        void transfer(account_name from, account_name to, asset quantity) {
            //print( "Hello, ", name{from} );
            auto quant_after_fee = quantity;

            eosio_assert(is_account(from), "to account does not exist");


            eosio_assert(quantity.is_valid(), "invalid quantity");
            eosio_assert(quantity.amount > 0, "must withdraw positive quantity");
            //printf("%s\n", "开始校验from的权限");

            require_auth(from);
            //printf("%s\n", "开始转from的账");
            action(
                    permission_level{from, N(active)},
                    N(eosio.token), N(transfer),
                    std::make_tuple(from, _self, quantity, std::string("cactus transfer"))
            ).send();

            cts.emplace(_self, [&](auto &a) {
                a.id = cts.available_primary_key();
                a.from = from;
                a.to = to;
                a.amount = quantity.amount;
                a.creation_date = eosio::time_point_sec();
            });

            //printf("%s\n", "执行cactus transfer 结束，请确认到账情况");
        }


        /// @abi action
        void msigtrans(account_name user, checksum256 &trx_id, account_name from, account_name to, asset quantity) {
            eosio_assert(witness_set.count(user) > 0, "user is not witness");
            require_auth(user);
            uint32_t now = (uint32_t)(current_time()  / 1000000);
            auto idx = mtranses.template get_index<N(trx_id)>();
            auto curr_msig = idx.find(mtrans::get_trx_id(trx_id));

            if (curr_msig == idx.end()) {
                mtranses.emplace(_self, [&](auto &a) {
                    a.id = mtranses.available_primary_key();
                    a.trx_id = trx_id;
                    a.from = from;
                    a.to = to;
                    a.quantity = quantity;
                    a.timestamp = now;
                    a.confirmed.push_back(user);
                });
            } else {
                eosio_assert((uint64_t)(now - curr_msig->timestamp) < 60, "letency is too long");
                eosio_assert(curr_msig->from == from && curr_msig->to == to, "transfer account is not correct");
                eosio_assert(curr_msig->quantity == quantity, "quantity is not correct");
                eosio_assert(curr_msig->confirmed.size() < wits_required_confs, "transaction already excused");
                eosio_assert(std::find(curr_msig->confirmed.begin(), curr_msig->confirmed.end(), user)
                             == curr_msig->confirmed.end(), "transaction already confirmed by this account");

                idx.modify(curr_msig, 0, [&](auto &a) {
                    a.timestamp = now;
                    a.confirmed.push_back(user);
                });

                if (curr_msig->confirmed.size() == wits_required_confs) {
                    action(
                            permission_level{_self, N(active)},
                            N(eosio.token), N(transfer),
                            std::make_tuple(_self, to, quantity, std::string("cactus transfer"))
                    ).send();
                }
            }
        }

        /// @abi action
        void newwitness(account_name user, uint32_t version, vector<account_name> witnesses) {
            eosio_assert(senator_set.count(user) > 0, "user is not a senator");
            require_auth(user);

            auto curr_witness = wits.find(version);
            if (curr_witness == wits.end()) {
                wits.emplace(_self, [&](auto &a) {
                    a.version = version;
                    a.witnesses = witnesses;
                    a.confirmed.push_back(user);
                });
            } else {
                eosio_assert(curr_witness->confirmed.size() < sntr_required_confs, "transaction already excused");
                eosio_assert(std::find(curr_witness->confirmed.begin(), curr_witness->confirmed.end(), user)
                             == curr_witness->confirmed.end(), "transaction already confirmed by this account");

                set<account_name> seta(witnesses.begin(), witnesses.end());
                set<account_name> setb(curr_witness->witnesses.begin(), curr_witness->witnesses.end());
                eosio_assert(seta == setb, "different witness propose");

                wits.modify(curr_witness, 0, [&](auto &a) {
                    a.confirmed.push_back(user);
                });

                if (curr_witness->confirmed.size() == sntr_required_confs) {
                    auto idx = chars.find( false );
                    eosio_assert(idx!=chars.end(), "error init");
                    chars.modify(idx, 0, [&](auto &a){
                        a.witness_set = witnesses;
                    });
                }
            }

        }

        /// @abi action
        void newsenator(account_name user, uint32_t version, vector<account_name> senators) {
            eosio_assert(witness_set.count(user) > 0, "user is not a witness");
            require_auth(user);

            auto curr_senator = sntr.find(version);
            if (curr_senator == sntr.end()) {
                sntr.emplace(_self, [&](auto &a) {
                    a.version = version;
                    a.senators = senators;
                    a.confirmed.push_back(user);
                });
            } else {
                eosio_assert(curr_senator->confirmed.size() < wits_required_confs, "transaction already excused");
                eosio_assert(std::find(curr_senator->confirmed.begin(), curr_senator->confirmed.end(), user)
                             == curr_senator->confirmed.end(), "transaction already confirmed by this account");

                set<account_name> seta(senators.begin(), senators.end());
                set<account_name> setb(curr_senator->senators.begin(), curr_senator->senators.end());
                eosio_assert(seta == setb, "different senator propose");

                sntr.modify(curr_senator, 0, [&](auto &a) {
                    a.confirmed.push_back(user);
                });

                if (curr_senator->confirmed.size() == wits_required_confs) {
                    auto idx = chars.find( false );
                    eosio_assert(idx!=chars.end(), "error init");
                    chars.modify(idx, 0, [&](auto &a){
                        a.senator_set = senators;
                    });
                }
            }
        }


        /// @abi action
        void upwitness(account_name user) {
            eosio_assert(is_account(user), "user account does not exist");
            require_auth(user);
            int count = wits.available_primary_key();

            //printf("%d\n", count);
            if (count%2==0) {
                wits.emplace(_self, [&](auto &a) {
                    a.version = count;
                    a.witnesses = witnesses1;
                });
            } else {
                wits.emplace(_self, [&](auto &a) {
                    a.version = count;
                    a.witnesses = witnesses2;
                });
            }

        }

    private:

        //@abi table mtrans i64
        struct mtrans {
            uint64_t id;
            checksum256 trx_id;
            account_name from;
            account_name to;
            asset quantity;
            uint32_t timestamp;
            vector<account_name> confirmed;

            uint64_t primary_key() const { return id; }

            key256 by_trx_id() const { return get_trx_id(trx_id); }

            static key256 get_trx_id(const checksum256 &trx_id) {
                const uint64_t *p64 = reinterpret_cast<const uint64_t *>(&trx_id);
                return key256::make_from_word_sequence<uint64_t>(p64[0], p64[1], p64[2], p64[3]);
            }

            EOSLIB_SERIALIZE(mtrans, (id)(trx_id)(from)(to)(quantity)(timestamp)(confirmed))
        };

    private:
        //@abi table cactushi i64
        struct cactushi {

            uint64_t id;
            account_name from;
            account_name to;
            int64_t amount;
            eosio::time_point_sec creation_date;

            uint64_t primary_key() const { return id; }

            EOSLIB_SERIALIZE(cactushi, (id)(from)(to)(amount)(creation_date))
        };

        typedef eosio::multi_index<N(cactushi), cactushi> cts_his_index;
        cts_his_index cts;


        typedef eosio::multi_index<N(mtrans), mtrans,
                indexed_by<N(trx_id), const_mem_fun<mtrans, key256, &mtrans::by_trx_id> >
        > mtran_index;

        //@abi table witness i64
        struct witness {
            uint64_t version;
            vector<account_name> witnesses;
            vector<account_name> confirmed;

            uint64_t primary_key() const { return version; }

            EOSLIB_SERIALIZE(witness, (version)(witnesses)(confirmed))
        };

        typedef eosio::multi_index<N(witness), witness> witness_index;

        //@abi table senator i64
        struct senator {
            uint64_t version;
            vector<account_name> senators;
            vector<account_name> confirmed;

            uint64_t primary_key() const { return version; }

            EOSLIB_SERIALIZE(senator, (version)(senators)(confirmed))
        };

        typedef eosio::multi_index<N(senator), senator> senator_index;

        //@abi table character
        struct character {
            uint64_t pending;
            vector<account_name> witness_set;
            vector<account_name> senator_set;

            uint64_t primary_key() const { return pending; }

            EOSLIB_SERIALIZE(character, (pending)(witness_set)(senator_set))
        };

        typedef eosio::multi_index<N(character), character> character_index;

        mtran_index mtranses;

        witness_index wits;
        senator_index sntr;

        set<account_name> witness_set;
        set<account_name> senator_set;

        uint32_t wits_required_confs;
        uint32_t sntr_required_confs;


        character_index chars;

        vector<account_name> witnesses1 = {N(a),N(b),N(c),N(d),N(e),N(f),N(g)};
        vector<account_name> witnesses2 = {N(t),N(u),N(v),N(w),N(x),N(y),N(z)};



    };

    EOSIO_ABI(msig, (transfer)(msigtrans)(newwitness)(newsenator)(upwitness))
}
