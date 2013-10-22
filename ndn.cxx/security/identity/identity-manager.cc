/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2013, Regents of the University of California
 *                     Yingdi Yu
 *
 * BSD license, See the LICENSE file for more information
 *
 * Author: Yingdi Yu <yingdi@cs.ucla.edu>
 */

#include "identity-manager.h"

#include "ndn.cxx/fields/key-locator.h"
#include "ndn.cxx/fields/signature-sha256-with-rsa.h"

#include "ndn.cxx/security/exception.h"

#include <ctime>
#include <boost/filesystem.hpp>
#include <fstream>



#include "logging.h"

namespace fs = boost::filesystem;

INIT_LOGGER("ndn.security.IdentityManager")


namespace ndn
{

namespace security
{
  IdentityManager::IdentityManager (Ptr<IdentityStorage> publicStorage, Ptr<PrivatekeyStorage> privateStorage)
    :m_publicStorage(publicStorage),
     m_privateStorage(privateStorage)
  {}

  Name
  IdentityManager::createIdentity (const Name & identity)
  {
    if(!m_publicStorage->doesIdentityExist(identity))
      {
	_LOG_DEBUG("Create Identity");
	m_publicStorage->addIdentity(identity);
	
	_LOG_DEBUG("Create Default RSA key pair");
	Name keyName = generateRSAKeyPairAsDefault(identity, true);

	_LOG_DEBUG("Create self-signed certificate");
	Ptr<IdentityCertificate> selfCert = selfSign(keyName); 
	
	_LOG_DEBUG("Add self-signed certificate as default");

	addCertificateAsDefault(selfCert);

        return keyName;
      }
    else
      throw SecException("Identity has already been created!");
  }

  // void
  // IdentityManager::setDefaultIdentity (const Name & identity)
  // {
  //   m_publicStorage->setDefaultIdentity (identity); 
  // }
  
  Name
  IdentityManager::generateKeyPair (const Name & identity, bool ksk, KeyType keyType, int keySize)
  {
    _LOG_DEBUG("Get new key ID");    
    Name keyName = m_publicStorage->getNewKeyName(identity, ksk);

    _LOG_DEBUG("Generate key pair in private storage");
    m_privateStorage->generateKeyPair(keyName.toUri(), keyType, keySize);

    _LOG_DEBUG("Create a key record in public storage");
    Ptr<Publickey> pubKey = m_privateStorage->getPublickey(keyName.toUri());
    m_publicStorage->addKey(keyName, keyType, pubKey->getKeyBlob());

    return keyName;
  }

  Name 
  IdentityManager::generateRSAKeyPair (const Name & identity, bool ksk, int keySize)
  {
    Name keyName = generateKeyPair(identity, ksk, KEY_TYPE_RSA, keySize);

    return keyName;
  }

  Name
  IdentityManager::generateRSAKeyPairAsDefault (const Name & identity, bool ksk, int keySize)
  {
    Name keyName = generateKeyPair(identity, ksk, KEY_TYPE_RSA, keySize);

    m_publicStorage->setDefaultKeyNameForIdentity(keyName, identity);
    
    return keyName;
  }

  void
  IdentityManager::setDefaultKeyForIdentity (const Name & keyName, const Name & identity)
  {
    m_publicStorage->setDefaultKeyNameForIdentity(keyName, identity);
  }

  Name
  IdentityManager::getDefaultKeyNameForIdentity (const Name & identity)
  {
    return m_publicStorage->getDefaultKeyNameForIdentity (identity);
  }

  Ptr<Publickey>
  IdentityManager::getPublickey(const Name & keyName)
  {
    return Publickey::fromDER(m_publicStorage->getKey(keyName));
  }

  Name
  IdentityManager::getDefaultIdentity ()
  {
    return m_publicStorage->getDefaultIdentity();
  }

  Name
  IdentityManager::createIdentityCertificate (const Name& certificatePrefix,
                                              const Name& signerCertificateName,
                                              const Time& notBefore,
                                              const Time& notAfter)
  {
    Name keyName = getKeyNameFromCertificatePrefix(certificatePrefix);
    
    Ptr<Blob> keyBlob = m_publicStorage->getKey(keyName);
    Ptr<Publickey> publickey = Publickey::fromDER(keyBlob);

    Ptr<IdentityCertificate> certificate = createIdentityCertificate(certificatePrefix,
                                                                     *publickey,
                                                                     signerCertificateName,
                                                                     notBefore,
                                                                     notAfter);

    m_publicStorage->addCertificate(certificate);
    
    return certificate->getName();
  }

