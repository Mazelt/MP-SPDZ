"""
Module for math operations.

Implements trigonometric and logarithmic functions.

This has to imported explicitely.
"""


import math
from Compiler import floatingpoint
from Compiler import types
from Compiler import comparison
from Compiler import program
# polynomials as enumerated on Hart's book
##
# @private
p_3307 = [1.57079632679489000000000, -0.64596409750624600000000,
          0.07969262624616700000000, -0.00468175413531868000000,
          0.00016044118478735800000, -0.00000359884323520707000,
          0.00000005692172920657320, -0.00000000066880348849204,
          0.00000000000606691056085, -0.00000000000004375295071,
          0.00000000000000025002854]
##
# @private
p_3508 = [1.00000000000000000000, -0.50000000000000000000,
          0.04166666666666667129, -0.00138888888888888873,
          0.00002480158730158702, -0.00000027557319223933,
          0.00000000208767569817, -0.00000000001147074513,
          0.00000000000004779454, -0.00000000000000015612,
          0.00000000000000000040]
##
# @private
p_1045 = [1.000000077443021686, 0.693147180426163827795756,
          0.224022651071017064605384, 0.055504068620466379157744,
          0.009618341225880462374977, 0.001332730359281437819329,
          0.000155107460590052573978, 0.000014197847399765606711,
          0.000001863347724137967076]
##
# @private
p_2524 = [-2.05466671951, -8.8626599391,
          +6.10585199015, +4.81147460989]
##
# @private
q_2524 = [+0.353553425277, +4.54517087629,
          +6.42784209029, +1]
##
# @private
p_5102 = [+21514.05962602441933193254468, +73597.43380288444240814980706,
          +100272.5618306302784970511863, +69439.29750032252337059765503,
          +25858.09739719099025716567793, +5038.63918550126655793779119,
          +460.1588804635351471161727227, +15.08767735870030987717455528,
          +0.07523052818757628444510729539]
##
# @private
q_5102 = [+21514.05962602441933193298234, +80768.78701155924885176713209,
          +122892.6789092784776298743322, +97323.20349053555680260434387,
          +42868.57652046408093184006664, +10401.13491566890057005103878,
          +1289.75056911611097141145955, +68.51937831018968013114024294,
          +1]
##
# @private
p_4737 = [-9338.550897341021522505385079, +43722.68009378241623148489754,
          -86008.12066370804865047446067, +92190.57592175496843898184959,
          -58360.27724533928122075635101, +22081.61324178027161353562222,
          -4805.541226761699661564427739, +542.2148323255220943742314911,
          -24.94928894422502466205102672, 0.2222361619461131578797029272]
##
# @private
q_4737 =[-9338.550897341021522505384935, +45279.10524333925315190231067,
         -92854.24688696401422824346529, +104687.2504366298224257408682,
         -70581.74909396877350961227976, +28972.22947326672977624954443,
         -7044.002024719172700685571406, +935.7104153502806086331621628,
         -56.83369358538071475796209327, 1]
##
# @private
p_4754 = [-6.90859801, +12.85564644, -5.94939208]

##
# @private
q_4754 = [-6.92529156, +14.20305096, -8.27925501, 1]

# all inputs are calcualted in radians hence we need some conversion.
pi = math.radians(180)
pi_over_2 = math.radians(90)

##
# truncates values regardless of the input type. (It always rounds down)
# @param x: coefficient to be truncated.
#
# @return truncated sint value of x
def trunc(x):
    if type(x) is types.sfix:
        return floatingpoint.Trunc(x.v, x.k, x.f, x.kappa, signed=True)
    elif type(x) is types.sfloat:
        v, p, z, s = floatingpoint.FLRound(x, 0)
        #return types.sfloat(v, p, z, s, x.err) 
        return types.sfloat(v, p, z, s) 
    return x


##
# loads integer to fractional type (sint)
# @param x: coefficient to be truncated.
#
# @return returns sfix, sfloat loaded value
def load_sint(x, l_type):
    if l_type is types.sfix:
        return types.sfix.from_sint(x)
    elif l_type is types.sfloat:
        return x
    return x


