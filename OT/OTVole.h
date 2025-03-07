
#ifndef _OTVOLE
#define _OTVOLE

#include "Math/Z2k.h"
#include "OTExtension.h"
#include "Row.h"
#include "config.h"

using namespace std;

template <class T>
class OTVoleBase : public OTExtension
{
public:
	const int S;

	OTVoleBase(int S, int nbaseOTs, int baseLength,
	                int nloops, int nsubloops,
	                TwoPartyPlayer* player,
	                const BitVector& baseReceiverInput,
	                const vector< vector<BitVector> >& baseSenderInput,
	                const vector<BitVector>& baseReceiverOutput,
	                OT_ROLE role=BOTH,
	                bool passive=false)
	    : OTExtension(nbaseOTs, baseLength, nloops, nsubloops, player, baseReceiverInput,
	            baseSenderInput, baseReceiverOutput, INV_ROLE(role), passive),
	        S(S),
	        corr_prime(),
	        t0(S),
	        t1(S),
	        u(S),
	        t(S),
	        a(S) {
	            // need to flip roles for OT extension init, reset to original role here
	            this->ot_role = role;
	            local_prng.ReSeed();
	        }

	    void evaluate(vector<T>& output, const vector<T>& newReceiverInput);

	    void evaluate(vector<T>& output, int nValues, const BitVector& newReceiverInput);

	    virtual int n_challenges() { return S; }
	    virtual int get_challenge(PRNG&, int i) { return i; }

	protected:

		// Sender fields
		Row<T> corr_prime;
		vector<Row<T>> t0, t1;
		// Receiver fields
		vector<Row<T>> u, t, a;
		// Both
		PRNG local_prng;

		Row<T> tmp;

	    virtual void consistency_check (vector<octetStream>& os);

	    void set_coeffs(__m128i* coefficients, PRNG& G, int num_elements) const;

	    void hash_row(octetStream& os, const Row<T>& row, const __m128i* coefficients);

	    void hash_row(octet* hash, const Row<T>& row, const __m128i* coefficients);

};

template <class T>
class OTVole : public OTVoleBase<T>
{

public:
    OTVole(int S, int nbaseOTs, int baseLength,
                int nloops, int nsubloops,
                TwoPartyPlayer* player,
                const BitVector& baseReceiverInput,
                const vector< vector<BitVector> >& baseSenderInput,
                const vector<BitVector>& baseReceiverOutput,
                OT_ROLE role=BOTH,
                bool passive=false)
    : OTVoleBase<T>(S, nbaseOTs, baseLength, nloops, nsubloops, player, baseReceiverInput,
            baseSenderInput, baseReceiverOutput, INV_ROLE(role), passive) {
    }

    int n_challenges() { return NUM_VOLE_CHALLENGES; }
    int get_challenge(PRNG& G, int) { return G.get_uint(this->S); }
};

#endif
