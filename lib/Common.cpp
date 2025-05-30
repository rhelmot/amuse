#include "amuse/Common.hpp"

#if defined(_WIN32) && !defined(__MINGW32__)
#include <cstdio>
#include <memory>
#else
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif
#include <sys/utime.h>

#include <logvisor/logvisor.hpp>

using namespace std::literals;

namespace amuse {
static logvisor::Module Log("amuse");

bool Copy(const char* from, const char* to) {
#if _WIN32 && !defined(__MINGW32__)
  const nowide::wstackstring wfrom(from);
  const nowide::wstackstring wto(to);
  return CopyFileW(wfrom.get(), wto.get(), FALSE) != 0;
#else
  FILE* fi = fopen(from, "rb");
  if (fi == nullptr)
    return false;
  FILE* fo = fopen(to, "wb");
  if (fo == nullptr) {
    fclose(fi);
    return false;
  }
  std::unique_ptr<uint8_t[]> buf(new uint8_t[65536]);
  size_t readSz = 0;
  while ((readSz = fread(buf.get(), 1, 65536, fi)))
    fwrite(buf.get(), 1, readSz, fo);
  fclose(fi);
  fclose(fo);
  struct stat theStat{};
  if (::stat(from, &theStat))
    return true;
#if __APPLE__
  struct utimbuf times = {theStat.st_atimespec, theStat.st_mtimespec};
#elif __SWITCH__
  struct utimbuf times = {theStat.st_atime, theStat.st_mtime};
#elif __MINGW32__
  struct utimbuf times = {theStat.st_atime, theStat.st_mtime};
#else
  struct utimbuf times = {theStat.st_atim, theStat.st_mtim};
#endif
  utime(to, &times);
  return true;
#endif
}

#if _WIN32 && !defined(__MINGW32__)
int Rename(const char* oldpath, const char* newpath) {
  const nowide::wstackstring woldpath(oldpath);
  const nowide::wstackstring wnewpath(newpath);
  return MoveFileExW(woldpath.get(), wnewpath.get(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == 0;
}
#endif

#define DEFINE_ID_TYPE(type, typeName)                                                                                 \
  thread_local NameDB* type::CurNameDB = nullptr;                                                                      \
  template <>                                                                                                          \
  template <>                                                                                                          \
  void type##DNA<athena::Endian::Little>::Enumerate<BigDNA::Read>(athena::io::IStreamReader& reader) {                 \
    id = reader.readUint16Little();                                                                                    \
  }                                                                                                                    \
  template <>                                                                                                          \
  template <>                                                                                                          \
  void type##DNA<athena::Endian::Little>::Enumerate<BigDNA::Write>(athena::io::IStreamWriter& writer) {                \
    writer.writeUint16Little(id.id);                                                                                   \
  }                                                                                                                    \
  template <>                                                                                                          \
  template <>                                                                                                          \
  void type##DNA<athena::Endian::Little>::Enumerate<BigDNA::BinarySize>(size_t& sz) {                                  \
    sz += 2;                                                                                                           \
  }                                                                                                                    \
  template <>                                                                                                          \
  template <>                                                                                                          \
  void type##DNA<athena::Endian::Little>::Enumerate<BigDNA::ReadYaml>(athena::io::YAMLDocReader& reader) {             \
    _read(reader);                                                                                                     \
  }                                                                                                                    \
  template <>                                                                                                          \
  template <>                                                                                                          \
  void type##DNA<athena::Endian::Little>::Enumerate<BigDNA::WriteYaml>(athena::io::YAMLDocWriter& writer) {            \
    _write(writer);                                                                                                    \
  }                                                                                                                    \
  template <>                                                                                                          \
  template <>                                                                                                          \
  void type##DNA<athena::Endian::Big>::Enumerate<BigDNA::Read>(athena::io::IStreamReader& reader) {                    \
    id = reader.readUint16Big();                                                                                       \
  }                                                                                                                    \
  template <>                                                                                                          \
  template <>                                                                                                          \
  void type##DNA<athena::Endian::Big>::Enumerate<BigDNA::Write>(athena::io::IStreamWriter& writer) {                   \
    writer.writeUint16Big(id.id);                                                                                      \
  }                                                                                                                    \
  template <>                                                                                                          \
  template <>                                                                                                          \
  void type##DNA<athena::Endian::Big>::Enumerate<BigDNA::BinarySize>(size_t& sz) {                                     \
    sz += 2;                                                                                                           \
  }                                                                                                                    \
  template <>                                                                                                          \
  template <>                                                                                                          \
  void type##DNA<athena::Endian::Big>::Enumerate<BigDNA::ReadYaml>(athena::io::YAMLDocReader& reader) {                \
    _read(reader);                                                                                                     \
  }                                                                                                                    \
  template <>                                                                                                          \
  template <>                                                                                                          \
  void type##DNA<athena::Endian::Big>::Enumerate<BigDNA::WriteYaml>(athena::io::YAMLDocWriter& writer) {               \
    _write(writer);                                                                                                    \
  }                                                                                                                    \
  template <athena::Endian DNAE>                                                                                       \
  void type##DNA<DNAE>::_read(athena::io::YAMLDocReader& r) {                                                          \
    std::string name = r.readString();                                                                                 \
    if (!type::CurNameDB)                                                                                              \
      Log.report(logvisor::Fatal, FMT_STRING("Unable to resolve " typeName " name {}, no database present"), name);           \
    if (name.empty()) {                                                                                                \
      id.id = 0xffff;                                                                                                  \
      return;                                                                                                          \
    }                                                                                                                  \
    id = type::CurNameDB->resolveIdFromName(name);                                                                     \
  }                                                                                                                    \
  template <athena::Endian DNAE>                                                                                       \
  void type##DNA<DNAE>::_write(athena::io::YAMLDocWriter& w) {                                                         \
    if (!type::CurNameDB)                                                                                              \
      Log.report(logvisor::Fatal, FMT_STRING("Unable to resolve " typeName " ID {}, no database present"), id);               \
    if (id.id == 0xffff)                                                                                               \
      return;                                                                                                          \
    std::string_view name = type::CurNameDB->resolveNameFromId(id);                                                    \
    if (!name.empty())                                                                                                 \
      w.writeString(name);                                                                                             \
  }                                                                                                                    \
  template <athena::Endian DNAE>                                                                                       \
  std::string_view type##DNA<DNAE>::DNAType() {                                                                        \
    return "amuse::" #type "DNA"sv;                                                                                    \
  }                                                                                                                    \
  template struct type##DNA<athena::Endian::Big>;                                                                      \
  template struct type##DNA<athena::Endian::Little>;