  Ptr<IdentityCertificate>
  IdentityManager::createIdentityCertificate (const Name& certificatePrefix,
                                              const Publickey& publickey,
                                              const Name& signerCertificateName,
                                              const Time& notBefore,
                                              const Time& notAfter)
  { 
    Ptr<IdentityCertificate> certificate = Create<IdentityCertificate>();
    Name keyName = getKeyNameFromCertificatePrefix(certificatePrefix);
    
    Name certificateName = certificatePrefix;
    TimeInterval ti = time::NowUnixTimestamp();
    ostringstream oss;
    oss << ti.total_seconds();

    certificateName.append("ID-CERT").append(oss.str());
     
    certificate->setName(certificateName);
    certificate->setNotBefore(notBefore);
    certificate->setNotAfter(notAfter);
    certificate->setPublicKeyInfo(publickey);
    certificate->addSubjectDescription(CertificateSubDescrypt("2.5.4.41", keyName.toUri()));
    certificate->encode();

    Ptr<signature::Sha256WithRsa> sha256Sig = Ptr<signature::Sha256WithRsa>::Create();

    KeyLocator keyLocator;    
    keyLocator.setType (KeyLocator::KEYNAME);
    keyLocator.setKeyName (signerCertificateName);
    
    sha256Sig->setKeyLocator (keyLocator);
    sha256Sig->setPublisherKeyDigest (*publickey.getDigest ());

    certificate->setSignature(sha256Sig);

    Ptr<Blob> unsignedData = certificate->encodeToUnsignedWire();

    Ptr<Blob> sigBits = m_privateStorage->sign (*unsignedData, keyName);
    
    sha256Sig->setSignatureBits(*sigBits);

    return certificate;
  }

  void
  IdentityManager::addCertificate (Ptr<IdentityCertificate> certificate)
  {
    m_publicStorage->addCertificate(certificate);
  }

  void 
  IdentityManager::addCertificateAsDefault (Ptr<IdentityCertificate> certificate)
  {
    m_publicStorage->addCertificate(certificate);
    
    setDefaultCertificateForKey(*certificate);
  }

  void
  IdentityManager::addCertificateAsIdentityDefault (Ptr<IdentityCertificate> certificate)
  {
    m_publicStorage->addCertificate(certificate);

    Name keyName = certificate->getPublicKeyName();
    
    setDefaultKeyForIdentity(keyName);

    setDefaultCertificateForKey(*certificate);
  }

  Ptr<IdentityCertificate>
  IdentityManager::getCertificate (const Name & certName)
  {
    return Ptr<IdentityCertificate>(new IdentityCertificate(*m_publicStorage->getCertificate(certName, false)));
  }

  Ptr<IdentityCertificate>
  IdentityManager::getAnyCertificate (const Name & certName)
  {
    return Ptr<IdentityCertificate>(new IdentityCertificate(*m_publicStorage->getCertificate(certName, true)));
  }

  void
  IdentityManager::setDefaultCertificateForKey (const IdentityCertificate & certificate)
  {
    Name keyName = certificate.getPublicKeyName();
    
    if(!m_publicStorage->doesKeyExist(keyName))
      throw SecException("No corresponding Key record for certificaite!");

    m_publicStorage->setDefaultCertificateNameForKey (keyName, certificate.getName());
  }

  Name
  IdentityManager::getDefaultCertificateNameByIdentity (const Name & identity)
  {
    return m_publicStorage->getDefaultCertificateNameForIdentity(identity);
  }
    
  Name
  IdentityManager::getDefaultCertificateName ()
  {
    return m_publicStorage->getDefaultCertificateNameForIdentity(getDefaultIdentity());
  }

  // Ptr<Signature>
  // IdentityManager::signByIdentity (const Blob & blob, const Name & identity)
  // {
  //   return signByCertificate(blob, m_publicStorage->getDefaultCertificateNameForIdentity(identity));
  // }

  // void
  // IdentityManager::signByIdentity (Data & data, const Name & identity)
  // {
  //   signByCertificate(data, m_publicStorage->getDefaultCertificateNameForIdentity(identity));
  // }

