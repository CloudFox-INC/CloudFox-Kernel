// SPDX-License-Identifier: GPL-2.0-only
/*
 * Cryptographic API.
 *
 * LZ4KD: COMPRESS-interface wrapper for the canonical LZ4KD library
 * (lib/lz4kd), imported verbatim from the Huawei P50 (ABR-AL60)
 * HarmonyOS 4.0.0 kernel open source release (5.4.86, as mirrored in
 * 0wnerDied/android_kernel_huawei_sm8350 "zram" branch). See
 * include/linux/lz4kd.h for the algorithm family documentation.
 *
 * This wrapper registers only the legacy CRYPTO_ALG_TYPE_COMPRESS
 * interface: the android12-5.10 zram zcomp layer allocates its tfms
 * via crypto_alloc_comp(), which forces CRYPTO_ALG_TYPE_COMPRESS and
 * can never resolve a CRYPTO_ALG_TYPE_SCOMPRESS registration. A
 * scompress registration would be dead code on this kernel, so it is
 * deliberately omitted.
 *
 * Copyright (c) 2026 CloudFox-INC
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/crypto.h>
#include <linux/lz4kd.h>

/*
 * The LZ4KD encode state is an 8192-byte hash table that doubles as
 * the tfm context (crypto_create_tfm() kzalloc's the context, so the
 * required once-at-creation zeroing is guaranteed; the explicit
 * memset below documents and enforces that contract). Unlike LZ4KDR,
 * lz4kd_encode() also re-zeroes its own state at the top of every
 * call (lib/lz4kd/lz4kd_encode.c), so state retention across calls
 * is safe by construction.
 */
enum {
	LZ4KD_CTX_SIZE = 8192,
};

static int lz4kd_init(struct crypto_tfm *tfm)
{
	memset(crypto_tfm_ctx(tfm), 0, LZ4KD_CTX_SIZE);

	return 0;
}

static void lz4kd_exit(struct crypto_tfm *tfm)
{
}

static int lz4kd_compress_crypto(struct crypto_tfm *tfm, const u8 *src,
				 unsigned int slen, u8 *dst,
				 unsigned int *dlen)
{
	void *ctx = crypto_tfm_ctx(tfm);
	int ret;

	/*
	 * zcomp hands us a 2-page dst and slen = PAGE_SIZE; out_limit
	 * 0 makes the encoder cap itself at min(slen, out_max) = slen,
	 * so a successful result is always <= slen. The encoder is
	 * block-oriented and rejects inputs larger than 4KB.
	 *
	 * ret == 0 means "incompressible": the encoded worst case does
	 * not fit out_max, and the encoder bails out before writing
	 * any output. Report dlen = slen so zram's existing
	 * PAGE_SIZE-output path stores the original input page raw and
	 * never reads dst. dst is left untouched here.
	 */
	ret = lz4kd_encode(ctx, src, dst, slen, *dlen, 0);
	if (ret < 0)
		return -EINVAL;

	if (ret == 0)
		*dlen = slen;
	else
		*dlen = ret;

	return 0;
}

static int lz4kd_decompress_crypto(struct crypto_tfm *tfm, const u8 *src,
				   unsigned int slen, u8 *dst,
				   unsigned int *dlen)
{
	int ret;

	ret = lz4kd_decode(src, dst, slen, *dlen);
	if (ret <= 0)
		return -EINVAL;

	*dlen = ret;
	return 0;
}

static struct crypto_alg alg_lz4kd = {
	.cra_name		= "lz4kd",
	.cra_driver_name	= "lz4kd-generic",
	.cra_flags		= CRYPTO_ALG_TYPE_COMPRESS,
	.cra_ctxsize		= LZ4KD_CTX_SIZE,
	.cra_module		= THIS_MODULE,
	.cra_init		= lz4kd_init,
	.cra_exit		= lz4kd_exit,
	.cra_u			= { .compress = {
	.coa_compress		= lz4kd_compress_crypto,
	.coa_decompress		= lz4kd_decompress_crypto } }
};

static int __init lz4kd_mod_init(void)
{
	if (lz4kd_encode_state_bytes_min() != LZ4KD_CTX_SIZE)
		return -EINVAL;

	return crypto_register_alg(&alg_lz4kd);
}

static void __exit lz4kd_mod_fini(void)
{
	crypto_unregister_alg(&alg_lz4kd);
}

subsys_initcall(lz4kd_mod_init);
module_exit(lz4kd_mod_fini);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("LZ4KD Compression Algorithm");
MODULE_ALIAS_CRYPTO("lz4kd");
