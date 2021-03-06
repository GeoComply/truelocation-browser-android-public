/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "secerr.h"
#include "ssl.h"
#include "sslerr.h"
#include "sslproto.h"

#include "gtest_utils.h"
#include "nss_scoped_ptrs.h"
#include "tls_connect.h"
#include "tls_filter.h"
#include "tls_parser.h"

namespace nss_test {

// This class tracks the maximum size of record that was sent, both cleartext
// and plain.  It only tracks records that have an outer type of
// application_data or DTLSCiphertext.  In TLS 1.3, this includes handshake
// messages.
class TlsRecordMaximum : public TlsRecordFilter {
 public:
  TlsRecordMaximum(const std::shared_ptr<TlsAgent>& a)
      : TlsRecordFilter(a), max_ciphertext_(0), max_plaintext_(0) {}

  size_t max_ciphertext() const { return max_ciphertext_; }
  size_t max_plaintext() const { return max_plaintext_; }

 protected:
  PacketFilter::Action FilterRecord(const TlsRecordHeader& header,
                                    const DataBuffer& record, size_t* offset,
                                    DataBuffer* output) override {
    std::cerr << "max: " << record << std::endl;
    // Ignore unprotected packets.
    if (!header.is_protected()) {
      return KEEP;
    }

    max_ciphertext_ = (std::max)(max_ciphertext_, record.len());
    return TlsRecordFilter::FilterRecord(header, record, offset, output);
  }

  PacketFilter::Action FilterRecord(const TlsRecordHeader& header,
                                    const DataBuffer& data,
                                    DataBuffer* changed) override {
    max_plaintext_ = (std::max)(max_plaintext_, data.len());
    return KEEP;
  }