DEFINE_ID_TYPE(ObjectId, "object")
DEFINE_ID_TYPE(SoundMacroId, "SoundMacro")
DEFINE_ID_TYPE(SampleId, "sample")
DEFINE_ID_TYPE(TableId, "table")
DEFINE_ID_TYPE(KeymapId, "keymap")
DEFINE_ID_TYPE(LayersId, "layers")
DEFINE_ID_TYPE(SongId, "song")
DEFINE_ID_TYPE(SFXId, "sfx")
DEFINE_ID_TYPE(GroupId, "group")

template <>
template <>
void PageObjectIdDNA<athena::Endian::Little>::Enumerate<BigDNA::Read>(athena::io::IStreamReader& reader) {
  id = reader.readUint16Little();
}
template <>
template <>
void PageObjectIdDNA<athena::Endian::Little>::Enumerate<BigDNA::Write>(athena::io::IStreamWriter& writer) {
  writer.writeUint16Little(id.id);
}
template <>
template <>
void PageObjectIdDNA<athena::Endian::Little>::Enumerate<BigDNA::BinarySize>(size_t& sz) {
  sz += 2;
}
template <>
template <>
void PageObjectIdDNA<athena::Endian::Little>::Enumerate<BigDNA::ReadYaml>(athena::io::YAMLDocReader& reader) {
  _read(reader);
}
template <>
template <>
void PageObjectIdDNA<athena::Endian::Little>::Enumerate<BigDNA::WriteYaml>(athena::io::YAMLDocWriter& writer) {
  _write(writer);
}
template <>
template <>
void PageObjectIdDNA<athena::Endian::Big>::Enumerate<BigDNA::Read>(athena::io::IStreamReader& reader) {
  id = reader.readUint16Big();
}
template <>
template <>
void PageObjectIdDNA<athena::Endian::Big>::Enumerate<BigDNA::Write>(athena::io::IStreamWriter& writer) {
  writer.writeUint16Big(id.id);
}
template <>
template <>
void PageObjectIdDNA<athena::Endian::Big>::Enumerate<BigDNA::BinarySize>(size_t& sz) {
  sz += 2;
}
template <>
template <>
void PageObjectIdDNA<athena::Endian::Big>::Enumerate<BigDNA::ReadYaml>(athena::io::YAMLDocReader& reader) {
  _read(reader);
}
template <>
template <>
void PageObjectIdDNA<athena::Endian::Big>::Enumerate<BigDNA::WriteYaml>(athena::io::YAMLDocWriter& writer) {
  _write(writer);
}
template <athena::Endian DNAE>
void PageObjectIdDNA<DNAE>::_read(athena::io::YAMLDocReader& r) {
  std::string name = r.readString();
  if (!KeymapId::CurNameDB || !LayersId::CurNameDB)
    Log.report(logvisor::Fatal, FMT_STRING("Unable to resolve keymap or layers name {}, no database present"), name);
  if (name.empty()) {
    id.id = 0xffff;
    return;
  }
  auto search = KeymapId::CurNameDB->m_stringToId.find(name);
  if (search == KeymapId::CurNameDB->m_stringToId.cend()) {
    search = LayersId::CurNameDB->m_stringToId.find(name);
    if (search == LayersId::CurNameDB->m_stringToId.cend()) {
      search = SoundMacroId::CurNameDB->m_stringToId.find(name);
      if (search == SoundMacroId::CurNameDB->m_stringToId.cend()) {
        Log.report(logvisor::Error, FMT_STRING("Unable to resolve name {}"), name);
        id.id = 0xffff;
        return;
      }
    }
  }
  id = search->second;
}
template <athena::Endian DNAE>
void PageObjectIdDNA<DNAE>::_write(athena::io::YAMLDocWriter& w) {
  if (!KeymapId::CurNameDB || !LayersId::CurNameDB)
    Log.report(logvisor::Fatal, FMT_STRING("Unable to resolve keymap or layers ID {}, no database present"), id);
  if (id.id == 0xffff)
    return;
  if (id.id & 0x8000) {
    std::string_view name = LayersId::CurNameDB->resolveNameFromId(id);
    if (!name.empty())
      w.writeString(name);
  } else if (id.id & 0x4000) {
    std::string_view name = KeymapId::CurNameDB->resolveNameFromId(id);
    if (!name.empty())
      w.writeString(name);
  } else {
    std::string_view name = SoundMacroId::CurNameDB->resolveNameFromId(id);
    if (!name.empty())
      w.writeString(name);
  }
}
template <athena::Endian DNAE>
std::string_view PageObjectIdDNA<DNAE>::DNAType() {
  return "amuse::PageObjectIdDNA"sv;
}
template struct PageObjectIdDNA<athena::Endian::Big>;
template struct PageObjectIdDNA<athena::Endian::Little>;

