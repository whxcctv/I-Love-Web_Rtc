/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/main/test/TestVADDTX.h"

#include <string>

#include "webrtc/engine_configurations.h"
#include "webrtc/modules/audio_coding/main/test/PCMFile.h"
#include "webrtc/modules/audio_coding/main/test/utility.h"
#include "webrtc/test/testsupport/fileutils.h"

namespace webrtc {

#ifdef WEBRTC_CODEC_ISAC
const CodecInst kIsacWb = {103, "ISAC", 16000, 480, 1, 32000};
const CodecInst kIsacSwb = {104, "ISAC", 32000, 960, 1, 56000};
#endif

#ifdef WEBRTC_CODEC_ILBC
const CodecInst kIlbc = {102, "ILBC", 8000, 240, 1, 13300};
#endif

#ifdef WEBRTC_CODEC_OPUS
const CodecInst kOpus = {120, "opus", 48000, 960, 1, 64000};
const CodecInst kOpusStereo = {120, "opus", 48000, 960, 2, 64000};
#endif

ActivityMonitor::ActivityMonitor() {
  ResetStatistics();
}

int32_t ActivityMonitor::InFrameType(int16_t frame_type) {
  counter_[frame_type]++;
  return 0;
}

void ActivityMonitor::PrintStatistics() {
  printf("\n");
  printf("kActiveNormalEncoded  kPassiveNormalEncoded  kPassiveDTXWB  ");
  printf("kPassiveDTXNB kPassiveDTXSWB kNoEncoding\n");
  printf("%19u", counter_[1]);
  printf("%22u", counter_[2]);
  printf("%14u", counter_[3]);
  printf("%14u", counter_[4]);
  printf("%14u", counter_[5]);
  printf("%11u", counter_[0]);
  printf("\n\n");
}

void ActivityMonitor::ResetStatistics() {
  memset(counter_, 0, sizeof(counter_));
}

void ActivityMonitor::GetStatistics(uint32_t* counter) {
  memcpy(counter, counter_, sizeof(counter_));
}

TestVadDtx::TestVadDtx()
    : acm_send_(AudioCodingModule::Create(0)),
      acm_receive_(AudioCodingModule::Create(1)),
      channel_(new Channel),
      monitor_(new ActivityMonitor) {
  EXPECT_EQ(0, acm_send_->RegisterTransportCallback(channel_.get()));
  channel_->RegisterReceiverACM(acm_receive_.get());
  EXPECT_EQ(0, acm_send_->RegisterVADCallback(monitor_.get()));
  assert(monitor_->kPacketTypes == this->kPacketTypes);
}

void TestVadDtx::RegisterCodec(CodecInst codec_param) {
  // Set the codec for sending and receiving.
  EXPECT_EQ(0, acm_send_->RegisterSendCodec(codec_param));
  EXPECT_EQ(0, acm_receive_->RegisterReceiveCodec(codec_param));
  channel_->SetIsStereo(codec_param.channels > 1);
}

// Encoding a file and see if the numbers that various packets occur follow
// the expectation.
void TestVadDtx::Run(std::string in_filename, int frequency, int channels,
                     std::string out_filename, bool append,
                     const int* expects) {
  monitor_->ResetStatistics();

  PCMFile in_file;
  in_file.Open(in_filename, frequency, "rb");
  in_file.ReadStereo(channels > 1);

  PCMFile out_file;
  if (append) {
    out_file.Open(out_filename, kOutputFreqHz, "ab");
  } else {
    out_file.Open(out_filename, kOutputFreqHz, "wb");
  }

  uint16_t frame_size_samples = in_file.PayloadLength10Ms();
  uint32_t time_stamp = 0x12345678;
  AudioFrame audio_frame;
  while (!in_file.EndOfFile()) {
    in_file.Read10MsData(audio_frame);
    audio_frame.timestamp_ = time_stamp;
    time_stamp += frame_size_samples;
    EXPECT_GE(acm_send_->Add10MsData(audio_frame), 0);
    acm_receive_->PlayoutData10Ms(kOutputFreqHz, &audio_frame);
    out_file.Write10MsData(audio_frame);
  }

  in_file.Close();
  out_file.Close();

#ifdef PRINT_STAT
  monitor_->PrintStatistics();
#endif

  uint32_t stats[kPacketTypes];
  monitor_->GetStatistics(stats);
  monitor_->ResetStatistics();

  for (int i = 0; i < kPacketTypes; i++) {
    switch (expects[i]) {
      case 0: {
        EXPECT_EQ(static_cast<uint32_t>(0), stats[i]) << "stats["
                                                      << i
                                                      << "] error.";
        break;
      }
      case 1: {
        EXPECT_GT(stats[i], static_cast<uint32_t>(0)) << "stats["
                                                      << i
                                                      << "] error.";
        break;
      }
    }
  }
}

// Following is the implementation of TestWebRtcVadDtx.
TestWebRtcVadDtx::TestWebRtcVadDtx()
    : vad_enabled_(false),
      dtx_enabled_(false),
      use_webrtc_dtx_(false),
      output_file_num_(0) {
}

void TestWebRtcVadDtx::Perform() {
  // Go through various test cases.
#ifdef WEBRTC_CODEC_ISAC
  // Register iSAC WB as send codec
  RegisterCodec(kIsacWb);
  RunTestCases();

  // Register iSAC SWB as send codec
  RegisterCodec(kIsacSwb);
  RunTestCases();
#endif

#ifdef WEBRTC_CODEC_ILBC
  // Register iLBC as send codec
  RegisterCodec(kIlbc);
  RunTestCases();
#endif

#ifdef WEBRTC_CODEC_OPUS
  // Register Opus as send codec
  RegisterCodec(kOpus);
  RunTestCases();
#endif
}

// Test various configurations on VAD/DTX.
void TestWebRtcVadDtx::RunTestCases() {
  // #1 DTX = OFF, VAD = OFF, VADNormal
  SetVAD(false, false, VADNormal);
  Test(true);

  // #2 DTX = ON, VAD = ON, VADAggr
  SetVAD(true, true, VADAggr);
  Test(false);

  // #3 DTX = ON, VAD = ON, VADLowBitrate
  SetVAD(true, true, VADLowBitrate);
  Test(false);

  // #4 DTX = ON, VAD = ON, VADVeryAggr
  SetVAD(true, true, VADVeryAggr);
  Test(false);

  // #5 DTX = ON, VAD = ON, VADNormal
  SetVAD(true, true, VADNormal);
  Test(false);
}

// Set the expectation and run the test.
void TestWebRtcVadDtx::Test(bool new_outfile) {
  int expects[kPacketTypes];
  int frequency = acm_send_->SendFrequency();
  expects[0] = -1;  // Do not care.
  expects[1] = 1;
  expects[2] = vad_enabled_ && !use_webrtc_dtx_;
  expects[3] = use_webrtc_dtx_ && (frequency == 8000);
  expects[4] = use_webrtc_dtx_ && (frequency == 16000);
  expects[5] = use_webrtc_dtx_ && (frequency == 32000);
  if (new_outfile) {
    output_file_num_++;
  }
  std::stringstream out_filename;
  out_filename << webrtc::test::OutputPath()
               << "testWebRtcVadDtx_outFile_"
               << output_file_num_
               << ".pcm";
  Run(webrtc::test::ResourcePath("audio_coding/testfile32kHz", "pcm"),
      32000, 1, out_filename.str(), !new_outfile, expects);
}

void TestWebRtcVadDtx::SetVAD(bool enable_dtx, bool enable_vad,
                              ACMVADMode vad_mode) {
  ACMVADMode mode;
  EXPECT_EQ(0, acm_send_->SetVAD(enable_dtx, enable_vad, vad_mode));
  EXPECT_EQ(0, acm_send_->VAD(&dtx_enabled_, &vad_enabled_, &mode));

  CodecInst codec_param;
  acm_send_->SendCodec(&codec_param);
  if (STR_CASE_CMP(codec_param.plname, "opus") == 0) {
    // If send codec is Opus, WebRTC VAD/DTX cannot be used.
    enable_dtx = enable_vad = false;
  }

  EXPECT_EQ(dtx_enabled_ , enable_dtx); // DTX should be set as expected.

  bool replaced = false;
  acm_send_->IsInternalDTXReplacedWithWebRtc(&replaced);

  use_webrtc_dtx_ = dtx_enabled_ && replaced;

  if (use_webrtc_dtx_) {
    EXPECT_TRUE(vad_enabled_); // WebRTC DTX cannot run without WebRTC VAD.
  }

  if (!dtx_enabled_ || !use_webrtc_dtx_) {
    // Using no DTX or codec Internal DTX should not affect setting of VAD.
    EXPECT_EQ(enable_vad, vad_enabled_);
  }
}

// Following is the implementation of TestOpusDtx.
void TestOpusDtx::Perform() {
#ifdef WEBRTC_CODEC_OPUS
  int expects[kPacketTypes] = {0, 1, 0, 0, 0, 0};

  // Register Opus as send codec
  std::string out_filename = webrtc::test::OutputPath() +
      "testOpusDtx_outFile_mono.pcm";
  RegisterCodec(kOpus);
  EXPECT_EQ(0, acm_send_->DisableOpusDtx());

  Run(webrtc::test::ResourcePath("audio_coding/testfile32kHz", "pcm"),
      32000, 1, out_filename, false, expects);

  EXPECT_EQ(0, acm_send_->EnableOpusDtx());
  expects[0] = 1;
  Run(webrtc::test::ResourcePath("audio_coding/testfile32kHz", "pcm"),
      32000, 1, out_filename, true, expects);

  // Register stereo Opus as send codec
  out_filename = webrtc::test::OutputPath() + "testOpusDtx_outFile_stereo.pcm";
  RegisterCodec(kOpusStereo);
  EXPECT_EQ(0, acm_send_->DisableOpusDtx());
  expects[0] = 0;
  Run(webrtc::test::ResourcePath("audio_coding/teststereo32kHz", "pcm"),
      32000, 2, out_filename, false, expects);

  // Opus DTX should only work in Voip mode.
  EXPECT_EQ(0, acm_send_->SetOpusApplication(kVoip));
  EXPECT_EQ(0, acm_send_->EnableOpusDtx());

  expects[0] = 1;
  Run(webrtc::test::ResourcePath("audio_coding/teststereo32kHz", "pcm"),
      32000, 2, out_filename, true, expects);
#endif
}

}  // namespace webrtc
