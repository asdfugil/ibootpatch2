#ifndef OFFSETFINDER_H
#define OFFSETFINDER_H

#include <stdint.h>
#include <strings.h>

uint64_t find_zero_region(uint64_t region, uint8_t* data, size_t size);

uint64_t find_printf(uint64_t region, uint8_t* data, size_t size);
uint64_t find_check_bootmode(uint64_t region, uint8_t* data, size_t size);
uint64_t find_bootargs_adr(uint64_t region, uint8_t* data, size_t size);
uint64_t find_zero(uint64_t region, uint8_t* data, size_t size);
uint64_t find_kc(uint64_t region, uint8_t* data, size_t size);
uint64_t find_dtre(uint64_t region, uint8_t* data, size_t size);
uint64_t find_avef(uint64_t region, uint8_t* data, size_t size);
uint32_t make_branch(uint64_t w, uint64_t a);
uint64_t find_panic(uint64_t region, uint8_t* data, size_t size);
#endif
