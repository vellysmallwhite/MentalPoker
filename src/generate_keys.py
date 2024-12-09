from Crypto.Util.number import getPrime
import os
def generate_keys():
    bit_length = 512

    # Generate large prime numbers p and q
    p = getPrime(bit_length)
    q = getPrime(bit_length)

    # Compute n = p * q
    n = p * q

    # Write p, q, and n to the 'key' file
    with open('key', 'w') as f:
        f.write(f"{p}\n{q}\n{n}\n")

if __name__ == "__main__":
    generate_keys()