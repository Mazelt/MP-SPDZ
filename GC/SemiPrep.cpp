/*
 * SemiPrep.cpp
 *
 */

#include "SemiPrep.h"
#include "ThreadMaster.h"
#include "OT/NPartyTripleGenerator.h"
#include "OT/BitDiagonal.h"

#include "Protocols/ReplicatedPrep.hpp"
#include "Protocols/MAC_Check_Base.hpp"
#include "OT/NPartyTripleGenerator.hpp"

namespace GC
{

SemiPrep::SemiPrep(DataPositions& usage, ShareThread<SemiSecret>&) :
        SemiPrep(usage)
{
}

SemiPrep::SemiPrep(DataPositions& usage) :
        BufferPrep<SemiSecret>(usage), triple_generator(0)
{
}

void SemiPrep::set_protocol(Beaver<SemiSecret>& protocol)
{
    (void) protocol;
    params.set_passive();
    triple_generator = new SemiSecret::TripleGenerator(
            BaseMachine::s().fresh_ot_setup(),
            protocol.P.N, -1, OnlineOptions::singleton.batch_size,
            1, params, {}, &protocol.P);
    triple_generator->multi_threaded = false;
}

void SemiPrep::buffer_triples()
{
    assert(this->triple_generator);
    this->triple_generator->generatePlainTriples();
    for (auto& x : this->triple_generator->plainTriples)
    {
        this->triples.push_back({{x[0], x[1], x[2]}});
    }
    this->triple_generator->unlock();
}

SemiPrep::~SemiPrep()
{
    if (triple_generator)
        delete triple_generator;
}

void SemiPrep::buffer_bits()
{
    auto& thread = Thread<SemiSecret>::s();
    word r = thread.secure_prng.get_word();
    for (size_t i = 0; i < sizeof(word) * 8; i++)
        this->bits.push_back((r >> i) & 1);
}

} /* namespace GC */