##
# evaluates a Polynomial to a given x in a privacy preserving manner.
# Inputs can be of any kind of register, secret or otherwise.
#
# @param p_c: Polynomial coefficients. (Array)
#
# @param x: Value to which the polynomial p_c is evaluated to.(register)
#
# @return the evaluation of the polynomial. return type depends on inputs.
def p_eval(p_c, x):
    degree = len(p_c) - 1
    if type(x) is types.sfix:
        # ignore coefficients smaller than precision
        for c in reversed(p_c):
            if c < 2 ** -(x.f + 1):
                degree -= 1
            else:
                break
    pre_mults = floatingpoint.PreOpL(lambda a,b,_: a * b,
                                           [x] * degree)
    local_aggregation = 0
    # Evaluation of the Polynomial
    for i, pre_mult in zip(p_c[1:], pre_mults):
        local_aggregation += pre_mult.mul_no_reduce(x.coerce(i))
    return local_aggregation.reduce_after_mul() + p_c[0]


##
# reduces the input to [0,90) and returns whether the  reduced value is
# greater than \Pi and greater than Pi over 2
# @param x: value of any type to be reduced to the [0,90) interval
#
# @return w: reduced angle in either fixed or floating point .
#
# @return b1: \{0,1\} value. Returns one when reduction to 2*\pi
# is greater than \pi
#
# @return b2: \{0,1\} value. Returns one when reduction to
# \pi is greater than \pi/2.
def sTrigSub(x):
    # reduction to 2* \pi
    f = x * (1.0 / (2 * pi))
    f = load_sint(trunc(f), type(x))
    y = x - (f) * (2 * pi)
    # reduction to \pi
    b1 = y > pi
    w = b1 * ((2 * pi - y) - y) + y
    # reduction  to \pi/2
    b2 = w > pi_over_2
    w = b2 * ((pi - w) - w) + w
    # returns scaled angle and boolean flags
    return w, b1, b2

# kernel method calls -- they are built in a generic way


##
# Kernel sin. Returns  the sin of a given angle on the [0, \pi/2) interval and
# adjust the sign in case the angle was reduced on the [0,360) interval
#
# @param w: fractional value for an angle on the [0, \pi) interval.
#
# @return  returns the sin of w.
def ssin(w, s):
    # calculates the v of w for polynomial evaluation
    v = w * (1.0 / pi_over_2)
    v_2 = v ** 2
    # adjust sign according to the movement in the reduction
    b = s * (-2) + 1
    # calculate the sin using polynomial evaluation
    local_sin = b * v * p_eval(p_3307, v_2)
    return local_sin


##
# Kernel cos. Returns  the cos of a given angle on the [0.pi/2)
# interval and adjust
# the sign in case the angle was reduced on the [0,360) interval.
#
# @param w: fractional value for an angle on the [0,\pi) interval.
#
# @param s: \{0,1\} value. Corresponding to b2. Returns 1 if the angle
# was reduced from an angle in the [\pi/2,\pi) interval.
#
# @return  returns the cos of w (sfix).
def scos(w, s):
    # calculates the v of the w.
    v = w
    v_2 = v ** 2
    # adjust sign according to the movement in the reduction
    b = s * (-2) + 1
    # calculate the cos using polynomial evaluation
    local_cos = b * p_eval(p_3508, v_2)
    return local_cos


# facade method calls --it is built in a generic way

def sin(x):
    """
    Returns the sine of any given fractional value.

    :param x: fractional input (sfix, sfloat)

    :return: sin of :py:obj:`x` (sfix, sfloat)
    """
    # reduces the angle to the [0,\pi/2) interval.
    w, b1, b2 = sTrigSub(x)
    # returns the sin with sign correction
    return ssin(w, b1)


def cos(x):
    """
    Returns the cosine of any given fractional value.

    :param x: fractional input (sfix, sfloat)

    :return: cos of :py:obj:`x` (sfix, sfloat)
    """
    # reduces the angle to the [0,\pi/2) interval.
    w, b1, b2 = sTrigSub(x)

    # returns the sin with sign correction
    return scos(w, b2)


def tan(x):
    """
    Returns the tangent of any given fractional value.

    :param x: fractional input (sfix, sfloat)

    :return: tan of :py:obj:`x` (sfix, sfloat)
    """
    # reduces the angle to the [0,\pi/2) interval.
    w, b1, b2 = sTrigSub(x)
    # calculates the sin and the cos.
    local_sin = ssin(w, b1)
    local_cos = scos(w, b2)
    # obtains the local tan
    local_tan = local_sin/local_cos
    return local_tan


