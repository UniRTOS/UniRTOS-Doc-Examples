#include <string.h>

#include "mbedtls/ctr_drbg.h"
#include "mbedtls/ecdh.h"
#include "mbedtls/entropy.h"

/*
 * 成功时：
 * - shared_key 输出协商得到的共享密钥
 * - shared_key_len 输出共享密钥长度
 *
 * 注意：实际协议中 pub_a/pub_b 应通过网络发送给对端，
 * 共享密钥应再经 HKDF-SHA256 派生后用于 AES-GCM 等算法。
 */
int ecdh_key_agreement_demo(unsigned char *shared_key,
                            size_t shared_key_size,
                            size_t *shared_key_len)
{
    int ret = -1;
    size_t pub_a_len = 0;
    size_t pub_b_len = 0;
    size_t secret_a_len = 0;
    size_t secret_b_len = 0;
    const unsigned char pers[] = "quecos_ecdh";

    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ecdh_context ctx_a;
    mbedtls_ecdh_context ctx_b;
    
     unsigned char pub_a[MBEDTLS_ECP_MAX_PT_LEN];
    unsigned char pub_b[MBEDTLS_ECP_MAX_PT_LEN];
    unsigned char secret_a[MBEDTLS_ECP_MAX_BYTES];
    unsigned char secret_b[MBEDTLS_ECP_MAX_BYTES];

    if (shared_key == NULL || shared_key_len == NULL) {
        return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    }

    *shared_key_len = 0;
    memset(secret_a, 0, sizeof(secret_a));
    memset(secret_b, 0, sizeof(secret_b));
    
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_ecdh_init(&ctx_a);
    mbedtls_ecdh_init(&ctx_b);

    /* 初始化随机数生成器。 */
    ret = mbedtls_ctr_drbg_seed(&ctr_drbg,
                                mbedtls_entropy_func,
                                &entropy,
                                pers,
                                sizeof(pers) - 1);
    if (ret != 0) {
        goto exit;
    }
    
     /* A、B 两端使用相同曲线。 */
    ret = mbedtls_ecdh_setup(&ctx_a, MBEDTLS_ECP_DP_SECP256R1);
    if (ret != 0) {
        goto exit;
    }

    ret = mbedtls_ecdh_setup(&ctx_b, MBEDTLS_ECP_DP_SECP256R1);
    if (ret != 0) {
        goto exit;
    }

    /* A 生成密钥对，并导出公钥发送给 B。 */
    ret = mbedtls_ecdh_make_public(&ctx_a, &pub_a_len,
                                   pub_a, sizeof(pub_a),
                                   mbedtls_ctr_drbg_random, &ctr_drbg);
    if (ret != 0) {
        goto exit;
    }
    
    /* B 生成密钥对，并导出公钥发送给 A。 */
    ret = mbedtls_ecdh_make_public(&ctx_b, &pub_b_len,
                                   pub_b, sizeof(pub_b),
                                   mbedtls_ctr_drbg_random, &ctr_drbg);
    if (ret != 0) {
        goto exit;
    }

    /* A、B 分别导入对端公钥。 */
    ret = mbedtls_ecdh_read_public(&ctx_a, pub_b, pub_b_len);
    if (ret != 0) {
        goto exit;
    }

    ret = mbedtls_ecdh_read_public(&ctx_b, pub_a, pub_a_len);
    if (ret != 0) {
        goto exit;
    }

    /* 双方分别计算共享密钥。 */
    ret = mbedtls_ecdh_calc_secret(&ctx_a, &secret_a_len,
                                   secret_a, sizeof(secret_a),
                                   mbedtls_ctr_drbg_random, &ctr_drbg);
    if (ret != 0) {
        goto exit;
    }
    
    ret = mbedtls_ecdh_calc_secret(&ctx_b, &secret_b_len,
                                   secret_b, sizeof(secret_b),
                                   mbedtls_ctr_drbg_random, &ctr_drbg);
    if (ret != 0) {
        goto exit;
    }

    /* 验证双方计算结果一致。 */
    if (secret_a_len != secret_b_len ||
        secret_a_len > shared_key_size ||
        memcmp(secret_a, secret_b, secret_a_len) != 0) {
        ret = MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
        goto exit;
    }

    memcpy(shared_key, secret_a, secret_a_len);
    *shared_key_len = secret_a_len;
    ret = 0;
    
    exit:
    memset(secret_a, 0, sizeof(secret_a));
    memset(secret_b, 0, sizeof(secret_b));

    mbedtls_ecdh_free(&ctx_b);
    mbedtls_ecdh_free(&ctx_a);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);

    return ret;
}