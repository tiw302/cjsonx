"""
file: test_el.py
description: python script to generate and verify expected outputs for
the eisel-lemire fast float parsing algorithm test vectors.
"""

# python prototype to verify eisel-lemire float parsing algorithm correctness.
# maps input mantissa and power-of-10 exponent (q) to a double-precision float.

def test_el(mantissa, q):
    import math, decimal
    decimal.getcontext().prec = 100

    # step 1: calculate 10^q with 100-digit precision to serve as the ground truth
    val = decimal.Decimal(10) ** q

    # step 2: estimate the binary exponent e to align 10^q into a 64-bit integer range [2^63, 2^64)
    e = math.floor(math.log2(float(val))) - 63 if -300 < q < 300 else math.floor(math.log2(10)*q) - 63

    # step 3: scale and round 10^q to get the 64-bit multiplier table_m
    m_dec = val * (decimal.Decimal(2) ** -e)
    table_m = int(m_dec)
    rem = m_dec - table_m
    if rem > 0.5 or (rem == 0.5 and (table_m % 2) == 1):
        table_m += 1
    if table_m >= (1 << 64):
        table_m //= 2
        e += 1

    # step 4: normalize the input mantissa to have its msb at bit 63
    lz = 63 - int(math.log2(mantissa))
    w = mantissa << lz

    # step 5: perform 64x64-bit multiplication to get the upper 64-bit product (high)
    high = (w * table_m) >> 64

    # step 6: determine the most significant bit (62 or 63) to align to 53 bits of precision
    msb = 63 if (high >> 63) == 1 else 62
    shift = msb - 52

    # step 7: extract the 53-bit mantissa and perform round-to-nearest (halfway to even)
    mantissa_53 = high >> shift

    mask = (1 << shift) - 1
    discarded = high & mask

    if discarded > (1 << (shift - 1)):
        mantissa_53 += 1

    # if rounding caused overflow beyond 53 bits, adjust mantissa and exponent
    if mantissa_53 >= (1 << 53):
        mantissa_53 >>= 1
        shift += 1

    # step 8: calculate the final biased exponent
    final_exp = e - lz + 116 + shift

    # step 9: reconstruct the float64 representation
    # pack 52 bits of mantissa (excluding the implicit bit) and 11 bits of exponent
    d_bits = (mantissa_53 & 0xFFFFFFFFFFFFF) | ((final_exp + 1023) << 52)

    # step 10: convert the raw bits back into a double-precision float
    import struct
    d_val = struct.unpack('d', struct.pack('Q', d_bits))[0]
    print(f"mantissa_53={hex(mantissa_53)}, final_exp={final_exp}, val={d_val}")

# run verification tests
test_el(3141592653589793238, -18)
test_el(12345, -19)
test_el(12345678901234567890, 0)
