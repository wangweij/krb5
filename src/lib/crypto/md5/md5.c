/*
 ***********************************************************************
 ** md5.c -- the source code for MD5 routines                         **
 ** RSA Data Security, Inc. MD5 Message-Digest Algorithm              **
 ** Created: 2/17/90 RLR                                              **
 ** Revised: 1/91 SRD,AJ,BSK,JT Reference C ver., 7/10 constant corr. **
 ***********************************************************************
 */

/*
 * Modified by John Carr, MIT, to use Kerberos 5 typedefs.
 */

/*
 ***********************************************************************
 ** Copyright (C) 1990, RSA Data Security, Inc. All rights reserved.  **
 **                                                                   **
 ** License to copy and use this software is granted provided that    **
 ** it is identified as the "RSA Data Security, Inc. MD5 Message-     **
 ** Digest Algorithm" in all material mentioning or referencing this  **
 ** software or this function.                                        **
 **                                                                   **
 ** License is also granted to make and use derivative works          **
 ** provided that such works are identified as "derived from the RSA  **
 ** Data Security, Inc. MD5 Message-Digest Algorithm" in all          **
 ** material mentioning or referencing the derived work.              **
 **                                                                   **
 ** RSA Data Security, Inc. makes no representations concerning       **
 ** either the merchantability of this software or the suitability    **
 ** of this software for any particular purpose.  It is provided "as  **
 ** is" without express or implied warranty of any kind.              **
 **                                                                   **
 ** These notices must be retained in any copies of any part of this  **
 ** documentation and/or software.                                    **
 ***********************************************************************
 */

#include <krb5/krb5.h>
#include <krb5/rsa-md5.h>

/*
 ***********************************************************************
 **  Message-digest routines:                                         **
 **  To form the message digest for a message M                       **
 **    (1) Initialize a context buffer mdContext using MD5Init        **
 **    (2) Call MD5Update on mdContext and M                          **
 **    (3) Call MD5Final on mdContext                                 **
 **  The message digest is now in mdContext->digest[0...15]           **
 ***********************************************************************
 */

/* forward declaration */
static void Transform ();

