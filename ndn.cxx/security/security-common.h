/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2013, Regents of the University of California
 *                     Yingdi Yu
 *
 * BSD license, See the LICENSE file for more information
 *
 * Author: Yingdi Yu <yingdi@cs.ucla.edu>
 */

#ifndef NDN_SECURITY_COMMON_H
#define NDN_SECURITY_COMMON_H

#include "ndn.cxx/common.h"
#include "ndn.cxx/fields/blob.h"

namespace ndn
{

namespace security
{
  enum KeyType{
    KEY_TYPE_RSA,
    // KEY_TYPE_DSA,
    KEY_TYPE_AES,
    // KEY_TYPE_DES,
    // KEY_TYPE_RC4,
    // KEY_TYPE_RC2
  };

  enum KeyClass{
    KEY_CLASS_PUBLIC,
    KEY_CLASS_PRIVATE,
    KEY_CLASS_SYMMETRIC
  };
  
  enum KeyFormat{
    KEY_PUBLIC_OPENSSL,
  };

  enum DigestAlgorithm{
    // DIGEST_MD2,
    // DIGEST_MD5,
    // DIGEST_SHA1,
    DIGEST_SHA256,
  };

  enum EncryptMode{
    EM_DEFAULT,
    EM_CFB_AES,
    // EM_CBC_AES,
  };

}//security

}//ndn

#endif