@types.vectorize
def exp2_fx(a):
    """
    Power of two for fixed-point numbers.

    :param a: exponent for :math:`2^a` (sfix)

    :return: :math:`2^a` if it is within the range. Undefined otherwise
    """
    if types.program.options.ring:
        sint = types.sint
        intbitint = types.intbitint
        # how many bits to use from integer part
        n_int_bits = int(math.ceil(math.log(a.k - a.f, 2)))
        n_bits = a.f + n_int_bits
        n_shift = int(types.program.options.ring) - a.k
        r_bits = [sint.get_random_bit() for i in range(a.k)]
        shifted = ((a.v - sint.bit_compose(r_bits)) << n_shift).reveal()
        masked_bits = (shifted >> n_shift).bit_decompose(a.k)
        lower_overflow = sint()
        comparison.CarryOut(lower_overflow, masked_bits[a.f-1::-1],
                            r_bits[a.f-1::-1])
        lower_r = sint.bit_compose(r_bits[:a.f])
        lower_masked = sint.bit_compose(masked_bits[:a.f])
        lower = lower_r + lower_masked - (lower_overflow << (a.f))
        c = types.sfix._new(lower, k=a.k, f=a.f)
        higher_bits = intbitint.bit_adder(masked_bits[a.f:n_bits],
                                          r_bits[a.f:n_bits],
                                          carry_in=lower_overflow,
                                          get_carry=True)
        d = types.sfix.from_sint(floatingpoint.Pow2_from_bits(higher_bits[:-1]),
                                 k=a.k, f=a.f)
        e = p_eval(p_1045, c)
        g = d * e
        small_result = types.sfix._new(g.v.round(a.k + 1, a.f, signed=False,
                                            nearest=types.sfix.round_nearest),
                                       k=a.k, f=a.f)
        carry = comparison.CarryOutLE(masked_bits[n_bits:-1],
                                      r_bits[n_bits:-1],
                                      higher_bits[-1])
        # should be for free
        highest_bits = intbitint.ripple_carry_adder(
            masked_bits[n_bits:-1], [0] * (a.k - n_bits),
            carry_in=higher_bits[-1])
        bits_to_check = [x.bit_xor(y)
                         for x, y in zip(highest_bits[:-1], r_bits[n_bits:-1])]
        t = floatingpoint.KMul(bits_to_check)
        # sign
        s = masked_bits[-1].bit_xor(r_bits[-1]).bit_xor(carry)
        return s.if_else(t.if_else(small_result, 0), g)
    else:
        # obtain absolute value of a
        s = a < 0
        a = (s * (-2) + 1) * a
        # isolates fractional part of number
        b = trunc(a)
        c = a - load_sint(b, type(a))
        # squares integer part of a
        d = load_sint(b.pow2(types.sfix.k - types.sfix.f), type(a))
        # evaluates fractional part of a in p_1045
        e = p_eval(p_1045, c)
        g = d * e
        return (1 - s) * g + s * ((types.sfix(1)) / g)


@types.vectorize
def log2_fx(x):
    """
    Returns the result of :math:`\log_2(x)` for any unbounded
    number. This is achieved by changing :py:obj:`x` into
    :math:`f \cdot 2^n` where f is bounded by :math:`[0.5, 1]`.  Then the
    polynomials are used to calculate :math:`\log_2(f)`, which is then
    just added to :math:`n`.

    :param x: input for :math:`\log_2` (sfix, sint).

    :return: (sfix) the value of :math:`\log_2(x)`

    """
    if type(x) is types.sfix:
        # transforms sfix to f*2^n, where f is [o.5,1] bounded
        # obtain number bounded by [0,5 and 1] by transforming input to sfloat
        v, p, z, s = floatingpoint.Int2FL(x.v, x.k, x.f, x.kappa)
        p -= x.f
        vlen = x.f
    else:
        d = types.sfloat(x)
        v, p, vlen = d.v, d.p, d.vlen
    # isolates mantisa of d, now the n can be also substituted by the
    # secret shared p from d in the expresion above.
    v = load_sint(v, type(x))
    w = (1.0 / (2 ** (vlen)))
    v = v * w
    # polynomials for the  log_2 evaluation of f are calculated
    P = p_eval(p_2524, v)
    Q = p_eval(q_2524, v)
    # the log is returned by adding the result of the division plus p.
    a = P / Q + load_sint(vlen + p, type(x))
    return a  # *(1-(f.z))*(1-f.s)*(1-f.error)


