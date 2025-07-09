#include "helpers.h"

/* Magic value tables */

// Validation function
static uint8_t validate_xor_table[8] = {0x20, 0x1a, 0x47, 0x11, 0x50, 0x93, 0x36, 0x8f};
static uint8_t validate_and_table[8] = {0xec, 0x3f, 0xd7, 0xdb, 0x79, 0xf7, 0xbe, 0xef};

// Preamble gen function
static uint8_t preamble_xor_key_table[8] = {0xfe, 0xff, 0x1c, 0x7c, 0xfc, 0x0c, 0x04, 0x3c};
static uint8_t preamble_and_table[8] = {0x41, 0x10, 0x28, 0x86, 0x08, 0xc0, 0x13, 0x24};


bool ant_is_valid_netkey(uint8_t* key) {
	struct ant_netkey k;
	memcpy(k.bytes, key, 8);

    // Xor index variables
    const uint8_t xor_start_offset = 2;
    uint8_t nb_xor = 1;

    // Key as bytes
    uint8_t* k_b = k.bytes;

    uint8_t k_deriv = 0;
    uint8_t tmp;

    for (uint8_t i = 0; i < 8; ++i) {
        tmp = 0;
        // Xor key bytes
        for (uint8_t j = 0; j < nb_xor; ++j) {
            if (j < 6) {
                tmp ^= k_b[xor_start_offset + j];
            } else if (j == 6) {
                tmp ^= k_b[1];
            } else {
                tmp ^= k_b[0];
            }
        }

        // And op
        tmp &= validate_and_table[i];

        // Xor op
        tmp ^= validate_xor_table[i];

        k_deriv |= tmp;
        ++nb_xor;
    }

    return k_deriv == 0;
}

// Generate the key's preamble
// Precondition: the key should be valid
uint16_t ant_gen_netkey_preamble(uint8_t* key) {

	struct ant_netkey k;
	memcpy(k.bytes, key, 8);
    struct ant_netkey_preamble k_p = {0};

    // Key as bytes
    uint8_t* k_b = k.bytes;

    uint8_t tmp = 0;
    uint8_t tmp2;

    for (uint8_t i = 0; i < 8; ++i) {
        tmp2 = 0;
        if (i == 4) {
            k_p.low = tmp;
            tmp = 0;
        }

        for (uint8_t j = 0; j < 8; ++j) {
            if (preamble_xor_key_table[i] & (1 << j)) {
                tmp2 ^= k_b[j];
            }
        }

        tmp2 &= preamble_and_table[i];
        tmp |= tmp2;
    }

    k_p.high = tmp;

	//uint16_t = ((k_p.raw & 0xFF) << 8) | ((k_p.raw & 0xFF00) >> 8);
    return k_p.raw;
}

uint32_t bytewise_bit_swap(uint32_t inp)
{
	inp = (inp & 0xF0F0F0F0) >> 4 | (inp & 0x0F0F0F0F) << 4;
	inp = (inp & 0xCCCCCCCC) >> 2 | (inp & 0x33333333) << 2;
	return (inp & 0xAAAAAAAA) >> 1 | (inp & 0x55555555) << 1;
}

void sort_array(uint32_t tab[], size_t size) {
	for (size_t i=size-1;i>0;i--) {
		for (size_t j=0;j<i;j++) {
			if (tab[j+1] < tab[j]) {
				uint32_t tmp = tab[j];
				tab[j] = tab[j+1];
				tab[j+1] = tmp;
			}
		}
	}
}

bool compare_buffers(uint8_t *buffer1, uint8_t *buffer2,size_t size) {
	for (size_t i=0;i<size;i++) {
		if (buffer1[i] != buffer2[i]) return false;
	}
	return true;
}



uint8_t dewhiten_byte_ble(uint8_t byte, int position, int channel) {
	int i,j;
	uint8_t c;
	uint8_t lfsr = bytewise_bit_swap(channel) | 2;
	for (i=0; i<position; i++) {
		for (j=7; j>=0; j--) {
		if (lfsr & 0x80) {
			lfsr ^= 0x11;
		}
		lfsr <<= 1;
		}
	}
	c = bytewise_bit_swap(byte);
	for (j=7; j>=0; j--) {
		if (lfsr & 0x80) {
			lfsr ^= 0x11;
			c ^= (1<<j);
		}
		lfsr <<= 1;
	}
	return bytewise_bit_swap(c);
}

void dewhiten_ble(uint8_t *data, int len, int channel){
	int i,j;
	uint8_t c;
	uint8_t lfsr = bytewise_bit_swap(channel) | 2;

	for (i=0; i<len; i++) {
		c = bytewise_bit_swap(data[i]);
		for (j=7; j>=0; j--) {
			if (lfsr & 0x80) {
				lfsr ^= 0x11;
				c ^= (1<<j);
			}
			lfsr <<= 1;
		}
		data[i] = bytewise_bit_swap(c);
	}
}