 private:
  size_t max_ciphertext_;
  size_t max_plaintext_;
};

void CheckRecordSizes(const std::shared_ptr<TlsAgent>& agent,
                      const std::shared_ptr<TlsRecordMaximum>& record_max,
                      size_t config) {
  uint16_t cipher_suite;
  ASSERT_TRUE(agent->cipher_suite(&cipher_suite));

  size_t expansion;
  size_t iv;
  switch (cipher_suite) {
    case TLS_AES_128_GCM_SHA256:
    case TLS_AES_256_GCM_SHA384:
    case TLS_CHACHA20_POLY1305_SHA256:
      expansion = 16;
      iv = 0;
      break;

    case TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256:
      expansion = 16;
      iv = 8;
      break;

    case TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA:
      // Expansion is 20 for the MAC.  Maximum block padding is 16.  Maximum
      // padding is added when the input plus the MAC is an exact multiple of
      // the block size.
      expansion = 20 + 16 - ((config + 20) % 16);
      iv = 16;
      break;

    default:
      ADD_FAILURE() << "No expansion set for ciphersuite "
                    << agent->cipher_suite_name();
      return;
  }

  switch (agent->version()) {
    case SSL_LIBRARY_VERSION_TLS_1_3:
      EXPECT_EQ(0U, iv) << "No IV for TLS 1.3";
      // We only have decryption in TLS 1.3.
      EXPECT_EQ(config - 1, record_max->max_plaintext())
          << "bad plaintext length for " << agent->role_str();
      break;

    case SSL_LIBRARY_VERSION_TLS_1_2:
    case SSL_LIBRARY_VERSION_TLS_1_1:
      expansion += iv;
      break;

    case SSL_LIBRARY_VERSION_TLS_1_0:
      break;

    default:
      ADD_FAILURE() << "Unexpected version " << agent->version();
      return;
  }

  EXPECT_EQ(config + expansion, record_max->max_ciphertext())
      << "bad ciphertext length for " << agent->role_str();
}

TEST_P(TlsConnectGeneric, RecordSizeMaximum) {
  uint16_t max_record_size =
      (version_ >= SSL_LIBRARY_VERSION_TLS_1_3) ? 16385 : 16384;
  size_t send_size = (version_ >= SSL_LIBRARY_VERSION_TLS_1_3)
                         ? max_record_size
                         : max_record_size + 1;

  EnsureTlsSetup();
  auto client_max = MakeTlsFilter<TlsRecordMaximum>(client_);
  auto server_max = MakeTlsFilter<TlsRecordMaximum>(server_);
  if (version_ >= SSL_LIBRARY_VERSION_TLS_1_3) {
    client_max->EnableDecryption();
    server_max->EnableDecryption();
  }

  Connect();
  client_->SendData(send_size, send_size);
  server_->SendData(send_size, send_size);
  server_->ReadBytes(send_size);
  client_->ReadBytes(send_size);

  CheckRecordSizes(client_, client_max, max_record_size);
  CheckRecordSizes(server_, server_max, max_record_size);
}

TEST_P(TlsConnectGeneric, RecordSizeMinimumClient) {
  EnsureTlsSetup();
  auto server_max = MakeTlsFilter<TlsRecordMaximum>(server_);
  if (version_ >= SSL_LIBRARY_VERSION_TLS_1_3) {
    server_max->EnableDecryption();
  }

  client_->SetOption(SSL_RECORD_SIZE_LIMIT, 64);
  Connect();
  SendReceive(127);  // Big enough for one record, allowing for 1+N splitting.

  CheckRecordSizes(server_, server_max, 64);
}

TEST_P(TlsConnectGeneric, RecordSizeMinimumServer) {
  EnsureTlsSetup();
  auto client_max = MakeTlsFilter<TlsRecordMaximum>(client_);
  if (version_ >= SSL_LIBRARY_VERSION_TLS_1_3) {
    client_max->EnableDecryption();
  }

  server_->SetOption(SSL_RECORD_SIZE_LIMIT, 64);
  Connect();
  SendReceive(127);

  CheckRecordSizes(client_, client_max, 64);
}

TEST_P(TlsConnectGeneric, RecordSizeAsymmetric) {
  EnsureTlsSetup();
  auto client_max = MakeTlsFilter<TlsRecordMaximum>(client_);
  auto server_max = MakeTlsFilter<TlsRecordMaximum>(server_);
  if (version_ >= SSL_LIBRARY_VERSION_TLS_1_3) {
    client_max->EnableDecryption();
    server_max->EnableDecryption();
  }

  client_->SetOption(SSL_RECORD_SIZE_LIMIT, 64);
  server_->SetOption(SSL_RECORD_SIZE_LIMIT, 100);
  Connect();
  SendReceive(127);

  CheckRecordSizes(client_, client_max, 100);
  CheckRecordSizes(server_, server_max, 64);
}

// This just modifies the encrypted payload so to include a few extra zeros.
class TlsRecordExpander : public TlsRecordFilter {
 public:
  TlsRecordExpander(const std::shared_ptr<TlsAgent>& a, size_t expansion)
      : TlsRecordFilter(a), expansion_(expansion) {}

 protected:
  virtual PacketFilter::Action FilterRecord(const TlsRecordHeader& header,
                                            const DataBuffer& data,
                                            DataBuffer* changed) {
    if (!header.is_protected()) {
      // We're targeting application_data records. If the record is
      // |!is_protected()|, we have two possibilities:
      if (!decrypting()) {
        //  1) We're not decrypting, in which this case this is truly an
        //  unencrypted record (Keep).
        return KEEP;
      }
      if (header.content_type() != ssl_ct_application_data) {
        //  2) We are decrypting, so is_protected() read the internal
        //  content_type. If the internal ct IS NOT application_data, then
        //  it's not our target (Keep).
        return KEEP;
      }
      // Otherwise, the the internal ct IS application_data (Change).
    }

    changed->Allocate(data.len() + expansion_);
    changed->Write(0, data.data(), data.len());
    return CHANGE;
  }