static unsigned char PADDING[64] = {
  0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/* F, G, H and I are basic MD5 functions */
#define F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define G(x, y, z) (((x) & (z)) | ((y) & (~z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define I(x, y, z) ((y) ^ ((x) | (~z)))

/* ROTATE_LEFT rotates x left n bits */
#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32-(n))))

/* FF, GG, HH, and II transformations for rounds 1, 2, 3, and 4 */
/* Rotation is separate from addition to prevent recomputation */
#define FF(a, b, c, d, x, s, ac) \
  {(a) += F ((b), (c), (d)) + (x) + (krb5_ui_4)(ac); \
   (a) = ROTATE_LEFT ((a), (s)); \
   (a) += (b); \
  }
#define GG(a, b, c, d, x, s, ac) \
  {(a) += G ((b), (c), (d)) + (x) + (krb5_ui_4)(ac); \
   (a) = ROTATE_LEFT ((a), (s)); \
   (a) += (b); \
  }
#define HH(a, b, c, d, x, s, ac) \
  {(a) += H ((b), (c), (d)) + (x) + (krb5_ui_4)(ac); \
   (a) = ROTATE_LEFT ((a), (s)); \
   (a) += (b); \
  }
#define II(a, b, c, d, x, s, ac) \
  {(a) += I ((b), (c), (d)) + (x) + (krb5_ui_4)(ac); \
   (a) = ROTATE_LEFT ((a), (s)); \
   (a) += (b); \
  }

/* The routine MD5Init initializes the message-digest context
   mdContext. All fields are set to zero.
 */
void MD5Init (mdContext)
MD5_CTX *mdContext;
{
  mdContext->i[0] = mdContext->i[1] = (krb5_ui_4)0;

  /* Load magic initialization constants.
   */
  mdContext->buf[0] = (krb5_ui_4)0x67452301L;
  mdContext->buf[1] = (krb5_ui_4)0xefcdab89L;
  mdContext->buf[2] = (krb5_ui_4)0x98badcfeL;
  mdContext->buf[3] = (krb5_ui_4)0x10325476L;
}

/* The routine MD5Update updates the message-digest context to
   account for the presence of each of the characters inBuf[0..inLen-1]
   in the message whose digest is being computed.
 */
void MD5Update (mdContext, inBuf, inLen)
MD5_CTX *mdContext;
unsigned char *inBuf;
unsigned int inLen;
{
  krb5_ui_4 in[16];
  int mdi;
  unsigned int i, ii;

  /* compute number of bytes mod 64 */
  mdi = (int)((mdContext->i[0] >> 3) & 0x3F);

  /* update number of bits */
  if ((mdContext->i[0] + ((krb5_ui_4)inLen << 3)) < mdContext->i[0])
    mdContext->i[1]++;
  mdContext->i[0] += ((krb5_ui_4)inLen << 3);
  mdContext->i[1] += ((krb5_ui_4)inLen >> 29);

  while (inLen--) {
    /* add new character to buffer, increment mdi */
    mdContext->in[mdi++] = *inBuf++;

    /* transform if necessary */
    if (mdi == 0x40) {
      for (i = 0, ii = 0; i < 16; i++, ii += 4)
        in[i] = (((krb5_ui_4)mdContext->in[ii+3]) << 24) |
                (((krb5_ui_4)mdContext->in[ii+2]) << 16) |
                (((krb5_ui_4)mdContext->in[ii+1]) << 8) |
                ((krb5_ui_4)mdContext->in[ii]);
      Transform (mdContext->buf, in);
      mdi = 0;
    }
  }
}

/* The routine MD5Final terminates the message-digest computation and
   ends with the desired message digest in mdContext->digest[0...15].
 */
void MD5Final (mdContext)
MD5_CTX *mdContext;
{
  krb5_ui_4 in[16];
  int mdi;
  unsigned int i, ii;
  unsigned int padLen;

  /* save number of bits */
  in[14] = mdContext->i[0];
  in[15] = mdContext->i[1];

  /* compute number of bytes mod 64 */
  mdi = (int)((mdContext->i[0] >> 3) & 0x3F);

  /* pad out to 56 mod 64 */
  padLen = (mdi < 56) ? (56 - mdi) : (120 - mdi);
  MD5Update (mdContext, PADDING, padLen);

  /* append length in bits and transform */
  for (i = 0, ii = 0; i < 14; i++, ii += 4)
    in[i] = (((krb5_ui_4)mdContext->in[ii+3]) << 24) |
            (((krb5_ui_4)mdContext->in[ii+2]) << 16) |
            (((krb5_ui_4)mdContext->in[ii+1]) << 8) |
            ((krb5_ui_4)mdContext->in[ii]);
  Transform (mdContext->buf, in);

  /* store buffer in digest */
  for (i = 0, ii = 0; i < 4; i++, ii += 4) {
    mdContext->digest[ii] = (unsigned char)(mdContext->buf[i] & 0xFF);
    mdContext->digest[ii+1] =
      (unsigned char)((mdContext->buf[i] >> 8) & 0xFF);
    mdContext->digest[ii+2] =
      (unsigned char)((mdContext->buf[i] >> 16) & 0xFF);
    mdContext->digest[ii+3] =
      (unsigned char)((mdContext->buf[i] >> 24) & 0xFF);
  }
}

/* Basic MD5 step. Transforms buf based on in.
 */
static void Transform (buf, in)
krb5_ui_4 *buf;
krb5_ui_4 *in;
{
  register krb5_ui_4 a = buf[0], b = buf[1], c = buf[2], d = buf[3];

  /* Round 1 */
#define S11 7
#define S12 12
#define S13 17
#define S14 22
  FF ( a, b, c, d, in[ 0], S11, 3614090360L); /* 1 */
  FF ( d, a, b, c, in[ 1], S12, 3905402710L); /* 2 */
  FF ( c, d, a, b, in[ 2], S13,  606105819L); /* 3 */
  FF ( b, c, d, a, in[ 3], S14, 3250441966L); /* 4 */
  FF ( a, b, c, d, in[ 4], S11, 4118548399L); /* 5 */
  FF ( d, a, b, c, in[ 5], S12, 1200080426L); /* 6 */
  FF ( c, d, a, b, in[ 6], S13, 2821735955L); /* 7 */
  FF ( b, c, d, a, in[ 7], S14, 4249261313L); /* 8 */
  FF ( a, b, c, d, in[ 8], S11, 1770035416L); /* 9 */
  FF ( d, a, b, c, in[ 9], S12, 2336552879L); /* 10 */
  FF ( c, d, a, b, in[10], S13, 4294925233L); /* 11 */
  FF ( b, c, d, a, in[11], S14, 2304563134L); /* 12 */
  FF ( a, b, c, d, in[12], S11, 1804603682L); /* 13 */
  FF ( d, a, b, c, in[13], S12, 4254626195L); /* 14 */
  FF ( c, d, a, b, in[14], S13, 2792965006L); /* 15 */
  FF ( b, c, d, a, in[15], S14, 1236535329L); /* 16 */

  /* Round 2 */
#define S21 5
#define S22 9
#define S23 14
#define S24 20
  GG ( a, b, c, d, in[ 1], S21, 4129170786L); /* 17 */
  GG ( d, a, b, c, in[ 6], S22, 3225465664L); /* 18 */
  GG ( c, d, a, b, in[11], S23,  643717713L); /* 19 */
  GG ( b, c, d, a, in[ 0], S24, 3921069994L); /* 20 */
  GG ( a, b, c, d, in[ 5], S21, 3593408605L); /* 21 */
  GG ( d, a, b, c, in[10], S22,   38016083L); /* 22 */
  GG ( c, d, a, b, in[15], S23, 3634488961L); /* 23 */
  GG ( b, c, d, a, in[ 4], S24, 3889429448L); /* 24 */
  GG ( a, b, c, d, in[ 9], S21,  568446438L); /* 25 */
  GG ( d, a, b, c, in[14], S22, 3275163606L); /* 26 */
  GG ( c, d, a, b, in[ 3], S23, 4107603335L); /* 27 */
  GG ( b, c, d, a, in[ 8], S24, 1163531501L); /* 28 */
  GG ( a, b, c, d, in[13], S21, 2850285829L); /* 29 */
  GG ( d, a, b, c, in[ 2], S22, 4243563512L); /* 30 */
  GG ( c, d, a, b, in[ 7], S23, 1735328473L); /* 31 */
  GG ( b, c, d, a, in[12], S24, 2368359562L); /* 32 */

  /* Round 3 */
#define S31 4
#define S32 11
#define S33 16
#define S34 23
  HH ( a, b, c, d, in[ 5], S31, 4294588738L); /* 33 */
  HH ( d, a, b, c, in[ 8], S32, 2272392833L); /* 34 */
  HH ( c, d, a, b, in[11], S33, 1839030562L); /* 35 */
  HH ( b, c, d, a, in[14], S34, 4259657740L); /* 36 */
  HH ( a, b, c, d, in[ 1], S31, 2763975236L); /* 37 */
  HH ( d, a, b, c, in[ 4], S32, 1272893353L); /* 38 */
  HH ( c, d, a, b, in[ 7], S33, 4139469664L); /* 39 */
  HH ( b, c, d, a, in[10], S34, 3200236656L); /* 40 */
  HH ( a, b, c, d, in[13], S31,  681279174L); /* 41 */
  HH ( d, a, b, c, in[ 0], S32, 3936430074L); /* 42 */
  HH ( c, d, a, b, in[ 3], S33, 3572445317L); /* 43 */
  HH ( b, c, d, a, in[ 6], S34,   76029189L); /* 44 */
  HH ( a, b, c, d, in[ 9], S31, 3654602809L); /* 45 */
  HH ( d, a, b, c, in[12], S32, 3873151461L); /* 46 */
  HH ( c, d, a, b, in[15], S33,  530742520L); /* 47 */
  HH ( b, c, d, a, in[ 2], S34, 3299628645L); /* 48 */

  /* Round 4 */
#define S41 6
#define S42 10
#define S43 15
#define S44 21
  II ( a, b, c, d, in[ 0], S41, 4096336452L); /* 49 */
  II ( d, a, b, c, in[ 7], S42, 1126891415L); /* 50 */
  II ( c, d, a, b, in[14], S43, 2878612391L); /* 51 */
  II ( b, c, d, a, in[ 5], S44, 4237533241L); /* 52 */
  II ( a, b, c, d, in[12], S41, 1700485571L); /* 53 */
  II ( d, a, b, c, in[ 3], S42, 2399980690L); /* 54 */
  II ( c, d, a, b, in[10], S43, 4293915773L); /* 55 */
  II ( b, c, d, a, in[ 1], S44, 2240044497L); /* 56 */
  II ( a, b, c, d, in[ 8], S41, 1873313359L); /* 57 */
  II ( d, a, b, c, in[15], S42, 4264355552L); /* 58 */
  II ( c, d, a, b, in[ 6], S43, 2734768916L); /* 59 */
  II ( b, c, d, a, in[13], S44, 1309151649L); /* 60 */
  II ( a, b, c, d, in[ 4], S41, 4149444226L); /* 61 */
  II ( d, a, b, c, in[11], S42, 3174756917L); /* 62 */
  II ( c, d, a, b, in[ 2], S43,  718787259L); /* 63 */
  II ( b, c, d, a, in[ 9], S44, 3951481745L); /* 64 */

  buf[0] += a;
  buf[1] += b;
  buf[2] += c;
  buf[3] += d;
}