  Ptr<Signature>
  IdentityManager::signByCertificate (const Blob & blob, const Name & certName)
  {   
    Ptr<IdentityCertificate> certificate = getCertificate(certName);
    Name keyName = certificate->getPublicKeyName();
    Ptr<Publickey> publickey = m_privateStorage->getPublickey (keyName.toUri());
    Ptr<Blob> sigBits = m_privateStorage->sign (blob, keyName.toUri());

    //For temporary usage, we support RSA + SHA256 only, but will support more.
    Ptr<signature::Sha256WithRsa> sha256Sig = Ptr<signature::Sha256WithRsa>::Create();

    KeyLocator keyLocator;    
    keyLocator.setType (KeyLocator::KEYNAME);
    keyLocator.setKeyName (certName);
    
    sha256Sig->setKeyLocator (keyLocator);
    sha256Sig->setPublisherKeyDigest (*publickey->getDigest ());
    sha256Sig->setSignatureBits(*sigBits);

    return sha256Sig;
  }

  void
  IdentityManager::signByCertificate (Data & data, const Name & certName)
  {
    Ptr<IdentityCertificate> certificate = getCertificate(certName);
    Name keyName = certificate->getPublicKeyName();
    Ptr<Publickey> publickey = m_privateStorage->getPublickey (keyName.toUri());

    //For temporary usage, we support RSA + SHA256 only, but will support more.
    Ptr<signature::Sha256WithRsa> sha256Sig = Ptr<signature::Sha256WithRsa>::Create();
    KeyLocator keyLocator;
    keyLocator.setType (KeyLocator::KEYNAME);
    keyLocator.setKeyName (certName);
    
    sha256Sig->setKeyLocator (keyLocator);
    sha256Sig->setPublisherKeyDigest (*publickey->getDigest ());
    
    data.setSignature(sha256Sig);

    Ptr<Blob> unsignedData = data.encodeToUnsignedWire();
    Ptr<SignedBlob> signedBlobPtr = Ptr<SignedBlob>(new SignedBlob(unsignedData->buf(), unsignedData->size()));
    signedBlobPtr->setSignedPortion(0, unsignedData->size());
    data.setSignedBlob(signedBlobPtr);

    // DERendec endec;
    // endec.printBlob(*unsignedData, "");
    
    Ptr<Blob> sigBits = m_privateStorage->sign (*unsignedData, keyName.toUri());

    sha256Sig->setSignatureBits(*sigBits);
  }

  Ptr<IdentityCertificate>
  IdentityManager::selfSign (const Name & keyName)
  {
    Ptr<IdentityCertificate> certificate = Create<IdentityCertificate>();
    
    Name certificateName = keyName.getSubName(0, keyName.size()-1);
    certificateName.append("KEY").append(keyName.get(keyName.size()-1)).append("ID-CERT").append("0");
    certificate->setName(certificateName);

    Ptr<Blob> keyBlob = m_publicStorage->getKey(keyName);
    Ptr<Publickey> publickey = Publickey::fromDER(keyBlob);

    tm current = boost::posix_time::to_tm(time::Now());
    current.tm_hour = 0;
    current.tm_min  = 0;
    current.tm_sec  = 0;
    Time notBefore = boost::posix_time::ptime_from_tm(current);
    current.tm_year = current.tm_year + 20;
    Time notAfter = boost::posix_time::ptime_from_tm(current);

    certificate->setNotBefore(notBefore);
    certificate->setNotAfter(notAfter);
    certificate->setPublicKeyInfo(*publickey);
    certificate->addSubjectDescription(CertificateSubDescrypt("2.5.4.41", keyName.toUri()));
    certificate->encode();

    Ptr<signature::Sha256WithRsa> sha256Sig = Ptr<signature::Sha256WithRsa>::Create();

    KeyLocator keyLocator;    
    keyLocator.setType (KeyLocator::KEYNAME);
    keyLocator.setKeyName (certificateName);
    
    sha256Sig->setKeyLocator (keyLocator);
    sha256Sig->setPublisherKeyDigest (*publickey->getDigest ());

    certificate->setSignature(sha256Sig);

    Ptr<Blob> unsignedData = certificate->encodeToUnsignedWire();

    Ptr<Blob> sigBits = m_privateStorage->sign (*unsignedData, keyName.toUri());
    
    sha256Sig->setSignatureBits(*sigBits);

    return certificate;
  }

  Name
  IdentityManager::getKeyNameFromCertificatePrefix(const Name & certificatePrefix)
  {
    Name result;

    string keyString("KEY");
    int i = 0;
    for(; i < certificatePrefix.size(); i++)
      if(certificatePrefix.get(i).toUri() == keyString)
        break;
    
    if(i >= certificatePrefix.size())
      throw SecException("Identity Certificate Prefix does not have KEY component");

    result.append(certificatePrefix.getSubName(0, i));
    result.append(certificatePrefix.getSubName(i+1, certificatePrefix.size()-i-1));
    
    return result;
  }
  
}//security

}//ndn