 private:
  size_t expansion_;
};

// Tweak the plaintext of server records so that they exceed the client's limit.
TEST_F(TlsConnectStreamTls13, RecordSizePlaintextExceed) {
  EnsureTlsSetup();
  auto server_expand = MakeTlsFilter<TlsRecordExpander>(server_, 1);
  server_expand->EnableDecryption();

  client_->SetOption(SSL_RECORD_SIZE_LIMIT, 64);
  Connect();

  server_->SendData(100);

  client_->ExpectReadWriteError();
  ExpectAlert(client_, kTlsAlertRecordOverflow);
  client_->ReadBytes(100);
  EXPECT_EQ(SSL_ERROR_RX_RECORD_TOO_LONG, client_->error_code());

  // Consume the alert at the server.
  server_->Handshake();
  server_->CheckErrorCode(SSL_ERROR_RECORD_OVERFLOW_ALERT);
}

// Tweak the ciphertext of server records so that they greatly exceed the limit.
// This requires a much larger expansion than for plaintext to trigger the
// guard, which runs before decryption (current allowance is 320 octets,
// see MAX_EXPANSION in ssl3con.c).
TEST_F(TlsConnectStreamTls13, RecordSizeCiphertextExceed) {
  EnsureTlsSetup();

  client_->SetOption(SSL_RECORD_SIZE_LIMIT, 64);
  Connect();

  auto server_expand = MakeTlsFilter<TlsRecordExpander>(server_, 336);
  server_->SendData(100);

  client_->ExpectReadWriteError();
  ExpectAlert(client_, kTlsAlertRecordOverflow);
  client_->ReadBytes(100);
  EXPECT_EQ(SSL_ERROR_RX_RECORD_TOO_LONG, client_->error_code());

  // Consume the alert at the server.
  server_->Handshake();
  server_->CheckErrorCode(SSL_ERROR_RECORD_OVERFLOW_ALERT);
}

TEST_F(TlsConnectStreamTls13, ClientHelloF5Padding) {
  EnsureTlsSetup();
  ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
  ScopedPK11SymKey key(
      PK11_KeyGen(slot.get(), CKM_NSS_CHACHA20_POLY1305, nullptr, 32, nullptr));

  auto filter =
      MakeTlsFilter<TlsHandshakeRecorder>(client_, kTlsHandshakeClientHello);

  // Add PSK with label long enough to push CH length into [256, 511].
  std::vector<uint8_t> label(100);
  EXPECT_EQ(SECSuccess,
            SSL_AddExternalPsk(client_->ssl_fd(), key.get(), label.data(),
                               label.size(), ssl_hash_sha256));
  StartConnect();
  client_->Handshake();

  // Filter removes the 4B handshake header.
  EXPECT_EQ(508UL, filter->buffer().len());
}

// This indiscriminately adds padding to application data records.
class TlsRecordPadder : public TlsRecordFilter {
 public:
  TlsRecordPadder(const std::shared_ptr<TlsAgent>& a, size_t padding)
      : TlsRecordFilter(a), padding_(padding) {}

 protected:
  PacketFilter::Action FilterRecord(const TlsRecordHeader& header,
                                    const DataBuffer& record, size_t* offset,
                                    DataBuffer* output) override {
    if (!header.is_protected()) {
      return KEEP;
    }

    uint16_t protection_epoch;
    uint8_t inner_content_type;
    DataBuffer plaintext;
    TlsRecordHeader out_header;
    if (!Unprotect(header, record, &protection_epoch, &inner_content_type,
                   &plaintext, &out_header)) {
      return KEEP;
    }

    if (decrypting() && inner_content_type != ssl_ct_application_data) {
      return KEEP;
    }

    DataBuffer ciphertext;
    bool ok = Protect(spec(protection_epoch), out_header, inner_content_type,
                      plaintext, &ciphertext, &out_header, padding_);
    EXPECT_TRUE(ok);
    if (!ok) {
      return KEEP;
    }
    *offset = out_header.Write(output, *offset, ciphertext);
    return CHANGE;
  }

