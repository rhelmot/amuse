#include "amuse/AudioGroupSampleDirectory.hpp"

#include <cstring>

#include "amuse/AudioGroup.hpp"
#include "amuse/AudioGroupData.hpp"
#include "amuse/Common.hpp"
#include "amuse/DirectoryEnumerator.hpp"
#include "amuse/DSPCodec.hpp"
#include "amuse/N64MusyXCodec.hpp"

#include <athena/FileReader.hpp>
#include <athena/FileWriter.hpp>
#include <athena/MemoryReader.hpp>
#include <athena/VectorWriter.hpp>

#if !defined(_WIN32) && !defined(__MINGW32__)
#include <fcntl.h>
#else
#include <sys/utime.h>
#endif

namespace amuse {

static bool AtEnd32(athena::io::IStreamReader& r) {
  uint32_t v = r.readUint32Big();
  r.seek(-4, athena::SeekOrigin::Current);
  return v == 0xffffffff;
}

void AudioGroupSampleDirectory::ADPCMParms::swapBigDSP() {
  dsp.m_bytesPerFrame = SBig(dsp.m_bytesPerFrame);
  dsp.m_hist2 = SBig(dsp.m_hist2);
  dsp.m_hist1 = SBig(dsp.m_hist1);
  for (int i = 0; i < 8; ++i) {
    dsp.m_coefs[i][0] = SBig(dsp.m_coefs[i][0]);
    dsp.m_coefs[i][1] = SBig(dsp.m_coefs[i][1]);
  }
}

void AudioGroupSampleDirectory::ADPCMParms::swapBigVADPCM() {
  int16_t* allCoefs = reinterpret_cast<int16_t*>(vadpcm.m_coefs[0][0]);
  for (int i = 0; i < 128; ++i)
    allCoefs[i] = SBig(allCoefs[i]);
}

AudioGroupSampleDirectory::AudioGroupSampleDirectory(athena::io::IStreamReader& r, GCNDataTag) {
  while (!AtEnd32(r)) {
    EntryDNA<athena::Endian::Big> ent;
    ent.read(r);
    m_entries[ent.m_sfxId] = MakeObj<Entry>(ent);
    if (SampleId::CurNameDB)
      SampleId::CurNameDB->registerPair(NameDB::generateName(ent.m_sfxId, NameDB::Type::Sample), ent.m_sfxId);
  }

  for (auto& p : m_entries) {
    if (p.second->m_data->m_adpcmParmOffset) {
      r.seek(p.second->m_data->m_adpcmParmOffset, athena::SeekOrigin::Begin);
      r.readUBytesToBuf(&p.second->m_data->m_ADPCMParms, sizeof(ADPCMParms::DSPParms));
      p.second->m_data->m_ADPCMParms.swapBigDSP();
    }
  }
}

AudioGroupSampleDirectory::AudioGroupSampleDirectory(athena::io::IStreamReader& r, const unsigned char* sampData,
                                                     bool absOffs, N64DataTag) {
  if (absOffs) {
    while (!AtEnd32(r)) {
      MusyX1AbsSdirEntry<athena::Endian::Big> ent;
      ent.read(r);
      m_entries[ent.m_sfxId] = MakeObj<Entry>(ent);
      SampleId::CurNameDB->registerPair(NameDB::generateName(ent.m_sfxId, NameDB::Type::Sample), ent.m_sfxId);
    }
  } else {
    while (!AtEnd32(r)) {
      MusyX1SdirEntry<athena::Endian::Big> ent;
      ent.read(r);
      m_entries[ent.m_sfxId] = MakeObj<Entry>(ent);
      SampleId::CurNameDB->registerPair(NameDB::generateName(ent.m_sfxId, NameDB::Type::Sample), ent.m_sfxId);
    }
  }

  for (auto& p : m_entries) {
    memcpy(&p.second->m_data->m_ADPCMParms, sampData + p.second->m_data->m_sampleOff, sizeof(ADPCMParms::VADPCMParms));
    p.second->m_data->m_ADPCMParms.swapBigVADPCM();
  }
}

AudioGroupSampleDirectory::AudioGroupSampleDirectory(athena::io::IStreamReader& r, bool absOffs, PCDataTag) {
  if (absOffs) {
    while (!AtEnd32(r)) {
      MusyX1AbsSdirEntry<athena::Endian::Little> ent;
      ent.read(r);
      auto& store = m_entries[ent.m_sfxId];
      store = MakeObj<Entry>(ent);
      store->m_data->m_numSamples |= atUint32(SampleFormat::PCM_PC) << 24;
      SampleId::CurNameDB->registerPair(NameDB::generateName(ent.m_sfxId, NameDB::Type::Sample), ent.m_sfxId);
    }
  } else {
    while (!AtEnd32(r)) {
      MusyX1SdirEntry<athena::Endian::Little> ent;
      ent.read(r);
      auto& store = m_entries[ent.m_sfxId];
      store = MakeObj<Entry>(ent);
      store->m_data->m_numSamples |= atUint32(SampleFormat::PCM_PC) << 24;
      SampleId::CurNameDB->registerPair(NameDB::generateName(ent.m_sfxId, NameDB::Type::Sample), ent.m_sfxId);
    }
  }
}

AudioGroupSampleDirectory AudioGroupSampleDirectory::CreateAudioGroupSampleDirectory(const AudioGroupData& data) {
  if (data.getSdirSize() < 4)
    return {};
  athena::io::MemoryReader r(data.getSdir(), data.getSdirSize());
  switch (data.getDataFormat()) {
  case DataFormat::GCN:
  default:
    return AudioGroupSampleDirectory(r, GCNDataTag{});
  case DataFormat::N64:
    return AudioGroupSampleDirectory(r, data.getSamp(), data.getAbsoluteProjOffsets(), N64DataTag{});
  case DataFormat::PC:
    return AudioGroupSampleDirectory(r, data.getAbsoluteProjOffsets(), PCDataTag{});
  }
}

static uint32_t DSPSampleToNibble(uint32_t sample) {
  uint32_t ret = sample / 14 * 16;
  if (sample % 14)
    ret += (sample % 14) + 2;
  return ret;
}

static uint32_t DSPNibbleToSample(uint32_t nibble) {
  uint32_t ret = nibble / 16 * 14;
  if (nibble % 16)
    ret += (nibble % 16) - 2;
  return ret;
}

void AudioGroupSampleDirectory::EntryData::setLoopStartSample(atUint32 sample) {
  _setLoopStartSample(sample);

  if (m_looseData && isFormatDSP()) {
    uint32_t block = m_loopStartSample / 14;
    uint32_t rem = m_loopStartSample % 14;
    int16_t prev1 = 0;
    int16_t prev2 = 0;
    for (uint32_t b = 0; b < block; ++b)
      DSPDecompressFrameStateOnly(m_looseData.get() + 8 * b, m_ADPCMParms.dsp.m_coefs, &prev1, &prev2, 14);
    if (rem)
      DSPDecompressFrameStateOnly(m_looseData.get() + 8 * block, m_ADPCMParms.dsp.m_coefs, &prev1, &prev2, rem);
    m_ADPCMParms.dsp.m_hist1 = prev1;
    m_ADPCMParms.dsp.m_hist2 = prev2;
    m_ADPCMParms.dsp.m_lps = m_looseData[8 * block];
  }
}

void AudioGroupSampleDirectory::EntryData::loadLooseDSP(std::string_view dspPath) {
  athena::io::FileReader r(dspPath);
  if (!r.hasError()) {
    DSPADPCMHeader header;
    header.read(r);
    m_pitch = header.m_pitch;
    m_sampleRate = atUint16(header.x8_sample_rate);
    m_numSamples = header.x0_num_samples;
    if (header.xc_loop_flag) {
      _setLoopStartSample(DSPNibbleToSample(header.x10_loop_start_nibble));
      setLoopEndSample(DSPNibbleToSample(header.x14_loop_end_nibble));
    }
    m_ADPCMParms.dsp.m_ps = uint8_t(header.x3e_ps);
    m_ADPCMParms.dsp.m_lps = uint8_t(header.x44_loop_ps);
    m_ADPCMParms.dsp.m_hist1 = header.x40_hist1;
    m_ADPCMParms.dsp.m_hist2 = header.x42_hist2;
    for (int i = 0; i < 8; ++i)
      for (int j = 0; j < 2; ++j)
        m_ADPCMParms.dsp.m_coefs[i][j] = header.x1c_coef[i][j];

    uint32_t dataLen = (header.x4_num_nibbles + 1) / 2;
    m_looseData.reset(new uint8_t[dataLen]);
    r.readUBytesToBuf(m_looseData.get(), dataLen);
  }
}

void AudioGroupSampleDirectory::EntryData::loadLooseVADPCM(std::string_view vadpcmPath) {
  athena::io::FileReader r(vadpcmPath);
  if (!r.hasError()) {
    VADPCMHeader header;
    header.read(r);
    m_pitch = header.m_pitchSampleRate >> 24;
    m_sampleRate = header.m_pitchSampleRate & 0xffff;
    m_numSamples = header.m_numSamples & 0xffff;
    m_numSamples |= atUint32(SampleFormat::N64) << 24;
    m_loopStartSample = header.m_loopStartSample;
    m_loopLengthSamples = header.m_loopLengthSamples;

    uint32_t dataLen = 256 + (m_numSamples + 63) / 64 * 40;
    m_looseData.reset(new uint8_t[dataLen]);
    r.readUBytesToBuf(m_looseData.get(), dataLen);

    memcpy(&m_ADPCMParms, m_looseData.get(), 256);
    m_ADPCMParms.swapBigVADPCM();
  }
}

void AudioGroupSampleDirectory::EntryData::loadLooseWAV(std::string_view wavPath) {
  athena::io::FileReader r(wavPath);
  if (!r.hasError()) {
    atUint32 riffMagic = r.readUint32Little();
    if (riffMagic != SBIG('RIFF'))
      return;
    atUint32 wavChuckSize = r.readUint32Little();
    atUint32 wavMagic = r.readUint32Little();
    if (wavMagic != SBIG('WAVE'))
      return;

    while (r.position() < wavChuckSize + 8) {
      atUint32 chunkMagic = r.readUint32Little();
      atUint32 chunkSize = r.readUint32Little();
      atUint64 startPos = r.position();
      if (chunkMagic == SBIG('fmt ')) {
        WAVFormatChunk fmt;
        fmt.read(r);
        m_sampleRate = atUint16(fmt.sampleRate);
      } else if (chunkMagic == SBIG('smpl')) {
        WAVSampleChunk smpl;
        smpl.read(r);
        m_pitch = atUint8(smpl.midiNote);

        if (smpl.numSampleLoops) {
          WAVSampleLoop loop;
          loop.read(r);
          _setLoopStartSample(loop.start);
          setLoopEndSample(loop.end);
        }
      } else if (chunkMagic == SBIG('data')) {
        m_numSamples = ((chunkSize / 2) & 0xffffff) | (atUint32(SampleFormat::PCM_PC) << 24);
        m_looseData.reset(new uint8_t[chunkSize]);
        r.readUBytesToBuf(m_looseData.get(), chunkSize);
      }
      r.seek(startPos + chunkSize, athena::SeekOrigin::Begin);
    }
  }
}

void AudioGroupSampleDirectory::Entry::loadLooseData(std::string_view basePath) {
  std::string wavPath = std::string(basePath) + ".wav";
  std::string dspPath = std::string(basePath) + ".dsp";
  std::string vadpcmPath = std::string(basePath) + ".vadpcm";
  Sstat wavStat, dspStat, vadpcmStat;
  bool wavValid = !Stat(wavPath.c_str(), &wavStat) && S_ISREG(wavStat.st_mode);
  bool dspValid = !Stat(dspPath.c_str(), &dspStat) && S_ISREG(dspStat.st_mode);
  bool vadpcmValid = !Stat(vadpcmPath.c_str(), &vadpcmStat) && S_ISREG(vadpcmStat.st_mode);

  if (wavValid && dspValid) {
    if (wavStat.st_mtime > dspStat.st_mtime)
      dspValid = false;
    else
      wavValid = false;
  }
  if (wavValid && vadpcmValid) {
    if (wavStat.st_mtime > vadpcmStat.st_mtime)
      vadpcmValid = false;
    else
      wavValid = false;
  }
  if (dspValid && vadpcmValid) {
    if (dspStat.st_mtime > vadpcmStat.st_mtime)
      vadpcmValid = false;
    else
      dspValid = false;
  }

  EntryData& curData = *m_data;

  if (dspValid && (!curData.m_looseData || dspStat.st_mtime > curData.m_looseModTime)) {
    m_data = MakeObj<EntryData>();
    m_data->loadLooseDSP(dspPath);
    m_data->m_looseModTime = dspStat.st_mtime;
  } else if (vadpcmValid && (!curData.m_looseData || vadpcmStat.st_mtime > curData.m_looseModTime)) {
    m_data = MakeObj<EntryData>();
    m_data->loadLooseVADPCM(vadpcmPath);
    m_data->m_looseModTime = vadpcmStat.st_mtime;
  } else if (wavValid && (!curData.m_looseData || wavStat.st_mtime > curData.m_looseModTime)) {
    m_data = MakeObj<EntryData>();
    m_data->loadLooseWAV(wavPath);
    m_data->m_looseModTime = wavStat.st_mtime;
  }
}

SampleFileState AudioGroupSampleDirectory::Entry::getFileState(std::string_view basePath, std::string* pathOut) const {
  std::string wavPath = std::string(basePath) + ".wav";
  std::string dspPath = std::string(basePath) + ".dsp";
  std::string vadpcmPath = std::string(basePath) + ".vadpcm";
  Sstat wavStat, dspStat, vadpcmStat;
  bool wavValid = !Stat(wavPath.c_str(), &wavStat) && S_ISREG(wavStat.st_mode);
  bool dspValid = !Stat(dspPath.c_str(), &dspStat) && S_ISREG(dspStat.st_mode);
  bool vadpcmValid = !Stat(vadpcmPath.c_str(), &vadpcmStat) && S_ISREG(vadpcmStat.st_mode);

  EntryData& curData = *m_data;
  if (!wavValid && !dspValid && !vadpcmValid) {
    if (!curData.m_looseData)
      return SampleFileState::NoData;
    if (curData.isFormatDSP() || curData.getSampleFormat() == SampleFormat::N64)
      return SampleFileState::MemoryOnlyCompressed;
    return SampleFileState::MemoryOnlyWAV;
  }

  if (wavValid && dspValid) {
    if (wavStat.st_mtime > dspStat.st_mtime) {
      if (pathOut)
        *pathOut = wavPath;
      return SampleFileState::WAVRecent;
    }
    if (pathOut)
      *pathOut = dspPath;
    return SampleFileState::CompressedRecent;
  }
  if (wavValid && vadpcmValid) {
    if (wavStat.st_mtime > vadpcmStat.st_mtime) {
      if (pathOut)
        *pathOut = wavPath;
      return SampleFileState::WAVRecent;
    }
    if (pathOut)
      *pathOut = vadpcmPath;
    return SampleFileState::CompressedRecent;
  }
  if (dspValid && vadpcmValid) {
    if (dspStat.st_mtime > vadpcmStat.st_mtime) {
      if (pathOut)
        *pathOut = dspPath;
      return SampleFileState::CompressedNoWAV;
    }
    if (pathOut)
      *pathOut = vadpcmPath;
    return SampleFileState::CompressedNoWAV;
  }

  if (dspValid) {
    if (pathOut)
      *pathOut = dspPath;
    return SampleFileState::CompressedNoWAV;
  }
  if (vadpcmValid) {
    if (pathOut)
      *pathOut = vadpcmPath;
    return SampleFileState::CompressedNoWAV;
  }
  if (pathOut)
    *pathOut = wavPath;
  return SampleFileState::WAVNoCompressed;
}

void AudioGroupSampleDirectory::EntryData::patchMetadataDSP(std::string_view dspPath) {
  athena::io::FileReader r(dspPath);
  if (!r.hasError()) {
    DSPADPCMHeader head;
    head.read(r);

    if (isLooped()) {
      uint32_t block = getLoopStartSample() / 14;
      uint32_t rem = getLoopStartSample() % 14;
      int16_t prev1 = 0;
      int16_t prev2 = 0;
      uint8_t blockBuf[8];
      for (uint32_t b = 0; b < block; ++b) {
        r.readBytesToBuf(blockBuf, 8);
        DSPDecompressFrameStateOnly(blockBuf, head.x1c_coef, &prev1, &prev2, 14);
      }
      if (rem) {
        r.readBytesToBuf(blockBuf, 8);
        DSPDecompressFrameStateOnly(blockBuf, head.x1c_coef, &prev1, &prev2, rem);
      }
      head.xc_loop_flag = 1;
      head.x44_loop_ps = blockBuf[0];
      head.x46_loop_hist1 = prev1;
      head.x48_loop_hist2 = prev2;
    } else {
      head.xc_loop_flag = 0;
      head.x44_loop_ps = 0;
      head.x46_loop_hist1 = 0;
      head.x48_loop_hist2 = 0;
    }
    head.x10_loop_start_nibble = DSPSampleToNibble(getLoopStartSample());
    head.x14_loop_end_nibble = DSPSampleToNibble(getLoopEndSample());
    head.m_pitch = m_pitch;
    r.close();

    athena::io::FileWriter w(dspPath, false);
    if (!w.hasError()) {
      w.seek(0, athena::SeekOrigin::Begin);
      head.write(w);
    }
  }
}

void AudioGroupSampleDirectory::EntryData::patchMetadataVADPCM(std::string_view vadpcmPath) {
  athena::io::FileWriter w(vadpcmPath, false);
  if (!w.hasError()) {
    w.seek(0, athena::SeekOrigin::Begin);
    VADPCMHeader header;
    header.m_pitchSampleRate = m_pitch << 24;
    header.m_pitchSampleRate |= m_sampleRate & 0xffff;
    header.m_numSamples = m_numSamples;
    header.m_loopStartSample = m_loopStartSample;
    header.m_loopLengthSamples = m_loopLengthSamples;
    header.write(w);
  }
}

void AudioGroupSampleDirectory::EntryData::patchMetadataWAV(std::string_view wavPath) {
  athena::io::FileReader r(wavPath);
  if (!r.hasError()) {
    atUint32 riffMagic = r.readUint32Little();
    if (riffMagic == SBIG('RIFF')) {
      atUint32 wavChuckSize = r.readUint32Little();
      atUint32 wavMagic = r.readUint32Little();
      if (wavMagic == SBIG('WAVE')) {
        atInt64 smplOffset = -1;
        atInt64 loopOffset = -1;
        WAVFormatChunk fmt;
        int readSec = 0;

        while (r.position() < wavChuckSize + 8) {
          atUint32 chunkMagic = r.readUint32Little();
          atUint32 chunkSize = r.readUint32Little();
          atUint64 startPos = r.position();
          if (chunkMagic == SBIG('fmt ')) {
            fmt.read(r);
            ++readSec;
          } else if (chunkMagic == SBIG('smpl')) {
            smplOffset = startPos;
            if (chunkSize >= 60)
              loopOffset = startPos + 36;
            ++readSec;
          }
          r.seek(startPos + chunkSize, athena::SeekOrigin::Begin);
        }

        if (smplOffset == -1 || loopOffset == -1) {
          /* Complete rewrite of RIFF layout - new smpl chunk */
          r.seek(12, athena::SeekOrigin::Begin);
          athena::io::FileWriter w(wavPath);
          if (!w.hasError()) {
            w.writeUint32Little(SBIG('RIFF'));
            w.writeUint32Little(0);
            w.writeUint32Little(SBIG('WAVE'));

            bool wroteSMPL = false;
            while (r.position() < wavChuckSize + 8) {
              atUint32 chunkMagic = r.readUint32Little();
              atUint32 chunkSize = r.readUint32Little();
              if (!wroteSMPL && (chunkMagic == SBIG('smpl') || chunkMagic == SBIG('data'))) {
                wroteSMPL = true;
                w.writeUint32Little(SBIG('smpl'));
                w.writeUint32Little(60);
                WAVSampleChunk smpl;
                smpl.smplPeriod = 1000000000 / fmt.sampleRate;
                smpl.midiNote = m_pitch;
                if (isLooped()) {
                  smpl.numSampleLoops = 1;
                  smpl.additionalDataSize = 0;
                } else {
                  smpl.numSampleLoops = 0;
                  smpl.additionalDataSize = 24;
                }
                smpl.write(w);
                WAVSampleLoop loop;
                loop.start = getLoopStartSample();
                loop.end = getLoopEndSample();
                loop.write(w);
                if (chunkMagic == SBIG('smpl')) {
                  r.seek(chunkSize, athena::SeekOrigin::Current);
                  continue;
                }
              }
              w.writeUint32Little(chunkMagic);
              w.writeUint32Little(chunkSize);
              w.writeUBytes(r.readUBytes(chunkSize).get(), chunkSize);
            }

            atUint64 wavLen = w.position();
            w.seek(4, athena::SeekOrigin::Begin);
            w.writeUint32Little(wavLen - 8);
          }
          r.close();
        } else {
          /* In-place patch of RIFF layout - edit smpl chunk */
          r.close();
          athena::io::FileWriter w(wavPath, false);
          if (!w.hasError()) {
            w.seek(smplOffset, athena::SeekOrigin::Begin);
            WAVSampleChunk smpl;
            smpl.smplPeriod = 1000000000 / fmt.sampleRate;
            smpl.midiNote = m_pitch;
            if (isLooped()) {
              smpl.numSampleLoops = 1;
              smpl.additionalDataSize = 0;
            } else {
              smpl.numSampleLoops = 0;
              smpl.additionalDataSize = 24;
            }
            smpl.write(w);
            WAVSampleLoop loop;
            loop.start = getLoopStartSample();
            loop.end = getLoopEndSample();
            loop.write(w);
          }
        }
      }
    }
  }
}

/* File timestamps reflect actual audio content, not loop/pitch data */
static void SetAudioFileTime(const std::string& path, const Sstat& stat) {
#if _WIN32 && !defined(__MINGW32__)
  __utimbuf64 times = {stat.st_atime, stat.st_mtime};
  const nowide::wstackstring wpath(path);
  _wutime64(wpath.get(), &times);
#else
#if __APPLE__
  struct utimbuf times = {stat.st_atimespec, stat.st_mtimespec};
#elif __SWITCH__
  struct utimbuf times = {stat.st_atime, stat.st_mtime};
#elif __MINGW32__
  struct utimbuf times = {stat.st_atime, stat.st_mtime};
#else
  struct utimbuf times = {stat.st_atim, stat.st_mtim};
#endif
  utime(path.c_str(), &times);
#endif
}

void AudioGroupSampleDirectory::Entry::patchSampleMetadata(std::string_view basePath) const {
  std::string wavPath = std::string(basePath) + ".wav";
  std::string dspPath = std::string(basePath) + ".dsp";
  std::string vadpcmPath = std::string(basePath) + ".vadpcm";
  Sstat wavStat, dspStat, vadpcmStat;
  bool wavValid = !Stat(wavPath.c_str(), &wavStat) && S_ISREG(wavStat.st_mode);
  bool dspValid = !Stat(dspPath.c_str(), &dspStat) && S_ISREG(dspStat.st_mode);
  bool vadpcmValid = !Stat(vadpcmPath.c_str(), &vadpcmStat) && S_ISREG(vadpcmStat.st_mode);

  EntryData& curData = *m_data;

  if (wavValid) {
    curData.patchMetadataWAV(wavPath);
    SetAudioFileTime(wavPath, wavStat);
  }

  if (vadpcmValid) {
    curData.patchMetadataVADPCM(vadpcmPath);
    SetAudioFileTime(vadpcmPath, vadpcmStat);
  }

  if (dspValid) {
    curData.patchMetadataDSP(dspPath);
    SetAudioFileTime(dspPath, dspStat);
  }
}

AudioGroupSampleDirectory AudioGroupSampleDirectory::CreateAudioGroupSampleDirectory(std::string_view groupPath) {
  AudioGroupSampleDirectory ret;

  DirectoryEnumerator de(groupPath, DirectoryEnumerator::Mode::FilesSorted);
  for (const DirectoryEnumerator::Entry& ent : de) {
    if (ent.m_name.size() < 4)
      continue;
    std::string baseName;
    std::string basePath;
    if (!CompareCaseInsensitive(ent.m_name.data() + ent.m_name.size() - 4, ".dsp") ||
        !CompareCaseInsensitive(ent.m_name.data() + ent.m_name.size() - 4, ".wav")) {
      baseName = std::string(ent.m_name.begin(), ent.m_name.begin() + ent.m_name.size() - 4);
      basePath = std::string(ent.m_path.begin(), ent.m_path.begin() + ent.m_path.size() - 4);
    } else if (ent.m_name.size() > 7 &&
               !CompareCaseInsensitive(ent.m_name.data() + ent.m_name.size() - 7, ".vadpcm")) {
      baseName = std::string(ent.m_name.begin(), ent.m_name.begin() + ent.m_name.size() - 7);
      basePath = std::string(ent.m_path.begin(), ent.m_path.begin() + ent.m_path.size() - 7);
    } else
      continue;

    ObjectId sampleId = SampleId::CurNameDB->generateId(NameDB::Type::Sample);
    SampleId::CurNameDB->registerPair(baseName, sampleId);

    auto& entry = ret.m_entries[sampleId];
    entry = MakeObj<Entry>();
    entry->loadLooseData(basePath);
  }

  return ret;
}

void AudioGroupSampleDirectory::_extractWAV(SampleId id, const EntryData& ent, std::string_view destDir,
                                            const unsigned char* samp) {
  std::string path(destDir);
  path += "/";
  path += SampleId::CurNameDB->resolveNameFromId(id);
  std::string dspPath = path;
  path += ".wav";
  dspPath += ".dsp";
  athena::io::FileWriter w(path);

  SampleFormat fmt = SampleFormat(ent.m_numSamples >> 24);
  uint32_t numSamples = ent.m_numSamples & 0xffffff;
  if (ent.isLooped()) {
    WAVHeaderLoop header;
    header.fmtChunk.sampleRate = ent.m_sampleRate;
    header.fmtChunk.byteRate = ent.m_sampleRate * 2u;
    header.smplChunk.smplPeriod = 1000000000u / ent.m_sampleRate;
    header.smplChunk.midiNote = ent.m_pitch;
    header.smplChunk.numSampleLoops = 1;
    header.sampleLoop.start = ent.getLoopStartSample();
    header.sampleLoop.end = ent.getLoopEndSample();
    header.dataChunkSize = numSamples * 2u;
    header.wavChuckSize = 36 + 68 + header.dataChunkSize;
    header.write(w);
  } else {
    WAVHeader header;
    header.fmtChunk.sampleRate = ent.m_sampleRate;
    header.fmtChunk.byteRate = ent.m_sampleRate * 2u;
    header.smplChunk.smplPeriod = 1000000000u / ent.m_sampleRate;
    header.smplChunk.midiNote = ent.m_pitch;
    header.dataChunkSize = numSamples * 2u;
    header.wavChuckSize = 36 + 44 + header.dataChunkSize;
    header.write(w);
  }

  atUint64 dataLen;
  if (fmt == SampleFormat::DSP || fmt == SampleFormat::DSP_DRUM) {
    uint32_t remSamples = numSamples;
    uint32_t numFrames = (remSamples + 13) / 14;
    const unsigned char* cur = samp;
    int16_t prev1 = ent.m_ADPCMParms.dsp.m_hist1;
    int16_t prev2 = ent.m_ADPCMParms.dsp.m_hist2;
    for (uint32_t i = 0; i < numFrames; ++i) {
      int16_t decomp[14] = {};
      unsigned thisSamples = std::min(remSamples, 14u);
      DSPDecompressFrame(decomp, cur, ent.m_ADPCMParms.dsp.m_coefs, &prev1, &prev2, thisSamples);
      remSamples -= thisSamples;
      cur += 8;
      w.writeBytes(decomp, thisSamples * 2);
    }

    w.close();
    Sstat dspStat;
    if (!Stat(dspPath.c_str(), &dspStat) && S_ISREG(dspStat.st_mode))
      SetAudioFileTime(path.c_str(), dspStat);

    dataLen = (DSPSampleToNibble(numSamples) + 1) / 2;
  } else if (fmt == SampleFormat::N64) {
    uint32_t remSamples = numSamples;
    uint32_t numFrames = (remSamples + 63) / 64;
    const unsigned char* cur = samp + sizeof(ADPCMParms::VADPCMParms);
    for (uint32_t i = 0; i < numFrames; ++i) {
      int16_t decomp[64] = {};
      unsigned thisSamples = std::min(remSamples, 64u);
      N64MusyXDecompressFrame(decomp, cur, ent.m_ADPCMParms.vadpcm.m_coefs, thisSamples);
      remSamples -= thisSamples;
      cur += 40;
      w.writeBytes(decomp, thisSamples * 2);
    }

    dataLen = sizeof(ADPCMParms::VADPCMParms) + (numSamples + 63) / 64 * 40;
  } else if (fmt == SampleFormat::PCM) {
    dataLen = numSamples * 2;
    const int16_t* cur = reinterpret_cast<const int16_t*>(samp);
    for (uint32_t i = 0; i < numSamples; ++i)
      w.writeInt16Big(cur[i]);
  } else // PCM_PC
  {
    dataLen = numSamples * 2;
    w.writeBytes(samp, dataLen);
  }

  std::unique_ptr<uint8_t[]>& ld = const_cast<std::unique_ptr<uint8_t[]>&>(ent.m_looseData);
  if (!ld) {
    Sstat theStat;
    Stat(path.c_str(), &theStat);

    const_cast<time_t&>(ent.m_looseModTime) = theStat.st_mtime;
    ld.reset(new uint8_t[dataLen]);
    memcpy(ld.get(), samp, dataLen);
  }
}

void AudioGroupSampleDirectory::extractWAV(SampleId id, std::string_view destDir,
                                           const unsigned char* samp) const {
  auto search = m_entries.find(id);
  if (search == m_entries.cend())
    return;
  _extractWAV(id, *search->second->m_data, destDir, samp + search->second->m_data->m_sampleOff);
}

void AudioGroupSampleDirectory::extractAllWAV(std::string_view destDir, const unsigned char* samp) const {
  for (const auto& ent : m_entries)
    _extractWAV(ent.first, *ent.second->m_data, destDir, samp + ent.second->m_data->m_sampleOff);
}

void AudioGroupSampleDirectory::_extractCompressed(SampleId id, const EntryData& ent, std::string_view destDir,
                                                   const unsigned char* samp, bool compressWAV) {
  SampleFormat fmt = ent.getSampleFormat();
  if (!compressWAV && (fmt == SampleFormat::PCM || fmt == SampleFormat::PCM_PC)) {
    _extractWAV(id, ent, destDir, samp);
    return;
  }

  std::string path(destDir);
  path += "/";
  path += SampleId::CurNameDB->resolveNameFromId(id);

  uint32_t numSamples = ent.getNumSamples();
  atUint64 dataLen = 0;
  if (fmt == SampleFormat::DSP || fmt == SampleFormat::DSP_DRUM) {
    DSPADPCMHeader header;
    header.x0_num_samples = numSamples;
    header.x4_num_nibbles = DSPSampleToNibble(numSamples);
    header.x8_sample_rate = ent.m_sampleRate;
    header.xc_loop_flag = atUint16(ent.isLooped());
    if (header.xc_loop_flag) {
      header.x10_loop_start_nibble = DSPSampleToNibble(ent.getLoopStartSample());
      header.x14_loop_end_nibble = DSPSampleToNibble(ent.getLoopEndSample());
    }
    for (int i = 0; i < 8; ++i)
      for (int j = 0; j < 2; ++j)
        header.x1c_coef[i][j] = ent.m_ADPCMParms.dsp.m_coefs[i][j];
    header.x3e_ps = ent.m_ADPCMParms.dsp.m_ps;
    header.x40_hist1 = ent.m_ADPCMParms.dsp.m_hist1;
    header.x42_hist2 = ent.m_ADPCMParms.dsp.m_hist2;
    header.x44_loop_ps = ent.m_ADPCMParms.dsp.m_lps;
    header.m_pitch = ent.m_pitch;

    path += ".dsp";
    athena::io::FileWriter w(path);
    header.write(w);
    dataLen = (header.x4_num_nibbles + 1) / 2;
    w.writeUBytes(samp, dataLen);
  } else if (fmt == SampleFormat::N64) {
    path += ".vadpcm";
    athena::io::FileWriter w(path);
    VADPCMHeader header;
    header.m_pitchSampleRate = ent.m_pitch << 24;
    header.m_pitchSampleRate |= ent.m_sampleRate & 0xffff;
    header.m_numSamples = ent.m_numSamples;
    header.m_loopStartSample = ent.m_loopStartSample;
    header.m_loopLengthSamples = ent.m_loopLengthSamples;
    header.write(w);
    dataLen = 256 + (numSamples + 63) / 64 * 40;
    w.writeUBytes(samp, dataLen);
  } else if (fmt == SampleFormat::PCM_PC || fmt == SampleFormat::PCM) {
    const int16_t* samps = reinterpret_cast<const int16_t*>(samp);
    std::unique_ptr<int16_t[]> sampsSwapped;
    if (fmt == SampleFormat::PCM) {
      sampsSwapped.reset(new int16_t[numSamples]);
      for (uint32_t i = 0; i < numSamples; ++i)
        sampsSwapped[i] = SBig(samps[i]);
      samps = sampsSwapped.get();
    }

    int32_t loopStartSample = ent.getLoopStartSample();
    int32_t loopEndSample = ent.getLoopEndSample();

    DSPADPCMHeader header = {};
    header.x0_num_samples = numSamples;
    header.x4_num_nibbles = DSPSampleToNibble(numSamples);
    header.x8_sample_rate = ent.m_sampleRate;
    header.xc_loop_flag = atUint16(ent.isLooped());
    header.m_pitch = ent.m_pitch;
    if (header.xc_loop_flag) {
      header.x10_loop_start_nibble = DSPSampleToNibble(loopStartSample);
      header.x14_loop_end_nibble = DSPSampleToNibble(loopEndSample);
    }
    DSPCorrelateCoefs(samps, numSamples, header.x1c_coef);

    path += ".dsp";
    athena::io::FileWriter w(path);
    header.write(w);

    uint32_t remSamples = numSamples;
    uint32_t curSample = 0;
    int16_t convSamps[16] = {};
    while (remSamples) {
      uint32_t sampleCount = std::min(14u, remSamples);
      convSamps[0] = convSamps[14];
      convSamps[1] = convSamps[15];
      memcpy(convSamps + 2, samps, sampleCount * 2);
      unsigned char adpcmOut[8];
      DSPEncodeFrame(convSamps, sampleCount, adpcmOut, header.x1c_coef);
      w.writeUBytes(adpcmOut, 8);
      if (curSample == 0)
        header.x3e_ps = adpcmOut[0];
      if (header.xc_loop_flag) {
        if (loopStartSample >= int32_t(curSample) && loopStartSample < int32_t(curSample) + 14)
          header.x44_loop_ps = adpcmOut[0];
        if (loopStartSample - 1 >= int32_t(curSample) && loopStartSample - 1 < int32_t(curSample) + 14)
          header.x46_loop_hist1 = convSamps[2 + (loopStartSample - 1 - curSample)];
        if (loopStartSample - 2 >= int32_t(curSample) && loopStartSample - 2 < int32_t(curSample) + 14)
          header.x48_loop_hist2 = convSamps[2 + (loopStartSample - 2 - curSample)];
      }
      remSamples -= sampleCount;
      curSample += sampleCount;
      samps += sampleCount;
    }

    w.seek(0, athena::SeekOrigin::Begin);
    header.write(w);
  } else {
    return;
  }

  std::unique_ptr<uint8_t[]>& ld = const_cast<std::unique_ptr<uint8_t[]>&>(ent.m_looseData);
  if (!ld) {
    Sstat theStat;
    Stat(path.c_str(), &theStat);

    const_cast<time_t&>(ent.m_looseModTime) = theStat.st_mtime;
    ld.reset(new uint8_t[dataLen]);
    memcpy(ld.get(), samp, dataLen);
  }
}

void AudioGroupSampleDirectory::extractCompressed(SampleId id, std::string_view destDir,
                                                  const unsigned char* samp) const {
  auto search = m_entries.find(id);
  if (search == m_entries.cend())
    return;
  _extractCompressed(id, *search->second->m_data, destDir, samp + search->second->m_data->m_sampleOff);
}

void AudioGroupSampleDirectory::extractAllCompressed(std::string_view destDir, const unsigned char* samp) const {
  for (const auto& ent : m_entries)
    _extractCompressed(ent.first, *ent.second->m_data, destDir, samp + ent.second->m_data->m_sampleOff);
}

void AudioGroupSampleDirectory::reloadSampleData(std::string_view groupPath) {
  DirectoryEnumerator de(groupPath, DirectoryEnumerator::Mode::FilesSorted);
  for (const DirectoryEnumerator::Entry& ent : de) {
    if (ent.m_name.size() < 4)
      continue;
    std::string baseName;
    std::string basePath;
    if (!CompareCaseInsensitive(ent.m_name.data() + ent.m_name.size() - 4, ".dsp") ||
        !CompareCaseInsensitive(ent.m_name.data() + ent.m_name.size() - 4, ".wav")) {
      baseName = std::string(ent.m_name.begin(), ent.m_name.begin() + ent.m_name.size() - 4);
      basePath = std::string(ent.m_path.begin(), ent.m_path.begin() + ent.m_path.size() - 4);
    } else if (ent.m_name.size() > 7 &&
               !CompareCaseInsensitive(ent.m_name.data() + ent.m_name.size() - 7, ".vadpcm")) {
      baseName = std::string(ent.m_name.begin(), ent.m_name.begin() + ent.m_name.size() - 7);
      basePath = std::string(ent.m_path.begin(), ent.m_path.begin() + ent.m_path.size() - 7);
    } else
      continue;

    if (SampleId::CurNameDB->m_stringToId.find(baseName) == SampleId::CurNameDB->m_stringToId.end()) {
      ObjectId sampleId = SampleId::CurNameDB->generateId(NameDB::Type::Sample);
      SampleId::CurNameDB->registerPair(baseName, sampleId);

      auto& entry = m_entries[sampleId];
      entry = MakeObj<Entry>();
      entry->loadLooseData(basePath);
    }
  }
}

std::pair<std::vector<uint8_t>, std::vector<uint8_t>>
AudioGroupSampleDirectory::toGCNData(const AudioGroupDatabase& group) const {
  constexpr athena::Endian DNAE = athena::Endian::Big;

  athena::io::VectorWriter fo;
  athena::io::VectorWriter sfo;

  std::vector<std::pair<EntryDNA<DNAE>, ADPCMParms>> entries;
  entries.reserve(m_entries.size());
  size_t sampleOffset = 0;
  size_t adpcmOffset = 0;
  for (const auto& ent : SortUnorderedMap(m_entries)) {
    std::string path = group.getSampleBasePath(ent.first);
    path += ".dsp";
    SampleFileState state = group.getSampleFileState(ent.first, ent.second.get().get(), &path);
    switch (state) {
    case SampleFileState::MemoryOnlyWAV:
    case SampleFileState::MemoryOnlyCompressed:
    case SampleFileState::WAVRecent:
    case SampleFileState::WAVNoCompressed:
      group.makeCompressedVersion(ent.first, ent.second.get().get());
      break;
    default:
      break;
    }

    athena::io::FileReader r(path);
    if (!r.hasError()) {
      EntryDNA<DNAE> entryDNA = ent.second.get()->toDNA<DNAE>(ent.first);

      DSPADPCMHeader header;
      header.read(r);
      entryDNA.m_pitch = header.m_pitch;
      entryDNA.m_sampleRate = atUint16(header.x8_sample_rate);
      entryDNA.m_numSamples = header.x0_num_samples;
      if (header.xc_loop_flag) {
        entryDNA._setLoopStartSample(DSPNibbleToSample(header.x10_loop_start_nibble));
        entryDNA.setLoopEndSample(DSPNibbleToSample(header.x14_loop_end_nibble));
      }

      ADPCMParms adpcmParms;
      adpcmParms.dsp.m_bytesPerFrame = 8;
      adpcmParms.dsp.m_ps = uint8_t(header.x3e_ps);
      adpcmParms.dsp.m_lps = uint8_t(header.x44_loop_ps);
      adpcmParms.dsp.m_hist1 = header.x40_hist1;
      adpcmParms.dsp.m_hist2 = header.x42_hist2;
      for (int i = 0; i < 8; ++i)
        for (int j = 0; j < 2; ++j)
          adpcmParms.dsp.m_coefs[i][j] = header.x1c_coef[i][j];

      uint32_t dataLen = (header.x4_num_nibbles + 1) / 2;
      auto dspData = r.readUBytes(dataLen);
      sfo.writeUBytes(dspData.get(), dataLen);
      sfo.seekAlign32();

      entryDNA.m_sampleOff = sampleOffset;
      sampleOffset += ROUND_UP_32(dataLen);
      entryDNA.binarySize(adpcmOffset);
      entries.emplace_back(entryDNA, adpcmParms);
    }
  }
  adpcmOffset += 4;

  for (auto& p : entries) {
    p.first.m_adpcmParmOffset = adpcmOffset;
    p.first.write(fo);
    adpcmOffset += sizeof(ADPCMParms::DSPParms);
  }
  const uint32_t term = 0xffffffff;
  athena::io::Write<athena::io::PropType::None>::Do<decltype(term), DNAE>({}, term, fo);

  for (auto& p : entries) {
    p.second.swapBigDSP();
    fo.writeUBytes((uint8_t*)&p.second, sizeof(ADPCMParms::DSPParms));
  }

  return {fo.data(), sfo.data()};
}
} // namespace amuse
