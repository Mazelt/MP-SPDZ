/*
 * TinierShare.h
 *
 */

#ifndef GC_TINIERSHARE_H_
#define GC_TINIERSHARE_H_

#include "Processor/DummyProtocol.h"
#include "Protocols/Share.h"
#include "Math/Bit.h"
#include "TinierSharePrep.h"

namespace GC
{

template<class T> class TinierSecret;

template<class T>
class TinierShare: public Share_<SemiShare<Bit>, SemiShare<T>>,
        public ShareSecret<TinierSecret<T>>
{
    typedef TinierShare This;

public:
    typedef Share_<SemiShare<Bit>, SemiShare<T>> super;

    typedef T mac_key_type;
    typedef T mac_type;
    typedef T sacri_type;
    typedef Share<T> input_check_type;

    typedef MAC_Check_<This> MAC_Check;
    typedef TinierSharePrep<This> LivePrep;
    typedef ::Input<This> Input;
    typedef Beaver<This> Protocol;
    typedef NPartyTripleGenerator<TinierSecret<T>> TripleGenerator;

    typedef void DynamicMemory;
    typedef SwitchableOutput out_type;

    static string name()
    {
        return "tinier share";
    }

    static string type_string()
    {
        return "Tinier";
    }

    static ShareThread<TinierSecret<T>>& get_party()
    {
        return ShareThread<TinierSecret<T>>::s();
    }

    static MAC_Check* new_mc(mac_key_type mac_key)
    {
        return new MAC_Check(mac_key);
    }

    static This new_reg()
    {
        return {};
    }

    TinierShare()
    {
    }
    TinierShare(const super& other) :
            super(other)
    {
    }
    TinierShare(const typename super::share_type& share, const typename super::mac_type& mac) :
            super(share, mac)
    {
    }

    void XOR(const This& a, const This& b)
    {
        *this = a + b;
    }

    This& operator^=(const This& other)
    {
        *this += other;
        return *this;
    }

    void public_input(bool input)
    {
        auto& party = get_party();
        *this = super::constant(input, party.P->my_num(),
                party.MC->get_alphai());
    }

    void random()
    {
        TinierSecret<T> tmp;
        get_party().DataF.get_one(DATA_BIT, tmp);
        *this = tmp.get_reg(0);
    }
};

} /* namespace GC */

#endif /* GC_TINIERSHARE_H_ */