 private:
  size_t padding_;
};

TEST_F(TlsConnectStreamTls13, RecordSizeExceedPad) {
  EnsureTlsSetup();
  auto server_max = std::make_shared<TlsRecordMaximum>(server_);
  auto server_expand = std::make_shared<TlsRecordPadder>(server_, 1);
  server_->SetFilter(std::make_shared<ChainedPacketFilter>(
      ChainedPacketFilterInit({server_max, server_expand})));
  server_expand->EnableDecryption();

  client_->SetOption(SSL_RECORD_SIZE_LIMIT, 64);
  Connect();

  server_->SendData(100);

  client_->ExpectReadWriteError();
  ExpectAlert(client_, kTlsAlertRecordOverflow);
  client_->ReadBytes(100);
  EXPECT_EQ(SSL_ERROR_RX_RECORD_TOO_LONG, client_->error_code());

  // Consume the alert at the server.
  server_->Handshake();
  server_->CheckErrorCode(SSL_ERROR_RECORD_OVERFLOW_ALERT);
}

TEST_P(TlsConnectGeneric, RecordSizeBadValues) {
  EnsureTlsSetup();
  EXPECT_EQ(SECFailure,
            SSL_OptionSet(client_->ssl_fd(), SSL_RECORD_SIZE_LIMIT, 63));
  EXPECT_EQ(SECFailure,
            SSL_OptionSet(client_->ssl_fd(), SSL_RECORD_SIZE_LIMIT, -1));
  EXPECT_EQ(SECFailure,
            SSL_OptionSet(server_->ssl_fd(), SSL_RECORD_SIZE_LIMIT, 16386));
  Connect();
}

TEST_P(TlsConnectGeneric, RecordSizeGetValues) {
  EnsureTlsSetup();
  int v;
  EXPECT_EQ(SECSuccess,
            SSL_OptionGet(client_->ssl_fd(), SSL_RECORD_SIZE_LIMIT, &v));
  EXPECT_EQ(16385, v);
  client_->SetOption(SSL_RECORD_SIZE_LIMIT, 300);
  EXPECT_EQ(SECSuccess,
            SSL_OptionGet(client_->ssl_fd(), SSL_RECORD_SIZE_LIMIT, &v));
  EXPECT_EQ(300, v);
  Connect();
}

// The value of the extension is capped by the maximum version of the client.
TEST_P(TlsConnectGeneric, RecordSizeCapExtensionClient) {
  EnsureTlsSetup();
  client_->SetOption(SSL_RECORD_SIZE_LIMIT, 16385);
  auto capture =
      MakeTlsFilter<TlsExtensionCapture>(client_, ssl_record_size_limit_xtn);
  if (version_ >= SSL_LIBRARY_VERSION_TLS_1_3) {
    capture->EnableDecryption();
  }
  Connect();

  uint64_t val = 0;
  EXPECT_TRUE(capture->extension().Read(0, 2, &val));
  if (version_ < SSL_LIBRARY_VERSION_TLS_1_3) {
    EXPECT_EQ(16384U, val) << "Extension should be capped";
  } else {
    EXPECT_EQ(16385U, val);
  }
}

// The value of the extension is capped by the maximum version of the server.
TEST_P(TlsConnectGeneric, RecordSizeCapExtensionServer) {
  EnsureTlsSetup();
  server_->SetOption(SSL_RECORD_SIZE_LIMIT, 16385);
  auto capture =
      MakeTlsFilter<TlsExtensionCapture>(server_, ssl_record_size_limit_xtn);
  if (version_ >= SSL_LIBRARY_VERSION_TLS_1_3) {
    capture->EnableDecryption();
  }
  Connect();

  uint64_t val = 0;
  EXPECT_TRUE(capture->extension().Read(0, 2, &val));
  if (version_ < SSL_LIBRARY_VERSION_TLS_1_3) {
    EXPECT_EQ(16384U, val) << "Extension should be capped";
  } else {
    EXPECT_EQ(16385U, val);
  }
}

// Damage the client extension and the handshake fails, but the server
// doesn't generate a validation error.
TEST_P(TlsConnectGenericPre13, RecordSizeClientExtensionInvalid) {
  EnsureTlsSetup();
  client_->SetOption(SSL_RECORD_SIZE_LIMIT, 1000);
  static const uint8_t v[] = {0xf4, 0x1f};
  MakeTlsFilter<TlsExtensionReplacer>(client_, ssl_record_size_limit_xtn,
                                      DataBuffer(v, sizeof(v)));
  ConnectExpectAlert(server_, kTlsAlertDecryptError);
}

// Special handling for TLS 1.3, where the alert isn't read.
TEST_F(TlsConnectStreamTls13, RecordSizeClientExtensionInvalid) {
  EnsureTlsSetup();
  client_->SetOption(SSL_RECORD_SIZE_LIMIT, 1000);
  static const uint8_t v[] = {0xf4, 0x1f};
  MakeTlsFilter<TlsExtensionReplacer>(client_, ssl_record_size_limit_xtn,
                                      DataBuffer(v, sizeof(v)));
  client_->ExpectSendAlert(kTlsAlertBadRecordMac);
  server_->ExpectSendAlert(kTlsAlertBadRecordMac);
  ConnectExpectFail();
}

TEST_P(TlsConnectGeneric, RecordSizeServerExtensionInvalid) {
  EnsureTlsSetup();
  server_->SetOption(SSL_RECORD_SIZE_LIMIT, 1000);
  static const uint8_t v[] = {0xf4, 0x1f};
  auto replace = MakeTlsFilter<TlsExtensionReplacer>(
      server_, ssl_record_size_limit_xtn, DataBuffer(v, sizeof(v)));
  if (version_ >= SSL_LIBRARY_VERSION_TLS_1_3) {
    replace->EnableDecryption();
  }
  ConnectExpectAlert(client_, kTlsAlertIllegalParameter);
}

TEST_P(TlsConnectGeneric, RecordSizeServerExtensionExtra) {
  EnsureTlsSetup();
  server_->SetOption(SSL_RECORD_SIZE_LIMIT, 1000);
  static const uint8_t v[] = {0x01, 0x00, 0x00};
  auto replace = MakeTlsFilter<TlsExtensionReplacer>(
      server_, ssl_record_size_limit_xtn, DataBuffer(v, sizeof(v)));
  if (version_ >= SSL_LIBRARY_VERSION_TLS_1_3) {
    replace->EnableDecryption();
  }
  ConnectExpectAlert(client_, kTlsAlertDecodeError);
}

class RecordSizeDefaultsTest : public ::testing::Test {
 public:
  void SetUp() {
    EXPECT_EQ(SECSuccess,
              SSL_OptionGetDefault(SSL_RECORD_SIZE_LIMIT, &default_));
  }
  void TearDown() {
    // Make sure to restore the default value at the end.
    EXPECT_EQ(SECSuccess,
              SSL_OptionSetDefault(SSL_RECORD_SIZE_LIMIT, default_));
  }