def pow_fx(x, y):
    """
    Returns the value of the expression :math:`x^y` where both inputs
    are secret shared. It uses  :py:func:`log2_fx` together with
    :py:func:`exp2_fx` to calculate the expression :math:`2^{y \log_2(x)}`.

    :param x: (sfix) secret shared base.

    :param y: (sfix, clear types) secret shared exponent.

    :return: :math:`x^y` (sfix)
    """
    log2_x =0
    # obtains log2(x)
    if (type(x) == int or type(x) == float):
        log2_x = math.log(x,2)
    else:
        log2_x = log2_fx(x)
    # obtains y * log2(x)
    exp = y * log2_x
    # returns 2^(y*log2(x))
    return exp2_fx(exp)


def log_fx(x, b):
    """
    Returns the value of the expression :math:`\log_b(x)` where
    :py:obj:`x` is secret shared. It uses :py:func:`log2_fx` to
    calculate the expression :math:`\log_b(2) \cdot \log_2(x)`.

    :param x: (sfix, sint) secret shared coefficient for log.

    :param b: (float) base for log operation.

    :return: (sfix) the value of :math:`log_b(x)`.

    """
    # calculates logb(2)
    logb_2 = math.log(2, b)
    # returns  logb(2) * log2(x)
    return logb_2 * log2_fx(x)


##
# Returns the absolute value of a fix point number.
# The method is also applicable to sfloat,
# however, more efficient mechanisms can be devised.
#
# @param x: (sfix)
#
# @return (sfix) unsigned
def abs_fx(x):
    s = x < 0
    return (1 - 2 * s) * x


##
# Floors the input and stores the value into a sflix register
# @param x: coefficient to be floored.
#
# @return floored sint value of x
def floor_fx(x):
    return load_sint(floatingpoint.Trunc(x.v, x.k - x.f, x.f, x.kappa), type(x))


### sqrt methods


##
# obtains the most significative bit (MSB) 
# of a given input. The size of the vector
# is tuned to the needs of sqrt. 
# @param b: number from which you obtain the
# most significative bit.
# @param k:  number of bits for which
# an output of size (k+1) if even
#  is going to be produced.
# @return z: index array for MSB of size
# k or K+1 if even.
def MSB(b, k):
    # calculation of z
    # x in order 0 - k
    if (k > types.program.bit_length):
        raise OverflowError("The supported bit \
        lenght of the application is smaller than k")

    x_order = b.bit_decompose(k)
    x = [0] * k
    # x i now inverted
    for i in range(k - 1, -1, -1):
        x[k - 1 - i] = x_order[i]
    # y is inverted for PReOR and then restored
    y_order = floatingpoint.PreOR(x)

    # y in order (restored in orginal order
    y = [0] * k
    for i in range(k - 1, -1, -1):
        y[k - 1 - i] = y_order[i]

    # obtain z
    z = [0] * (k + 1 - k % 2)
    for i in range(k - 1):
        z[i] = y[i] - y[i + 1]
    z[k - 1] = y[k - 1]

    return z


##
# Similar to norm_SQ, saves rounds by not 
# calculating v and c. 
#
# @param b: sint input to be normalized. 
# @param k: bitsize of the input, by definition
# its value is either sfix.k or program.bit_lengthh 
# @return m_odd: the parity of  most signficative bit index m
# @return m: index of most significative bit
# @return w: 2^m/2 or 2^ (m-1) /2
def norm_simplified_SQ(b, k):
    z = MSB(b, k)
    # construct m
    #m = types.sint(0)
    m_odd = 0
    for i in range(k):
        #m = m + (i + 1) * z[i]
        # determine the parity of the input
        if (i % 2 == 0):
            m_odd = m_odd + z[i]

    # construct w,
    k_over_2 = k // 2 + 1
    w_array = [0] * (k_over_2)
    w_array[0] = z[0]
    for i in range(1, k_over_2):
        w_array[i] = z[2 * i - 1] + z[2 * i]

    # w aggregation
    w = types.sint(0)
    for i in range(k_over_2):
        w += (2 ** i) * w_array[i]

    # return computed values
    #return m_odd, m, w
    return m_odd, None, w


