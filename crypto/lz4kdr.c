// SPDX-License-Identifier: GPL-2.0-only
/*
 * Cryptographic API.
 *
 * LZ4KDR: COMPRESS-interface wrapper for the canonical LZ4KDR v1.3
 * library (lib/lz4kdr), imported from firelzrd/zram-ir
 * "0001-linux6.12.74-lz4kdr-1.3.patch" (commit 849c3b7f). See
 * include/linux/lz4kdr.h for the algorithm attribution note.
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
#include <linux/lz4kdr.h>

/*
 * The LZ4KDR encode state is a 2048-byte hash table that doubles as
 * the tfm context (crypto_create_tfm() kzalloc's the context, so the
 * required once-at-creation zeroing is guaranteed; the explicit
 * memset below documents and enforces that contract). The table is
 * intentionally NOT re-zeroed per call -- lz4kdr_encode()'s branchless
 * q<r probe guard makes stale entries safe by construction, and state
 * retention across calls is part of the algorithm's design (see the
 * change-1 note in lib/lz4kdr/lz4kdr_encode.c).
 */
enum {
	LZ4KDR_CTX_SIZE = 2048,
};

static int lz4kdr_init(struct crypto_tfm *tfm)
{
	memset(crypto_tfm_ctx(tfm), 0, LZ4KDR_CTX_SIZE);

	return 0;
}

static void lz4kdr_exit(struct crypto_tfm *tfm)
{
}

static int lz4kdr_compress_crypto(struct crypto_tfm *tfm, const u8 *src,
				  unsigned int slen, u8 *dst,
				  unsigned int *dlen)
{
	void *ctx = crypto_tfm_ctx(tfm);
	int ret;

	/*
	 * zcomp hands us a 2-page dst and slen = PAGE_SIZE; out_limit
	 * 0 makes the encoder cap itself at min(slen, out_max) = slen,
	 * so a successful result is always <= slen.
	 *
	 * ret == 0 means "incompressible": the encoder bailed out
	 * before writing any output. Report dlen = slen so zram's
	 * existing PAGE_SIZE-output path stores the original input
	 * page raw and never reads dst. dst is left untouched here.
	 */
	ret = lz4kdr_encode(ctx, src, dst, slen, *dlen, 0);
	if (ret < 0)
		return -EINVAL;

	if (ret == 0)
		*dlen = slen;
	else
		*dlen = ret;

	return 0;
}

static int lz4kdr_decompress_crypto(struct crypto_tfm *tfm, const u8 *src,
				    unsigned int slen, u8 *dst,
				    unsigned int *dlen)
{
	int ret;

	ret = lz4kdr_decode(src, dst, slen, *dlen);
	if (ret <= 0)
		return -EINVAL;

	*dlen = ret;
	return 0;
}

static struct crypto_alg alg_lz4kdr = {
	.cra_name		= "lz4kdr",
	.cra_driver_name	= "lz4kdr-generic",
	.cra_flags		= CRYPTO_ALG_TYPE_COMPRESS,
	.cra_ctxsize		= LZ4KDR_CTX_SIZE,
	.cra_module		= THIS_MODULE,
	.cra_init		= lz4kdr_init,
	.cra_exit		= lz4kdr_exit,
	.cra_u			= { .compress = {
	.coa_compress		= lz4kdr_compress_crypto,
	.coa_decompress		= lz4kdr_decompress_crypto } }
};

static int __init lz4kdr_mod_init(void)
{
	if (lz4kdr_encode_state_bytes_min() != LZ4KDR_CTX_SIZE)
		return -EINVAL;

	return crypto_register_alg(&alg_lz4kdr);
}

static void __exit lz4kdr_mod_fini(void)
{
	crypto_unregister_alg(&alg_lz4kdr);
}

subsys_initcall(lz4kdr_mod_init);
module_exit(lz4kdr_mod_fini);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("LZ4KDR Compression Algorithm");
MODULE_ALIAS_CRYPTO("lz4kdr");