 private:
  PRIntn default_ = 0;
};

TEST_F(RecordSizeDefaultsTest, RecordSizeBadValues) {
  EXPECT_EQ(SECFailure, SSL_OptionSetDefault(SSL_RECORD_SIZE_LIMIT, 63));
  EXPECT_EQ(SECFailure, SSL_OptionSetDefault(SSL_RECORD_SIZE_LIMIT, -1));
  EXPECT_EQ(SECFailure, SSL_OptionSetDefault(SSL_RECORD_SIZE_LIMIT, 16386));
}

TEST_F(RecordSizeDefaultsTest, RecordSizeGetValue) {
  int v;
  EXPECT_EQ(SECSuccess, SSL_OptionGetDefault(SSL_RECORD_SIZE_LIMIT, &v));
  EXPECT_EQ(16385, v);
  EXPECT_EQ(SECSuccess, SSL_OptionSetDefault(SSL_RECORD_SIZE_LIMIT, 3000));
  EXPECT_EQ(SECSuccess, SSL_OptionGetDefault(SSL_RECORD_SIZE_LIMIT, &v));
  EXPECT_EQ(3000, v);
}

class TlsCtextResizer : public TlsRecordFilter {
 public:
  TlsCtextResizer(const std::shared_ptr<TlsAgent>& a, size_t size)
      : TlsRecordFilter(a), size_(size) {}

 protected:
  virtual PacketFilter::Action FilterRecord(const TlsRecordHeader& header,
                                            const DataBuffer& data,
                                            DataBuffer* changed) {
    // allocate and initialise buffer
    changed->Allocate(size_);

    // copy record data (partially)
    changed->Write(0, data.data(),
                   ((data.len() >= size_) ? size_ : data.len()));

    return CHANGE;
  }

