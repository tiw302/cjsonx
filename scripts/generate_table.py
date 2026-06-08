import sys

def get_power_of_10(q):
    # Returns (mantissa, binary_exponent) such that 10^q ≈ mantissa * 2^binary_exponent
    # mantissa is a 64-bit integer with the MSB set.
    import decimal
    decimal.getcontext().prec = 100
    
    val = decimal.Decimal(10) ** q
    
    # We want val = m * 2^e where m is a 64-bit integer (>= 2^63).
    # log2(val) = log2(m) + e
    # e = floor(log2(val)) - 63
    
    import math
    if val == 0:
        return (0, 0)
        
    log2_val = math.log2(float(val)) if q > -308 and q < 308 else q * math.log2(10)
    
    # Let's do it more precisely
    e = math.floor(math.log2(10) * q) - 63
    
    # m = val / 2^e
    # Since we need exactly 64 bits, we calculate m as an integer.
    # To handle rounding exactly as Eisel-Lemire does, we compute val * 2^(-e).
    
    m_dec = val * (decimal.Decimal(2) ** -e)
    m = int(m_dec)
    
    # Round to nearest
    rem = m_dec - m
    if rem > 0.5 or (rem == 0.5 and (m % 2) == 1):
        m += 1
        
    if m >= (1 << 64):
        m //= 2
        e += 1
        
    return (m, e)

import os

script_dir = os.path.dirname(os.path.abspath(__file__))
table_c_path = os.path.join(script_dir, "generate_table.c")
table_h_path = os.path.join(script_dir, "..", "include", "cjsonx_eisel_lemire.h")

with open(table_c_path, "w") as f:
    f.write('''#include <stdio.h>
#include <stdint.h>
int main() {
    printf("static const uint64_t cjsonx_eisel_lemire_mantissas[] = {\\n");
    // ... we can just generate it directly in python
}
''')

with open(table_h_path, "w") as f:
    f.write("#ifndef CJSONX_EISEL_LEMIRE_H\n")
    f.write("#define CJSONX_EISEL_LEMIRE_H\n\n")
    f.write("#include <stdint.h>\n\n")
    
    f.write("static const uint64_t cjsonx_eisel_lemire_mantissa[] = {\n")
    for q in range(-348, 343):
        m, e = get_power_of_10(q)
        f.write(f"    0x{m:016x}ULL, // 10^{q}\n")
    f.write("};\n\n")
    
    f.write("static const int16_t cjsonx_eisel_lemire_exp[] = {\n")
    for q in range(-348, 343):
        m, e = get_power_of_10(q)
        f.write(f"    {e},\n")
    f.write("};\n\n")
    
    f.write("#endif\n")

print(f"Generated {table_h_path}")