template <>
template <>
void SoundMacroStepDNA<athena::Endian::Little>::Enumerate<BigDNA::Read>(athena::io::IStreamReader& reader) {
  step = reader.readUint16Little();
}
template <>
template <>
void SoundMacroStepDNA<athena::Endian::Little>::Enumerate<BigDNA::Write>(athena::io::IStreamWriter& writer) {
  writer.writeUint16Little(step);
}
template <>
template <>
void SoundMacroStepDNA<athena::Endian::Little>::Enumerate<BigDNA::BinarySize>(size_t& sz) {
  sz += 2;
}
template <>
template <>
void SoundMacroStepDNA<athena::Endian::Little>::Enumerate<BigDNA::ReadYaml>(athena::io::YAMLDocReader& reader) {
  step = reader.readUint16();
}
template <>
template <>
void SoundMacroStepDNA<athena::Endian::Little>::Enumerate<BigDNA::WriteYaml>(athena::io::YAMLDocWriter& writer) {
  writer.writeUint16(step);
}
template <>
template <>
void SoundMacroStepDNA<athena::Endian::Big>::Enumerate<BigDNA::Read>(athena::io::IStreamReader& reader) {
  step = reader.readUint16Big();
}
template <>
template <>
void SoundMacroStepDNA<athena::Endian::Big>::Enumerate<BigDNA::Write>(athena::io::IStreamWriter& writer) {
  writer.writeUint16Big(step);
}
template <>
template <>
void SoundMacroStepDNA<athena::Endian::Big>::Enumerate<BigDNA::BinarySize>(size_t& sz) {
  sz += 2;
}
template <>
template <>
void SoundMacroStepDNA<athena::Endian::Big>::Enumerate<BigDNA::ReadYaml>(athena::io::YAMLDocReader& reader) {
  step = reader.readUint16();
}
template <>
template <>
void SoundMacroStepDNA<athena::Endian::Big>::Enumerate<BigDNA::WriteYaml>(athena::io::YAMLDocWriter& writer) {
  writer.writeUint16(step);
}
template <athena::Endian DNAE>
std::string_view SoundMacroStepDNA<DNAE>::DNAType() {
  return "amuse::SoundMacroStepDNA"sv;
}
template struct SoundMacroStepDNA<athena::Endian::Big>;
template struct SoundMacroStepDNA<athena::Endian::Little>;

