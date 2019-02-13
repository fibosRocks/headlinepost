#include <eosiolib/currency.hpp>
#include <eosiolib/asset.hpp>

#include <cmath>

using namespace eosio;
using namespace std;

class headline : public eosio::contract
{
  public:
    headline(account_name self) : eosio::contract(self),
                                  _global(_self, _self)
    {
    }

    // @abi action
    void reset()
    {
        require_auth(_self);

        auto gl_itr = _global.begin();
        if (gl_itr == _global.end())
        {
            print("\n\n ========= init ========= \n\n");
            gl_itr = _global.emplace(_self, [&](auto &gl) {
                gl.id = 0;
                gl.winner = _self;
                gl.content = "我要上头条!";
                gl.amount = 0;
                gl.timestamp = now();
            });
        }
        else
        {
            print("\n\n ========= init again ========= \n\n");
            _global.modify(gl_itr, 0, [&](auto &gl) {
                gl.winner = _self;
                gl.content = "我要上头条!";
                gl.amount = 0;
                gl.timestamp = now();
            });
        }
    }

    void transfer(const account_name from, const account_name to, const asset quantity, const string memo)
    {
        if (from == _self || to != _self)
        {
            return;
        }

        eosio_assert(quantity.symbol == S(4, FO), "accept FO only");
        eosio_assert(quantity.is_valid(), "transfer invalid quantity");

        auto gl_itr = _global.begin();
        if (gl_itr == _global.end())
        {
            eosio_assert(0, "need reset");
        }
        else
        {
            auto gap = now() - gl_itr->timestamp;
            double gap_day = (double)gap / 60 / 60 / 24;
            uint64_t current_price = gl_itr->amount * std::pow(0.5, gap_day);
            eosio_assert(current_price < quantity.amount, "need higher price!");
            auto loser = gl_itr->winner;
            _global.modify(gl_itr, 0, [&](auto &gl) {
                gl.winner = from;
                gl.content = memo;
                gl.amount = quantity.amount;
                gl.timestamp = now();
            });
            if (current_price > 0)
            {
                asset payback(current_price, S(4, FO));
                auto memo = std::string("pay back from headlinepost");
                print("payback", payback);
                print("memo", memo);
                action(
                    permission_level{_self, N(active)},
                    N(eosio.token), N(transfer),
                    std::make_tuple(_self, loser, payback, memo))
                    .send();
            }
        }
    }

    void extransfer(const account_name from, const account_name to, const extended_asset fo_quantity, const string memo)
    {
        eosio_assert(fo_quantity.contract == N(eosio), "must be system token!");
        asset quantity(fo_quantity.amount,fo_quantity.symbol);
        transfer(from, to, quantity, memo);
    }

  private:
    // @abi table global i64
    struct global
    {
        uint64_t id;
        account_name winner;
        string content;
        uint64_t amount;
        uint64_t timestamp;

        uint64_t primary_key() const { return id; }
        EOSLIB_SERIALIZE(global, (id)(winner)(content)(amount)(timestamp))
    };

    typedef eosio::multi_index<N(global), global> global_index;
    global_index _global;

    /* typedef struct extended_asset
    {
        asset quantity;
        account_name contract;
        EOSLIB_SERIALIZE(extended_asset, (quantity)(contract))
    }extended_asset; */
};

#define EOSIO_ABI_EX(TYPE, MEMBERS)                                                                                  \
    extern "C"                                                                                                       \
    {                                                                                                                \
        void apply(uint64_t receiver, uint64_t code, uint64_t action)                                                \
        {                                                                                                            \
            if (action == N(onerror))                                                                                \
            {                                                                                                        \
                eosio_assert(code == N(eosio), "onerror action's are only valid from the \"eosio\" system account"); \
            }                                                                                                        \
            auto self = receiver;                                                                                    \
            if (code == self || code == N(eosio.token))                                                              \
            {                                                                                                        \
                TYPE thiscontract(self);                                                                             \
                switch (action)                                                                                      \
                {                                                                                                    \
                    EOSIO_API(TYPE, MEMBERS)                                                                         \
                }                                                                                                    \
            }                                                                                                        \
        }                                                                                                            \
    }

EOSIO_ABI_EX(headline, (transfer)(reset)(extransfer))