// Function extracted from btlejack firmware
int is_access_address_valid(uint32_t aa)
{
    uint8_t a,b,c,d;
    uint32_t bb;
    int i, t;

    /* Four different bytes. */
    a = (aa & 0xff000000)>>24;
    b = (aa & 0x00ff0000)>>16;
    c = (aa & 0x0000ff00)>>8;
    d = (aa & 0x000000ff);
    if ((a==b) && (b==c) && (c==d))
        return 0;

    /* Not the advertising access address. */
    if (aa == 0x8E89BED6)
        return 0;

    /* Check consecutive bits. */
    bb = aa;
    for (i=0; i<26; i++)
    {
        if (((bb&0x3F) == 0) || ((bb&0x3F)==0x3F))
            return 0;
        bb >>= 1;
    }

    /* Check transitions. */
    bb = aa;
    t = 0;
    a = (bb & 0x80000000)>>31;
    for (i=30; i>=0; i--)
    {
        if (((bb & (1<<i))>>i) != a)
        {
            a = ((bb & (1<<i))>>i);
            t++;
            if (t>24)
                return 0;
        }
        if ((i < 26) && (t<2))
            return 0;
    }

    return 1;
}

uint32_t reverse_crc_ble(uint32_t crc, uint8_t *data, int len) {
	uint32_t state = crc;
	uint32_t lfsr_mask = 0xb4c000;
	uint32_t ret;
    uint8_t cur;
	int i, j, top_bit;

	for (i = len - 1; i >= 0; --i) {
		cur = data[i];
		for (j = 0; j < 8; ++j) {
			top_bit = state >> 23;
			state = (state << 1) & 0xffffff;
			state |= top_bit ^ ((cur >> (7 - j)) & 1);
			if (top_bit)
				state ^= lfsr_mask;
		}
	}

	ret = 0;
	for (i = 0; i < 24; ++i)
		ret |= ((state >> i) & 1) << (23 - i);

	return ret;
}


int hamming(uint8_t *demod_buffer,uint8_t *pattern) {
	int count = 0;
	for (int i=0;i<4;i++) {
		for (int j=0;j<8;j++) {
			if (((pattern[i] >> (7-j)) & 0x01) != (((demod_buffer[i] & (i==0 && j==0 ? 0x7F : 0xFF)) >> (7-j)) & 0x01)) {
				count++;
			}
		}
	}
	return count;
}

void shift_buffer(uint8_t *demod_buffer,int size) {
	for (int i=0;i<size;i++) {
		if (i != 0) {
			demod_buffer[i-1]=((demod_buffer[i] & 0x80) >> 7) | demod_buffer[i-1];
		}
		demod_buffer[i] = demod_buffer[i] << 1;
	}
}

uint16_t update_fcs_dot15d4(uint8_t byte, uint16_t fcs) {
	uint16_t q = (fcs ^ byte) & 15;
	fcs = (fcs / 16) ^ (q * 4225);
	q = (fcs ^ (byte / 16)) & 15;
	fcs = (fcs / 16) ^ (q * 4225);
	return fcs;
}

uint16_t calculate_fcs_dot15d4(uint8_t data[], int size) {
	uint16_t fcs = 0;
	for (int i=2; i<size-2;i++) {
		fcs = update_fcs_dot15d4(data[i],fcs);
	}
	return fcs;
}

bool check_fcs_dot15d4(uint8_t data[],int size) {
	uint16_t fcs = calculate_fcs_dot15d4(data,size);
	return ((fcs & 0xFF) == data[size-2]) && (((fcs >> 8) & 0xFF) == data[size-1]);
}

bool correct_size_dot15d4(uint8_t data[],int max) {
	int i=2;
	uint16_t fcs = 0;
	int correctSize = 0;
	while (i < max-2) {
		correctSize++;
		fcs = update_fcs_dot15d4(data[i],fcs);
		if (((fcs & 0xFF) == data[i+1]) && (((fcs >> 8) & 0xFF) == data[i+2])) {
			data[1] = correctSize+2;
			return true;
		}
		i++;
	}
	return false;
}


unsigned short calculate_crc_mosart(uint8_t *ptr, int count)
{
   uint16_t  crc;
   uint8_t i;
   crc = 0;
   while (--count >= 0)
   {
      crc = crc ^ (int) *ptr++ << 8;
      i = 8;
      do
      {
         if (crc & 0x8000)
            crc = crc << 1 ^ 0x1021;
         else
            crc = crc << 1;
      } while(--i);
   }
   return (crc);
}
