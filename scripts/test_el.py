# python prototype to verify eisel-lemire float parsing algorithm correctness
def test_el(mantissa, q):
    import math, decimal
    decimal.getcontext().prec = 100
    
    val = decimal.Decimal(10) ** q
    e = math.floor(math.log2(float(val))) - 63 if -300 < q < 300 else math.floor(math.log2(10)*q) - 63
    
    m_dec = val * (decimal.Decimal(2) ** -e)
    table_m = int(m_dec)
    rem = m_dec - table_m
    if rem > 0.5 or (rem == 0.5 and (table_m % 2) == 1):
        table_m += 1
    if table_m >= (1 << 64):
        table_m //= 2
        e += 1
        
    lz = 63 - int(math.log2(mantissa))
    w = mantissa << lz
    
    high = (w * table_m) >> 64
    
    msb = 63 if (high >> 63) == 1 else 62
    shift = msb - 52
    
    mantissa_53 = high >> shift
    
    mask = (1 << shift) - 1
    discarded = high & mask
    
    if discarded > (1 << (shift - 1)):
        mantissa_53 += 1
        
    if mantissa_53 >= (1 << 53):
        mantissa_53 >>= 1
        shift += 1
        
    final_exp = e - lz + 116 + shift
    
    # calculate constructed double
    d_bits = (mantissa_53 & 0xFFFFFFFFFFFFF) | ((final_exp + 1023) << 52)
    
    import struct
    d_val = struct.unpack('d', struct.pack('Q', d_bits))[0]
    print(f"mantissa_53={hex(mantissa_53)}, final_exp={final_exp}, val={d_val}")

test_el(3141592653589793238, -18)
test_el(12345, -19)
test_el(12345678901234567890, 0)
