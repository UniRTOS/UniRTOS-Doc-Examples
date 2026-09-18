#include "mbedtls/des.h"

void des3_encrypt_demo(void)
{
    mbedtls_des3_context ctx;

    unsigned char key[24] = "123456789012345678901234";
    unsigned char input[8] = "Quectel";
    unsigned char output[8];

    mbedtls_des3_init(&ctx);

    mbedtls_des3_set3key_enc(&ctx, key);
    mbedtls_des3_crypt_ecb(&ctx, input, output);

    mbedtls_des3_free(&ctx);
}