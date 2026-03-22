#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "elf.h"

using FileContents = std::shared_ptr<std::vector<unsigned char>>;

#define ElfFileParams class Elf_Ehdr, class Elf_Phdr, class Elf_Shdr, class Elf_Addr, class Elf_Off, class Elf_Dyn, class Elf_Sym, class Elf_Versym, class Elf_Verdef, class Elf_Verdaux, class Elf_Verneed, class Elf_Vernaux, class Elf_Rel, class Elf_Rela, unsigned ElfClass
#define ElfFileParamNames Elf_Ehdr, Elf_Phdr, Elf_Shdr, Elf_Addr, Elf_Off, Elf_Dyn, Elf_Sym, Elf_Versym, Elf_Verdef, Elf_Verdaux, Elf_Verneed, Elf_Vernaux, Elf_Rel, Elf_Rela, ElfClass

template<class T>
struct span
{
    explicit span(T* d = {}, size_t l = {}) : data(d), len(l) {}
    span(T* from, T* to) : data(from), len(to-from) { assert(from <= to); }
    T& operator[](std::size_t i) { checkRange(i); return data[i]; }
    T* begin() { return data; }
    T* end() { return data + len; }
    auto size() const { return len; }
    explicit operator bool() const { return size() > 0; }

private:
    void checkRange(std::size_t i) {
        if (i >= size()) throw std::out_of_range("error: Span access out of range.");
    }

    T* data;
    size_t len;
};

template<ElfFileParams>
class ElfFile
{
public:

    const FileContents fileContents;

private:

    std::vector<Elf_Phdr> phdrs;
    std::vector<Elf_Shdr> shdrs;

    bool littleEndian;

    bool changed = false;

    bool isExecutable = false;

    using SectionName = std::string;
    using ReplacedSections = std::map<SectionName, std::string>;

    ReplacedSections replacedSections;

    std::string sectionNames; /* content of the .shstrtab section */

    /* Align on 4 or 8 bytes boundaries on 32- or 64-bit platforms
       respectively. */
    static constexpr size_t sectionAlignment = sizeof(Elf_Off);

    std::vector<SectionName> sectionsByOldIndex;

public:
    explicit ElfFile(FileContents fileContents);

    [[nodiscard]] bool isChanged() const noexcept
    {
        return changed;
    }

private:

    struct CompPhdr
    {
        const ElfFile * elfFile;
        bool operator ()(const Elf_Phdr & x, const Elf_Phdr & y) const noexcept
        {
            // A PHDR comes before everything else.
            if (elfFile->rdi(y.p_type) == PT_PHDR) return false;
            if (elfFile->rdi(x.p_type) == PT_PHDR) return true;

            // Sort non-PHDRs by address.
            return elfFile->rdi(x.p_paddr) < elfFile->rdi(y.p_paddr);
        }
    };

    void sortPhdrs();

    struct CompShdr
    {
        const ElfFile * elfFile;
        bool operator ()(const Elf_Shdr & x, const Elf_Shdr & y) const noexcept
        {
            return elfFile->rdi(x.sh_offset) < elfFile->rdi(y.sh_offset);
        }
    };

    [[nodiscard]] unsigned int getPageSize() const noexcept;

    void sortShdrs();

    void shiftFile(unsigned int extraPages, size_t sizeOffset, size_t extraBytes);

    [[nodiscard]] std::string getSectionName(const Elf_Shdr & shdr) const;

    const Elf_Shdr & findSectionHeader(const SectionName & sectionName) const;

    [[nodiscard]] std::optional<std::reference_wrapper<const Elf_Shdr>> tryFindSectionHeader(const SectionName & sectionName) const;

    template<class T> span<T> getSectionSpan(const Elf_Shdr & shdr) const;
    template<class T> span<T> getSectionSpan(const SectionName & sectionName);
    template<class T> span<T> tryGetSectionSpan(const SectionName & sectionName);

    [[nodiscard]] unsigned int getSectionIndex(const SectionName & sectionName) const;

    std::string & replaceSection(const SectionName & sectionName,
        unsigned int size);

    [[nodiscard]] bool haveReplacedSection(const SectionName & sectionName) const;

    void writeReplacedSections(Elf_Off & curOff,
        Elf_Addr startAddr, Elf_Off startOffset);

    void rewriteHeaders(Elf_Addr phdrAddress);

    void rewriteSectionsLibrary();

    void rewriteSectionsExecutable();

    void normalizeNoteSegments();

public:

    void rewriteSections(bool force = false);

    [[nodiscard]] std::string getInterpreter() const;

    typedef enum { printOsAbi, replaceOsAbi } osAbiMode;

    void modifyOsAbi(osAbiMode op, const std::string & newOsAbi);

    typedef enum { printSoname, replaceSoname } sonameMode;

    void modifySoname(sonameMode op, const std::string & newSoname);

    void setInterpreter(const std::string & newInterpreter);

    typedef enum { rpPrint, rpShrink, rpSet, rpAdd, rpRemove } RPathOp;

    void modifyRPath(RPathOp op, const std::vector<std::string> & allowedRpathPrefixes, std::string newRPath);
    std::string shrinkRPath(char* rpath, std::vector<std::string> &neededLibs, const std::vector<std::string> & allowedRpathPrefixes);
    void removeRPath(Elf_Shdr & shdrDynamic);

