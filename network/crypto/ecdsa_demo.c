#include <stdint.h>
#include <stddef.h>
#include "psa/crypto.h"

int ecdsa_psa_sign_verify_demo(void)
{
    psa_status_t status;
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    mbedtls_svc_key_id_t key = 0;

    const uint8_t message[] = "QuecOS ECDSA PSA demo";
    uint8_t hash[32] = {0};
    size_t hash_len = 0;

    uint8_t signature[PSA_SIGNATURE_MAX_SIZE] = {0};
    size_t signature_len = 0;

    psa_algorithm_t hash_alg = PSA_ALG_SHA_256;
    psa_algorithm_t sign_alg = PSA_ALG_ECDSA(PSA_ALG_SHA_256);

    status = psa_crypto_init();
    if (status != PSA_SUCCESS) {
        return status;
    }

    psa_set_key_type(&attributes, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_R1));
    psa_set_key_bits(&attributes, 256);
    psa_set_key_usage_flags(&attributes,
                            PSA_KEY_USAGE_SIGN_HASH | PSA_KEY_USAGE_VERIFY_HASH);
    psa_set_key_algorithm(&attributes, sign_alg);

    status = psa_generate_key(&attributes, &key);
    psa_reset_key_attributes(&attributes);
    if (status != PSA_SUCCESS) {
        return status;
    }

    status = psa_hash_compute(hash_alg,
                              message,
                              sizeof(message) - 1,
                              hash,
                              sizeof(hash),
                              &hash_len);
    if (status != PSA_SUCCESS) {
        psa_destroy_key(key);
        return status;
    }
    
    status = psa_sign_hash(key,
                           sign_alg,
                           hash,
                           hash_len,
                           signature,
                           sizeof(signature),
                           &signature_len);
    if (status != PSA_SUCCESS) {
        psa_destroy_key(key);
        return status;
    }
    
    status = psa_verify_hash(key,
                             sign_alg,
                             hash,
                             hash_len,
                             signature,
                             signature_len);
    if (status != PSA_SUCCESS) {
        psa_destroy_key(key);
        return status;
    }

    psa_destroy_key(key);
    return PSA_SUCCESS;
}