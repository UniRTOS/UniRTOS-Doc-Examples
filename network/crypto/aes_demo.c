#include "qcm_aes.h"
#include "qosa_sys.h"

void app_aes_demo(void) {
    qosa_uint8_t key[16] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                            0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};
    
    qosa_uint8_t org_data[16] = "HelloWorld!1234";
    qosa_uint8_t enc_data[16] = {0};
    qosa_uint8_t dec_data[16] = {0};

    /* 1. 加密 */
    qcm_aes_string_encryption(key, 16, org_data, enc_data);
    
    /* 2. 解密 */
    qcm_aes_string_decryption(key, 16, enc_data, dec_data);
}