    void addNeeded(const std::set<std::string> & libs);

    void removeNeeded(const std::set<std::string> & libs);

    void replaceNeeded(const std::map<std::string, std::string> & libs);

    void printNeededLibs() const;

    void noDefaultLib();

    void addDebugTag();

    void renameDynamicSymbols(const std::unordered_map<std::string_view, std::string>&);

    void clearSymbolVersions(const std::set<std::string> & syms);

    enum class ExecstackMode { print, set, clear };

    void modifyExecstack(ExecstackMode op);

private:
    struct GnuHashTable {
        using BloomWord = Elf_Addr;
        struct Header {
            uint32_t numBuckets, symndx, maskwords, shift2;
        } m_hdr;
        span<BloomWord> m_bloomFilters;
        span<uint32_t> m_buckets, m_table;
    };
    GnuHashTable parseGnuHashTable(span<char> gh);

    struct HashTable {
        struct Header {
            uint32_t numBuckets, nchain;
        } m_hdr;
        span<uint32_t> m_buckets, m_chain;
    };
    HashTable parseHashTable(span<char> gh);

    void rebuildGnuHashTable(span<char> strTab, span<Elf_Sym> dynsyms);
    void rebuildHashTable(span<char> strTab, span<Elf_Sym> dynsyms);

    using Elf_Rel_Info = decltype(Elf_Rel::r_info);

    uint32_t rel_getSymId(const Elf_Rel_Info& info) const
    {
        if constexpr (std::is_same_v<Elf_Rel, Elf64_Rel>)
            return ELF64_R_SYM(info);
        else
            return ELF32_R_SYM(info);
    }

    Elf_Rel_Info rel_setSymId(Elf_Rel_Info info, uint32_t id) const
    {
        if constexpr (std::is_same_v<Elf_Rel, Elf64_Rel>)
        {
            constexpr Elf_Rel_Info idmask = (~Elf_Rel_Info()) << 32;
            info = (info & ~idmask) | (Elf_Rel_Info(id) << 32);
        }
        else
        {
            constexpr Elf_Rel_Info idmask = (~Elf_Rel_Info()) << 8;
            info = (info & ~idmask) | (Elf_Rel_Info(id) << 8);
        }
        return info;
    }

    template<class ElfRelType, class RemapFn>
    void changeRelocTableSymIds(const Elf_Shdr& shdr, RemapFn&& old2newSymId)
    {
        static_assert(std::is_same_v<ElfRelType, Elf_Rel> || std::is_same_v<ElfRelType, Elf_Rela>);

        for (auto& r : getSectionSpan<ElfRelType>(shdr))
        {
            auto info = rdi(r.r_info);
            auto oldSymIdx = rel_getSymId(info);
            auto newSymIdx = old2newSymId(oldSymIdx);
            if (newSymIdx != oldSymIdx)
                wri(r.r_info, rel_setSymId(info, newSymIdx));
        }
    }

    template<class StrIdxCallback>
    void forAllStringReferences(const Elf_Shdr& strTabHdr, StrIdxCallback&& fn);

    template<class T, class U>
    auto follow(U* ptr, size_t offset) -> T* {
        return offset ? (T*)(((char*)ptr)+offset) : nullptr;
    };

    template<class VdFn, class VaFn>
    void forAll_ElfVer(span<Elf_Verdef> vdspan, VdFn&& vdfn, VaFn&& vafn)
    {
        auto* vd = vdspan.begin();
        for (; vd; vd = follow<Elf_Verdef>(vd, rdi(vd->vd_next)))
        {
            vdfn(*vd);
            auto va = follow<Elf_Verdaux>(vd, rdi(vd->vd_aux));
            for (; va; va = follow<Elf_Verdaux>(va, rdi(va->vda_next)))
                vafn(*va);
        }
    }

    template<class VnFn, class VaFn>
    void forAll_ElfVer(span<Elf_Verneed> vnspan, VnFn&& vnfn, VaFn&& vafn)
    {
        auto* vn = vnspan.begin();
        for (; vn; vn = follow<Elf_Verneed>(vn, rdi(vn->vn_next)))
        {
            vnfn(*vn);
            auto va = follow<Elf_Vernaux>(vn, rdi(vn->vn_aux));
            for (; va; va = follow<Elf_Vernaux>(va, rdi(va->vna_next)))
                vafn(*va);
        }
    }

    /* Convert an integer in big or little endian representation (as
       specified by the ELF header) to this platform's integer
       representation. */
    template<class I>
    constexpr I rdi(I i) const noexcept;

    /* Convert back to the ELF representation. */
    template<class I, class U>
    constexpr inline I wri(I & t, U i) const
    {
        I val = static_cast<I>(i);
        if (static_cast<U>(val) != i)            
            throw std::runtime_error { "value truncation" };
        t = rdi(val);
        return val;
    }

    [[nodiscard]] Elf_Ehdr *hdr() noexcept {
      return reinterpret_cast<Elf_Ehdr *>(fileContents->data());
    }

    [[nodiscard]] const Elf_Ehdr *hdr() const noexcept {
      return reinterpret_cast<const Elf_Ehdr *>(fileContents->data());
    }
};
