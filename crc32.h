#ifndef CRC32

#include <stddef.h>
#include <stdint.h>

/* CRC-32C (iSCSI) polynomial in reversed bit order. */
#define POLY 0xedb88320

/* CRC-32 (Ethernet, ZIP, etc.) polynomial in reversed bit order. */
/* #define POLY 0xedb88320 */

uint32_t crc32c(uint32_t crc, unsigned char *buf, size_t len);

#define CRC32
#endif // !CRC32