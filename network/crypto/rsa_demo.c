#include <stdio.h>
#include <string.h>

#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/error.h"
#include "mbedtls/pk.h"

#define RSA_BUFFER_SIZE 512

/*
 * 替换为实际PEM公钥和私钥。
 *
 * PEM字符串长度必须包含结尾的'\0'。
 */
static const unsigned char g_rsa_public_key[] =
    "-----BEGIN PUBLIC KEY-----\r\n"
    /* Base64 public key */
    "-----END PUBLIC KEY-----\r\n";

static const unsigned char g_rsa_private_key[] =
    "-----BEGIN PRIVATE KEY-----\r\n"
    /* Base64 private key */
    "-----END PRIVATE KEY-----\r\n";
    
static void print_mbedtls_error(const char *operation, int ret)
{
    char error_buf[128] = {0};

    mbedtls_strerror(ret, error_buf, sizeof(error_buf));
    printf("%s failed: -0x%04X (%s)\r\n",
           operation,
           (unsigned int)-ret,
           error_buf);
}

int rsa_encrypt_decrypt_demo(void)
{
    int ret = -1;

    const unsigned char plaintext[] = "Quectel RSA demo";

    unsigned char encrypted[RSA_BUFFER_SIZE] = {0};
    unsigned char decrypted[RSA_BUFFER_SIZE] = {0};

    size_t rsa_len      = 0;
    size_t encrypted_len = 0;
    size_t decrypted_len = 0;

    mbedtls_pk_context       public_key;
    mbedtls_pk_context       private_key;
    mbedtls_entropy_context  entropy;
    mbedtls_ctr_drbg_context ctr_drbg;

    static const unsigned char personalization[] =
        "quectel_rsa_demo";

    /*
     * 1. 初始化全部上下文。
     */
    mbedtls_pk_init(&public_key);
    mbedtls_pk_init(&private_key);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);

    /*
     * 2. 初始化安全随机数生成器。
     */
    ret = mbedtls_ctr_drbg_seed(
        &ctr_drbg,
        mbedtls_entropy_func,
        &entropy,
        personalization,
        sizeof(personalization) - 1);
        
        
    if (ret != 0)
    {
        print_mbedtls_error("mbedtls_ctr_drbg_seed", ret);
        goto cleanup;
    }

    /*
     * 3. 解析RSA公钥。
     * PEM输入长度必须包含结尾的'\0'。
     */
    ret = mbedtls_pk_parse_public_key(
        &public_key,
        g_rsa_public_key,
        sizeof(g_rsa_public_key));

    if (ret != 0)
    {
        print_mbedtls_error("mbedtls_pk_parse_public_key", ret);
        goto cleanup;
    }
    
    
    /*
     * 4. 确认导入的是RSA公钥。
     */
    if (!mbedtls_pk_can_do(&public_key, MBEDTLS_PK_RSA))
    {
        printf("The public key is not an RSA key\r\n");
        ret = MBEDTLS_ERR_PK_TYPE_MISMATCH;
        goto cleanup;
    }

    /*
     * 5. 解析RSA私钥。
     *
     * 当前私钥没有密码，因此pwd传NULL、pwdlen传0。
     */
    ret = mbedtls_pk_parse_key(
        &private_key,
        g_rsa_private_key,
        sizeof(g_rsa_private_key),
        NULL,
        0);
        
         if (ret != 0)
    {
        print_mbedtls_error("mbedtls_pk_parse_key", ret);
        goto cleanup;
    }

    /*
     * 6. 确认导入的是RSA私钥。
     */
    if (!mbedtls_pk_can_do(&private_key, MBEDTLS_PK_RSA))
    {
        printf("The private key is not an RSA key\r\n");
        ret = MBEDTLS_ERR_PK_TYPE_MISMATCH;
        goto cleanup;
    }

    /*
     * 7. 获取RSA输出长度。
     *
     * RSA-2048为256字节，RSA-4096为512字节。
     */
    rsa_len = mbedtls_pk_get_len(&public_key);

    if (rsa_len == 0 || rsa_len > sizeof(encrypted))
    {
        printf("Invalid RSA length: %u\r\n", (unsigned int)rsa_len);
        ret = MBEDTLS_ERR_PK_BAD_INPUT_DATA;
        goto cleanup;
    }
    
    
    if (mbedtls_pk_get_len(&private_key) != rsa_len)
    {
        printf("Public and private key lengths do not match\r\n");
        ret = MBEDTLS_ERR_PK_TYPE_MISMATCH;
        goto cleanup;
    }

    /*
     * 8. 使用RSA公钥加密。
     *
     * sizeof(plaintext) - 1排除字符串结尾的'\0'。
     */
    ret = mbedtls_pk_encrypt(
        &public_key,
        plaintext,
        sizeof(plaintext) - 1,
        encrypted,
        &encrypted_len,
        sizeof(encrypted),
        mbedtls_ctr_drbg_random,
        &ctr_drbg);
        
        
    if (ret != 0)
    {
        print_mbedtls_error("mbedtls_pk_encrypt", ret);
        goto cleanup;
    }

    printf("RSA encryption succeeded, ciphertext length: %u\r\n",
           (unsigned int)encrypted_len);

    /*
     * 9. 使用RSA私钥解密。
     */
    ret = mbedtls_pk_decrypt(
        &private_key,
        encrypted,
        encrypted_len,
        decrypted,
        &decrypted_len,
        sizeof(decrypted) - 1,
        mbedtls_ctr_drbg_random,
        &ctr_drbg);
        
    if (ret != 0)
    {
        print_mbedtls_error("mbedtls_pk_decrypt", ret);
        goto cleanup;
    }

    /*
     * 10. 仅为了演示文本输出，补充字符串结束符。
     * 二进制明文不能使用%s输出。
     */
    decrypted[decrypted_len] = '\0';

    printf("RSA decryption succeeded, plaintext length: %u\r\n",
           (unsigned int)decrypted_len);
    printf("Decrypted text: %s\r\n", decrypted);

    /*
     * 11. 验证解密结果。
     */
    if (decrypted_len != sizeof(plaintext) - 1 ||
        memcmp(decrypted, plaintext, decrypted_len) != 0)
    {
        printf("Decrypted data does not match plaintext\r\n");
        ret = -1;
        goto cleanup;
    }
    
     printf("RSA demo succeeded\r\n");
    ret = 0;

cleanup:

    /*
     * 出错时清理可能包含明文的数据。
     * 实际项目建议使用平台提供的安全清零接口。
     */
    memset(decrypted, 0, sizeof(decrypted));
    memset(encrypted, 0, sizeof(encrypted));

    mbedtls_pk_free(&private_key);
    mbedtls_pk_free(&public_key);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);

    return ret;
}