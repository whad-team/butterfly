#ifndef HELPERS_H
#define HELPERS_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct ant_netkey {
    union {
        uint8_t bytes[8];
        uint64_t raw;
    };
};

struct ant_netkey_preamble {
    union {
        struct {
            uint8_t low;
            uint8_t high;
        };
        uint16_t raw;
    };
};

#define DIVIDE_ROUND(N, D) ((N) + (D)/2) / (D)

uint16_t ant_gen_netkey_preamble(uint8_t* key);
bool ant_is_valid_netkey(uint8_t* key);

uint32_t bytewise_bit_swap(uint32_t inp);
void sort_array(uint32_t tab[], size_t size);

bool compareBuffers(uint8_t *buffer1, uint8_t *buffer2,size_t size);
int is_access_address_valid(uint32_t aa);


uint8_t dewhiten_byte_ble(uint8_t byte, int position, int channel);
void dewhiten_ble(uint8_t *data, int len, int channel);
uint32_t reverse_crc_ble(uint32_t crc, uint8_t *data, int len);

int hamming(uint8_t *demod_buffer, uint8_t *pattern);
void shift_buffer(uint8_t *demod_buffer, int size);

uint16_t update_fcs_dot15d4(uint8_t byte, uint16_t fcs);
uint16_t calculate_fcs_dot15d4(uint8_t data[], int size);
bool check_fcs_dot15d4(uint8_t data[], int size);
bool correct_size_dot15d4(uint8_t data[], int size);

unsigned short calculate_crc_mosart(uint8_t *ptr, int count);
#endif