##
# Obtains the sqrt using  our custom mechanism
# for any sfix input value. 
# no restrictions on the size of f. 
#
#  @param x: secret shared input from which the sqrt
# is calucalted,
#
# @return g: approximated sqrt
def sqrt_simplified_fx(x):
    # fix theta (number of iterations)
    theta = max(int(math.ceil(math.log(types.sfix.k))), 6)

    # process to use 2^(m/2) approximation
    m_odd, m, w = norm_simplified_SQ(x.v, x.k)
    # process to set up the precision and allocate correct 2**f
    if x.f % 2 == 1:
        m_odd =  (1 - 2 * m_odd) + m_odd
        w = (w * 2 - w) * (1-m_odd) + w
    # map number to use sfix format and instantiate the number
    w = types.sfix(w * 2 ** ((x.f - (x.f % 2)) // 2))
    # obtains correct 2 ** (m/2)
    w = (w * (types.cfix(2 ** (1/2.0))) - w) * m_odd + w
    # produce x/ 2^(m/2)
    y_0 = types.cfix(1.0) / w

    # from this point on it sufices to work sfix-wise
    g_0 = (y_0 * x)
    h_0 = y_0 * types.cfix(0.5)
    gh_0 = g_0 * h_0

    ## initialization
    g = g_0
    h = h_0
    gh = gh_0

    for i in range(1, theta - 2):
        r = (3 / 2.0) - gh
        g = g * r
        h = h * r
        gh = g * h

    # newton
    r = (3 / 2.0) - gh
    h = h * r
    H = 4 * (h * h)
    H = H * x
    H = (3) - H
    H = h * H
    g = H * x
    g = g

    return g


##
# Calculates the normSQ of a number
# @param x: number from which the norm is going to be extracted
# @param k: bitsize of x
#
# @return c: where c = x*v where c is bounded by 2^{k-1} and 2^k
# @return v: where v = 2^k-m
# @return m: where m = MSB
# @return w: where w = 2^{m/2} if m is oeven and 2^{m-1 / 2} otherwise
def norm_SQ(b, k): 
    # calculation of z
    # x in order 0 - k
    z = MSB(b,k)
    # now reverse bits of z[i] to generate v
    v = types.sint(0)
    for i in range(k):
        v += (2**(k - i - 1)) * z[i]
    c = b * v
  
    # construct m
    m = types.sint(0)
    for i in range(k):
        m = m + (i+1) * z[i]

    # construct w, changes from what is on the paper
    # and the documentation
    k_over_2= k/2+1#int(math.ceil((k/2.0)))+1
    w_array = [0]*(k_over_2 )
    w_array[0] = z[0]
    for i in range(1, k_over_2):
        w_array[i] = z[2 * i - 1] + z[2 * i]

    w = types.sint(0)
    for i in range(k_over_2):
        w += (2 ** i) * w_array[i]

    # return computed values
    return c, v, m, w


##
# Given f and k, returns a linear approximation of 1/x^{1/2}
# escalated by s^f.
# Method only works for sfix inputs. It uses the normSQ.
# the method is an implementation of [Liedel2012]
# @param x: number from which the approximation is caluclated
# @param k: bitsize of x
# @param f: precision of the input f
#
# @return c: Some approximation of  (1/x^{1/2} * 2^f) *K
# where K is close to 1
def lin_app_SQ(b, k, f):

    alpha = types.cfix((-0.8099868542) * 2 ** (k))
    beta = types.cfix(1.787727479 * 2 ** (2 * k))

    # obtain normSQ parameters
    c, v, m, W = norm_SQ(types.sint(b), k)

    # c is now escalated
    w = alpha * load_sint(c,types.sfix) + beta  # equation before b and reduction by order of k


    # m even or odd determination
    m_bit = types.sint()
    comparison.Mod2(m_bit, m, int(math.ceil(math.log(k, 2))), w.kappa, False)
    m = load_sint(m_bit, types.sfix)

    # w times v  this way both terms have 2^3k and can be symplified
    w = w * v
    factor = 1.0 / (2 ** (3.0 * k - 2 * f))
    w = w * factor  # w escalated to 3k -2 * f
    # normalization factor W* 1/2 ^{f/2}
    w = w * W * types.cfix(1.0 / (2 ** (f / 2.0)))
    # now we need to  elminate an additional root of 2 in case m was odd
    sqr_2 = types.cfix((2 ** (1 / 2.0)))
    w = (1 - m) * w + sqr_2 * w * m

    return w


##
# Given bitsize k and precision f, it calulates the square root of x.
# @param x: number from which the norm is going to be extracted
# @param k: bitsize of x.
# @param f: precision of x.
#
# @return g: square root of de-scaled input x
def sqrt_fx(x_l, k, f):
    factor = 1.0 / (2.0 ** f)

    x = load_sint(x_l, types.sfix) * factor

    theta = int(math.ceil(math.log(k/5.4)))

    y_0 = lin_app_SQ(x_l,k,f) #cfix(1.0/ (cx ** (1/2.0))) # lin_app_SQ(x_l,5,2)

    y_0 = y_0 * factor #*((1.0/(2.0 ** f)))
    g_0 = y_0 * x


    #g = mpc_math.load_sint(mpc_math.trunc(g_0),types.sfix)
    h_0 = y_0 *(0.5)
    gh_0 = g_0 * h_0

    ##initialization
    g= g_0
    h= h_0
    gh =gh_0

    for i in range(1,theta-2): #to implement \in [1,\theta-2]
        r = (3/2.0) - gh
        g = g * r
        h = h * r
        gh = g * h

    # newton
    r =  (3/2.0) - gh
    h = h * r
    H = 4 * (h * h)
    H = H * x
    H = (3) - H
    H = h * H
    g = H * x
    g = g #* (0.5)

    return g


@types.vectorize
def sqrt(x, k = types.sfix.k, f = types.sfix.f):
    """
    Returns the square root (sfix) of any given fractional
    value as long as it can be rounded to a integral value
    with :py:obj:`f` bits of decimal precision.

    :param x: fractional input (sfix).

    :return:  square root of :py:obj:`x` (sfix).
    """
    if (3 *k -2 * f >= types.sfix.f):
        return sqrt_simplified_fx(x)
        # raise OverflowError("bound for precision violated: 3 * k - 2 * f <  x.f ")
    else:
        param = trunc(x *(2 ** (f)))
        return sqrt_fx(param ,k ,f)


def atan(x):
    """
    Returns the arctangent (sfix) of any given fractional value.

    :param x: fractional input (sfix).

    :return:  arctan of :py:obj:`x` (sfix).
    """
    # obtain absolute value of x
    s = x < 0
    x_abs  = (s * (-2) + 1) * x
    # angle isolation
    b = x_abs > 1
    v = (types.cfix(1.0) / x_abs)
    v = (1 - b) * (x_abs - v) + v
    v_2 =v*v

    # range of polynomial coefficients
    assert x.k - x.f >= 18
    P = p_eval(p_5102, v_2)
    Q = p_eval(q_5102, v_2)

    # padding
    y = v * (P / Q)
    y_pi_over_two =  pi_over_2 - y

    # sign correction
    y = (1 - b) * (y - y_pi_over_two) + y_pi_over_two
    y = (1 - s) * (y - (-y)) + (-y)

    return y


def asin(x):
    """
    Returns the arcsine (sfix) of any given fractional value.

    :param x: fractional input (sfix). valid interval is :math:`-1 \le x \le 1`

    :return:  arcsin of :py:obj:`x` (sfix).
    """
    # Square x
    x_2 = x*x
    # trignometric identities
    sqrt_l = sqrt(1- (x_2))
    x_sqrt_l =x / sqrt_l
    return atan(x_sqrt_l)


def acos(x):
    """
    Returns the arccosine (sfix) of any given fractional value.

    :param x: fractional input (sfix). :math:`-1 \le x \le 1`

    :return:  arccos of :py:obj:`x` (sfix).
    """
    y = asin(x)
    return pi_over_2 - y
