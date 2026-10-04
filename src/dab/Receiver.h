/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "dab/DataListener.h"
#include "dab/DynamicLabel.h"
#include "dab/ServiceListener.h"
#include "dab/Tuner.h"
#include "device/SampleSource.h"

#include <atomic>
#include <chrono>
#include <complex>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <thread>
#include <unordered_set>
#include <vector>

class Basic_Audio_Channel;
class Basic_Data_Packet_Channel;
class BasicRadio;
class OFDM_Demod;

namespace DABPLUS
{

class CSoftwareAgc;

struct GainConfig
{
  bool automatic{true};
  int manualGain{300}; // 1/10 dB
};

/*!
 * \brief Receives the DAB ensemble on one frequency.
 *
 * Samples are demodulated on one thread and decoded on another, so that a slow decoder does not
 * stall the sample source. Both stages drop data rather than block when they fall behind.
 */
class CReceiver : public ISpiTuner
{
public:
  CReceiver(std::unique_ptr<ISampleSource> source, GainConfig gain);
  ~CReceiver() override;

  bool Start();
  void Stop();

  std::string GetSourceName() const { return m_source->GetName(); }

  bool Tune(uint32_t frequency) override;
  TunerStatus GetStatus() const override;
  std::optional<EnsembleInfo> GetEnsemble() const override;
  bool HasSpiService() const override;

  /*!
   * \brief Decodes the given service and passes its audio and labels to the listener. The
   * selection survives Tune(); the service is decoded as soon as it is found on the frequency.
   */
  void SelectService(uint16_t sid, uint8_t scids, IServiceListener* listener);
  void ClearService();

  /*!
   * \brief Passes the MOT objects of the SPI data services of any tuned ensemble to the listener,
   * which must outlive the receiver.
   */
  void SetDataListener(IDataListener* listener) { m_dataListener = listener; }

private:
  struct ServiceSelection
  {
    uint16_t sid{0};
    uint8_t scids{0};
    IServiceListener* listener{nullptr};
  };

  // Data carries the tune generation it belongs to, so that data of a previous frequency still
  // held by a thread during Tune() is not processed afterwards
  struct SampleBlock
  {
    uint64_t generation{0};
    std::vector<uint8_t> samples;
  };

  struct Frame
  {
    uint64_t generation{0};
    std::vector<int8_t> bits;
  };

  void UpdateServiceSelection();
  void AttachChannel(Basic_Audio_Channel* channel);
  void AttachDataChannel(uint8_t subchannel, Basic_Data_Packet_Channel& channel);
  bool IsSpiSubchannel(uint8_t subchannel) const;
  void OnSamples(std::span<const uint8_t> samples);
  void DemodulateSamples();
  void DecodeFrames();

  const std::unique_ptr<ISampleSource> m_source;
  const GainConfig m_gainConfig;
  bool m_isStarted{false};

  std::mutex m_sampleMutex;
  std::condition_variable m_sampleCondition;
  std::deque<SampleBlock> m_samples;
  std::chrono::steady_clock::time_point m_discardUntil;

  std::mutex m_frameMutex;
  std::condition_variable m_frameCondition;
  std::deque<Frame> m_frames;

  // Guards the demodulator stage
  std::mutex m_demodulatorMutex;
  std::unique_ptr<OFDM_Demod> m_demodulator;
  std::unique_ptr<CSoftwareAgc> m_agc;
  std::vector<std::complex<float>> m_iq;

  // Guards the decoder stage
  mutable std::mutex m_decoderMutex;
  std::unique_ptr<BasicRadio> m_radio;
  uint32_t m_frequency{0};

  // Also guarded by the decoder mutex, the channel callbacks run while the decoder holds it
  std::optional<ServiceSelection> m_selection;
  Basic_Audio_Channel* m_activeChannel{nullptr};
  std::unordered_set<Basic_Audio_Channel*> m_attachedChannels;
  CDynamicLabelDecoder m_labelDecoder;

  std::atomic<IDataListener*> m_dataListener{nullptr};

  std::atomic<uint64_t> m_tuneGeneration{0};

  std::atomic<bool> m_isSynced{false};
  std::atomic<int> m_framesRead{0};
  std::atomic<int> m_framesDesynced{0};
  std::atomic<int> m_gain{0};
  std::atomic<float> m_mer{0.0f};
  std::atomic<float> m_level{-100.0f};
  std::atomic<int> m_uncorrectable{0};

  std::atomic<bool> m_stop{false};
  std::thread m_demodulatorThread;
  std::thread m_decoderThread;
};

} // namespace DABPLUS
