/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2013-2019 Regents of the University of California.
 *
 * This file is part of ndn-cxx library (NDN C++ library with eXperimental eXtensions).
 *
 * ndn-cxx library is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later version.
 *
 * ndn-cxx library is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more details.
 *
 * You should have received copies of the GNU General Public License and GNU Lesser
 * General Public License along with ndn-cxx, e.g., in COPYING.md file.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * See AUTHORS.md for complete list of ndn-cxx authors and contributors.
 */

#include "ndn-cxx/security/tpm/back-end.hpp"
#include "ndn-cxx/security/tpm/key-handle.hpp"
#include "ndn-cxx/security/tpm/tpm.hpp"
#include "ndn-cxx/security/pib/key.hpp"
#include "ndn-cxx/security/transform/buffer-source.hpp"
#include "ndn-cxx/security/transform/digest-filter.hpp"
#include "ndn-cxx/security/transform/private-key.hpp"
#include "ndn-cxx/security/transform/stream-sink.hpp"
#include "ndn-cxx/encoding/buffer-stream.hpp"
#include "ndn-cxx/util/random.hpp"

namespace ndn {
namespace security {
namespace tpm {

BackEnd::~BackEnd() = default;

bool
BackEnd::hasKey(const Name& keyName) const
{
  return doHasKey(keyName);
}

unique_ptr<KeyHandle>
BackEnd::getKeyHandle(const Name& keyName) const
{
  return doGetKeyHandle(keyName);
}

unique_ptr<KeyHandle>
BackEnd::createKey(const Name& identity, const KeyParams& params)
{
  if (params.getKeyType() == KeyType::HMAC) {
    return doCreateKey(identity, params);
  }

  // key name checking
  switch (params.getKeyIdType()) {
    case KeyIdType::USER_SPECIFIED: { // keyId is pre-set.
      Name keyName = v2::constructKeyName(identity, params.getKeyId());
      if (hasKey(keyName)) {
        NDN_THROW(Tpm::Error("Key `" + keyName.toUri() + "` already exists"));
      }
      break;
    }
    case KeyIdType::SHA256: {
      // KeyName will be assigned in setKeyName after key is generated
      break;
    }
    case KeyIdType::RANDOM: {
      Name keyName;
      name::Component keyId;
      do {
        keyId = name::Component::fromNumber(random::generateSecureWord64());
        keyName = v2::constructKeyName(identity, keyId);
      } while (hasKey(keyName));

      const_cast<KeyParams&>(params).setKeyId(keyId);
      break;
    }
    default: {
      NDN_THROW(Error("Unsupported key id type"));
    }
  }

  return doCreateKey(identity, params);
}

void
BackEnd::deleteKey(const Name& keyName)
{
  doDeleteKey(keyName);
}

ConstBufferPtr
BackEnd::exportKey(const Name& keyName, const char* pw, size_t pwLen)
{
  if (!hasKey(keyName)) {
    NDN_THROW(Error("Key `" + keyName.toUri() + "` does not exist"));
  }
  return doExportKey(keyName, pw, pwLen);
}

void
BackEnd::importKey(const Name& keyName, const uint8_t* pkcs8, size_t pkcs8Len, const char* pw, size_t pwLen)
{
  if (hasKey(keyName)) {
    NDN_THROW(Error("Key `" + keyName.toUri() + "` already exists"));
  }
  doImportKey(keyName, pkcs8, pkcs8Len, pw, pwLen);
}

void
BackEnd::importKey(const Name& keyName, shared_ptr<transform::PrivateKey> key)
{
  if (hasKey(keyName)) {
    NDN_THROW(Error("Key `" + keyName.toUri() + "` already exists"));
  }
  doImportKey(keyName, key);
}

Name
BackEnd::constructAsymmetricKeyName(const KeyHandle& keyHandle, const Name& identity,
                                    const KeyParams& params)
{
  name::Component keyId;

  switch (params.getKeyIdType()) {
    case KeyIdType::USER_SPECIFIED: {
      keyId = params.getKeyId();
      break;
    }
    case KeyIdType::SHA256: {
      using namespace transform;
      OBufferStream os;
      bufferSource(*keyHandle.derivePublicKey()) >>
        digestFilter(DigestAlgorithm::SHA256) >>
        streamSink(os);
      keyId = name::Component(os.buf());
      break;
    }
    case KeyIdType::RANDOM: {
      BOOST_ASSERT(!params.getKeyId().empty());
      keyId = params.getKeyId();
      break;
    }
    default: {
      NDN_THROW(Error("Unsupported key id type"));
    }
  }

  return v2::constructKeyName(identity, keyId);
}

Name
BackEnd::constructHmacKeyName(const transform::PrivateKey& key, const Name& identity,
                              const KeyParams& params)
{
  return Name(identity).append(name::Component(key.getKeyDigest(DigestAlgorithm::SHA256)));
}

bool
BackEnd::isTerminalMode() const
{
  return true;
}

void
BackEnd::setTerminalMode(bool isTerminal) const
{
}

bool
BackEnd::isTpmLocked() const
{
  return false;
}

bool
BackEnd::unlockTpm(const char* pw, size_t pwLen) const
{
  return !isTpmLocked();
}

} // namespace tpm
} // namespace security
} // namespace ndn
