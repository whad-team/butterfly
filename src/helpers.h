#ifndef HELPERS_H
#define HELPERS_H

#include <stdint.h>
#include <stdlib.h>

uint32_t bytewise_bit_swap(uint32_t inp);
void sort_array(uint32_t tab[], size_t size);

bool compareBuffers(uint8_t *buffer1, uint8_t *buffer2,size_t size);



uint8_t dewhiten_byte_ble(uint8_t byte, int position, int channel);
void dewhiten_ble(uint8_t *data, int len, int channel);


int hamming(uint8_t *demod_buffer, uint8_t *pattern);
void shift_buffer(uint8_t *demod_buffer, int size);

uint16_t update_fcs_dot15d4(uint8_t byte, uint16_t fcs);
uint16_t calculate_fcs_dot15d4(uint8_t data[], int size);
bool check_fcs_dot15d4(uint8_t data[], int size);
bool correct_size_dot15d4(uint8_t data[], int size);

unsigned short calculate_crc_mosart(uint8_t *ptr, int count);
#endif
