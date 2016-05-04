#include "tools.h"

/* START https://gist.github.com/cellularmitosis/0d8c0abf7f8aa6a2dff3 */
// inspired by http://stackoverflow.com/a/12839870/558735
int hexify(const uint8_t *in, size_t in_size, char *out, size_t out_size)
{
    char map[16+1] = "0123456789abcdef";
    int bytes_written = 0;
    size_t i = 0;
    uint8_t high_nibble;
    uint8_t low_nibble;

    if (in_size == 0 || out_size == 0) return 0;

    
    while(i < in_size && (i*2 + (2+1)) <= out_size)
    {
        high_nibble = (in[i] & 0xF0) >> 4;
        *out = map[high_nibble];
        out++;

        low_nibble = in[i] & 0x0F;
        *out = map[low_nibble];
        out++;

        i++;

        bytes_written += 2;
    }
    *out = '\0';

    return bytes_written;
}
/* END  https://gist.github.com/cellularmitosis/0d8c0abf7f8aa6a2dff3 */