 private:
  size_t size_;
};

/* (D)TLS overlong record test for maximum default record size of
 * 2^14 + (256 (TLS 1.3) OR 2048 (TLS <= 1.2)
 * [RFC8446, Section 5.2; RFC5246 , Section 6.2.3].
 * This should fail the first size check in ssl3gthr.c/ssl3_GatherData().
 * DTLS Record errors are dropped silently. [RFC6347, Section 4.1.2.7]. */
TEST_P(TlsConnectGeneric, RecordGatherOverlong) {
  EnsureTlsSetup();

  size_t max_ctext = MAX_FRAGMENT_LENGTH;
  if (version_ >= SSL_LIBRARY_VERSION_TLS_1_3) {
    max_ctext += TLS_1_3_MAX_EXPANSION;
  } else {
    max_ctext += TLS_1_2_MAX_EXPANSION;
  }

  Connect();

  MakeTlsFilter<TlsCtextResizer>(server_, max_ctext + 1);
  // Dummy record will be overwritten
  server_->SendData(0xf0);

  /* Drop DTLS Record Errors silently [RFC6347, Section 4.1.2.7]. */
  if (variant_ == ssl_variant_datagram) {
    size_t received = client_->received_bytes();
    client_->ReadBytes(max_ctext + 1);
    ASSERT_EQ(received, client_->received_bytes());
  } else {
    client_->ExpectSendAlert(kTlsAlertRecordOverflow);
    client_->ReadBytes(max_ctext + 1);
    server_->ExpectReceiveAlert(kTlsAlertRecordOverflow);
    server_->Handshake();
  }
}

/* (D)TLS overlong record test with recordSizeLimit Extension and plus RFC
 * specified maximum Expansion: 2^14 + (256 (TLS 1.3) OR 2048 (TLS <= 1.2)
 * [RFC8446, Section 5.2; RFC5246 , Section 6.2.3].
 * DTLS Record errors are dropped silently. [RFC6347, Section 4.1.2.7]. */
TEST_P(TlsConnectGeneric, RecordSizeExtensionOverlong) {
  EnsureTlsSetup();

  // Set some boundary
  size_t max_ctext = 1000;

  client_->SetOption(SSL_RECORD_SIZE_LIMIT, max_ctext);

  if (version_ >= SSL_LIBRARY_VERSION_TLS_1_3) {
    // The record size limit includes the inner content type byte
    max_ctext += TLS_1_3_MAX_EXPANSION - 1;
  } else {
    max_ctext += TLS_1_2_MAX_EXPANSION;
  }

  Connect();

  MakeTlsFilter<TlsCtextResizer>(server_, max_ctext + 1);
  // Dummy record will be overwritten
  server_->SendData(0xf);

  /* Drop DTLS Record Errors silently [RFC6347, Section 4.1.2.7].
   * For DTLS 1.0 and 1.2 the package is dropped before the size check because
   * of the modification. This just tests that no error is thrown as required.
   */
  if (variant_ == ssl_variant_datagram) {
    size_t received = client_->received_bytes();
    client_->ReadBytes(max_ctext + 1);
    ASSERT_EQ(received, client_->received_bytes());
  } else {
    client_->ExpectSendAlert(kTlsAlertRecordOverflow);
    client_->ReadBytes(max_ctext + 1);
    server_->ExpectReceiveAlert(kTlsAlertRecordOverflow);
    server_->Handshake();
  }
}

/* For TLS <= 1.2:
 * MAX_EXPANSION is the amount by which a record might plausibly be expanded
 * when protected.  It's the worst case estimate, so the sum of block cipher
 * padding (up to 256 octets), HMAC (48 octets for SHA-384), and IV (16
 * octets for AES). */
#define MAX_EXPANSION (256 + 48 + 16)

/* (D)TLS overlong record test for specific ciphersuite expansion.
 * Testing the smallest illegal record.
 * This check is performed in ssl3con.c/ssl3_UnprotectRecord() OR
 * tls13con.c/tls13_UnprotectRecord() and enforces stricter size limitations,
 * dependent on the implemented cipher suites, than the RFC.
 * DTLS Record errors are dropped silently. [RFC6347, Section 4.1.2.7]. */
TEST_P(TlsConnectGeneric, RecordExpansionOverlong) {
  EnsureTlsSetup();

  // Set some boundary
  size_t max_ctext = 1000;

  client_->SetOption(SSL_RECORD_SIZE_LIMIT, max_ctext);

  if (version_ >= SSL_LIBRARY_VERSION_TLS_1_3) {
    // For TLS1.3 all ciphers expand the cipherext by 16B
    // The inner content type byte is included in the record size limit
    max_ctext += 16;
  } else {
    // For TLS<=1.2 the max possible expansion in the NSS implementation is 320
    max_ctext += MAX_EXPANSION;
  }

  Connect();

  MakeTlsFilter<TlsCtextResizer>(server_, max_ctext + 1);
  // Dummy record will be overwritten
  server_->SendData(0xf);

  /* Drop DTLS Record Errors silently [RFC6347, Section 4.1.2.7].
   * For DTLS 1.0 and 1.2 the package is dropped before the size check because
   * of the modification. This just tests that no error is thrown as required/
   * no bytes are received. */
  if (variant_ == ssl_variant_datagram) {
    size_t received = client_->received_bytes();
    client_->ReadBytes(max_ctext + 1);
    ASSERT_EQ(received, client_->received_bytes());
  } else {
    client_->ExpectSendAlert(kTlsAlertRecordOverflow);
    client_->ReadBytes(max_ctext + 1);
    server_->ExpectReceiveAlert(kTlsAlertRecordOverflow);
    server_->Handshake();
  }
}

/* (D)TLS longest allowed record default size test. */
TEST_P(TlsConnectGeneric, RecordSizeDefaultLong) {
  EnsureTlsSetup();
  Connect();

  // Maximum allowed plaintext size
  size_t max = MAX_FRAGMENT_LENGTH;

  /* For TLS 1.0 the first byte of application data is sent in a single record
   * as explained in the documentation of SSL_CBC_RANDOM_IV in ssl.h.
   * Because of that we use TlsCTextResizer to send a record of max size.
   * A bad record mac alert is expected since we modify the record. */
  if (version_ == SSL_LIBRARY_VERSION_TLS_1_0 &&
      variant_ == ssl_variant_stream) {
    // Set size to maxi plaintext + max allowed expansion
    MakeTlsFilter<TlsCtextResizer>(server_, max + MAX_EXPANSION);
    // Dummy record will be overwritten
    server_->SendData(0xF);
    // Expect alert
    client_->ExpectSendAlert(kTlsAlertBadRecordMac);
    // Receive record
    client_->ReadBytes(max);
    // Handle alert on server side
    server_->ExpectReceiveAlert(kTlsAlertBadRecordMac);
    server_->Handshake();
  } else {  // Everything but TLS 1.0
    // Send largest legal plaintext as single record
    // by setting SendData() block size to max.
    server_->SendData(max, max);
    // Receive record
    client_->ReadBytes(max);
    // Assert that data was received successfully
    ASSERT_EQ(client_->received_bytes(), max);
  }
}

/* (D)TLS longest allowed record size limit extension test. */
TEST_P(TlsConnectGeneric, RecordSizeLimitLong) {
  EnsureTlsSetup();

  // Set some boundary
  size_t max = 1000;
  client_->SetOption(SSL_RECORD_SIZE_LIMIT, max);

  Connect();

  // For TLS 1.3 the InnerContentType byte is included in the record size limit
  if (version_ == SSL_LIBRARY_VERSION_TLS_1_3) {
    max--;
  }

  /* For TLS 1.0 the first byte of application data is sent in a single record
   * as explained in the documentation of SSL_CBC_RANDOM_IV in ssl.h.
   * Because of that we use TlsCTextResizer to send a record of max size.
   * A bad record mac alert is expected since we modify the record. */
  if (version_ == SSL_LIBRARY_VERSION_TLS_1_0 &&
      variant_ == ssl_variant_stream) {
    // Set size to maxi plaintext + max allowed expansion
    MakeTlsFilter<TlsCtextResizer>(server_, max + MAX_EXPANSION);
    // Dummy record will be overwritten
    server_->SendData(0xF);
    // Expect alert
    client_->ExpectSendAlert(kTlsAlertBadRecordMac);
    // Receive record
    client_->ReadBytes(max);
    // Handle alert on server side
    server_->ExpectReceiveAlert(kTlsAlertBadRecordMac);
    server_->Handshake();
  } else {  // Everything but TLS 1.0
    // Send largest legal plaintext as single record
    // by setting SendData() block size to max.
    server_->SendData(max, max);
    // Receive record
    client_->ReadBytes(max);
    // Assert that data was received successfully
    ASSERT_EQ(client_->received_bytes(), max);
  }
}

}  // namespace nss_test