ObjectId NameDB::generateId(Type tp) const {
  uint16_t maxMatch = 0;
  if (tp == Type::Layer) {
    maxMatch = 0x8000;
  } else if (tp == Type::Keymap) {
    maxMatch = 0x4000;
  }
  for (const auto& p : m_idToString) {
    if (p.first.id >= maxMatch) {
      maxMatch = p.first.id + 1;
    }
  }
  return maxMatch;
}

std::string NameDB::generateName(ObjectId id, Type tp) {
  switch (tp) {
  case Type::SoundMacro:
    return fmt::format(FMT_STRING("macro{}"), id);
  case Type::Table:
    return fmt::format(FMT_STRING("table{}"), id);
  case Type::Keymap:
    return fmt::format(FMT_STRING("keymap{}"), id);
  case Type::Layer:
    return fmt::format(FMT_STRING("layers{}"), id);
  case Type::Song:
    return fmt::format(FMT_STRING("song{}"), id);
  case Type::SFX:
    return fmt::format(FMT_STRING("sfx{}"), id);
  case Type::Group:
    return fmt::format(FMT_STRING("group{}"), id);
  case Type::Sample:
    return fmt::format(FMT_STRING("sample{}"), id);
  default:
    return fmt::format(FMT_STRING("obj{}"), id);
  }
}

std::string NameDB::generateDefaultName(Type tp) const { return generateName(generateId(tp), tp); }

std::string_view NameDB::registerPair(std::string_view str, ObjectId id) {
  auto string = std::string(str);
  m_stringToId.insert_or_assign(string, id);
  return m_idToString.emplace(id, std::move(string)).first->second;
}

std::string_view NameDB::resolveNameFromId(ObjectId id) const {
  auto search = m_idToString.find(id);
  if (search == m_idToString.cend()) {
    Log.report(logvisor::Error, FMT_STRING("Unable to resolve ID {}"), id);
    return ""sv;
  }
  return search->second;
}

ObjectId NameDB::resolveIdFromName(std::string_view str) const {
  auto search = m_stringToId.find(std::string(str));
  if (search == m_stringToId.cend()) {
    Log.report(logvisor::Error, FMT_STRING("Unable to resolve name {}"), str);
    return {};
  }
  return search->second;
}

void NameDB::remove(ObjectId id) {
  auto search = m_idToString.find(id);
  if (search == m_idToString.cend())
    return;
  auto search2 = m_stringToId.find(search->second);
  if (search2 == m_stringToId.cend())
    return;
  m_idToString.erase(search);
  m_stringToId.erase(search2);
}

void NameDB::rename(ObjectId id, std::string_view str) {
  auto search = m_idToString.find(id);
  if (search == m_idToString.cend())
    return;
  if (search->second == str)
    return;
  auto search2 = m_stringToId.find(search->second);
  if (search2 == m_stringToId.cend())
    return;
#if __APPLE__
  std::swap(m_stringToId[std::string(str)], search2->second);
  m_stringToId.erase(search2);
#else
  auto nh = m_stringToId.extract(search2);
  nh.key() = str;
  m_stringToId.insert(std::move(nh));
#endif
  search->second = str;
}

template <>
void LittleUInt24::Enumerate<LittleDNA::Read>(athena::io::IStreamReader& reader) {
  union {
    atUint32 val;
    char bytes[4];
  } data = {};
  reader.readBytesToBuf(data.bytes, 3);
  val = SLittle(data.val);
}

template <>
void LittleUInt24::Enumerate<LittleDNA::Write>(athena::io::IStreamWriter& writer) {
  union {
    atUint32 val;
    char bytes[4];
  } data;
  data.val = SLittle(val);
  writer.writeBytes(data.bytes, 3);
}

template <>
void LittleUInt24::Enumerate<LittleDNA::BinarySize>(size_t& sz) {
  sz += 3;
}

template <>
void LittleUInt24::Enumerate<LittleDNA::ReadYaml>(athena::io::YAMLDocReader& reader) {
  val = reader.readUint32();
}

template <>
void LittleUInt24::Enumerate<LittleDNA::WriteYaml>(athena::io::YAMLDocWriter& writer) {
  writer.writeUint32(val);
}

std::string_view LittleUInt24::DNAType() { return "amuse::LittleUInt24"sv; }

} // namespace amuse
