// mini_elf_linker.cpp — A minimal ELF x86-64 static linker
// Build: g++ -std=c++23 -O2 -o mini_elf_linker mini_elf_linker.cpp
// Platform: Linux x86-64 only

#include <elf.h>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <format>
#include <expected>
#include <algorithm>
#include <filesystem>
#include <climits>

// ============================================================
// Utilities
// ============================================================

static uint64_t align_up(uint64_t v, uint64_t a) {
    return a ? (v + a - 1) & ~(a - 1) : v;
}

static std::expected<std::vector<uint8_t>, std::string>
read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return std::unexpected(std::format("Cannot open: {}", path));
    auto sz = f.tellg();
    f.seekg(0);
    std::vector<uint8_t> buf(static_cast<size_t>(sz));
    f.read(reinterpret_cast<char*>(buf.data()), sz);
    return buf;
}

// ============================================================
// Input Object — wraps a raw ELF .o file buffer
// ============================================================

struct InputObject {
    std::string filename;
    std::vector<uint8_t> buf;

    const Elf64_Ehdr* ehdr() const {
        return reinterpret_cast<const Elf64_Ehdr*>(buf.data());
    }
    const Elf64_Shdr* shdr(int i) const {
        if (i < 0 || i >= shnum()) return nullptr;
        return reinterpret_cast<const Elf64_Shdr*>(
            buf.data() + ehdr()->e_shoff + i * sizeof(Elf64_Shdr));
    }
    int shnum() const { return ehdr()->e_shnum; }
    const char* shstrtab() const {
        auto* sh = shdr(ehdr()->e_shstrndx);
        if (!sh) return "";
        return reinterpret_cast<const char*>(buf.data() + sh->sh_offset);
    }
    std::string_view sec_name(int i) const {
        auto* sh = shdr(i);
        if (!sh) return "";
        return shstrtab() + sh->sh_name;
    }
    const uint8_t* sec_data(int i) const {
        auto* sh = shdr(i);
        if (!sh) return nullptr;
        return buf.data() + sh->sh_offset;
    }
};

// ============================================================
// Linker-internal structures
// ============================================================

// A piece of an input section mapped into an output section
struct Chunk {
    int obj_idx;
    int shdr_idx;
    std::string name;       // e.g. ".text"
    uint64_t size;
    uint64_t align;
    uint64_t flags;
    uint32_t type;          // SHT_PROGBITS, SHT_NOBITS, etc.
    std::vector<uint8_t> data;
    uint64_t offset_in_osec = 0;  // offset within merged output section
};

// Merged output section
struct OutputSection {
    std::string name;
    uint64_t flags  = 0;
    uint64_t align  = 1;
    uint64_t vaddr  = 0;
    uint64_t foff   = 0;   // file offset
    uint64_t size   = 0;
    uint64_t filesz = 0;   // file size (may differ from size for .bss)
    bool     is_bss = false;
    std::vector<int> chunk_ids;
};

// Resolved symbol
struct SymInfo {
    std::string name;
    uint64_t value   = 0;
    int      obj_idx = -1;
    uint16_t shndx   = SHN_UNDEF;
    uint8_t  bind    = STB_LOCAL;
    uint8_t  type    = STT_NOTYPE;
    bool     resolved = false;  // true if value has been computed
};

// ============================================================
// Constants
// ============================================================

static constexpr uint64_t BASE_ADDR = 0x400000;
static constexpr uint64_t PAGE_SIZE = 0x1000;

// ============================================================
// Helper: Check if section type should be included
// ============================================================

static bool is_alloc_section_type(uint32_t type) {
    switch (type) {
        case SHT_PROGBITS:
        case SHT_NOBITS:
        case SHT_INIT_ARRAY:
        case SHT_FINI_ARRAY:
        case SHT_PREINIT_ARRAY:
            return true;
        default:
            return false;
    }
}

// ============================================================
// main
// ============================================================

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << std::format(
            "Usage: {} <output> <input1.o> [input2.o ...]\n"
            "  Minimal ELF x86-64 static linker.\n", argv[0]);
        return 1;
    }

    const std::string output_path = argv[1];

    // ----------------------------------------------------------
    // 1. Read & validate input objects
    // ----------------------------------------------------------
    std::vector<InputObject> objects;
    for (int i = 2; i < argc; ++i) {
        auto res = read_file(argv[i]);
        if (!res) { std::cerr << res.error() << "\n"; return 1; }

        InputObject obj{argv[i], std::move(*res)};
        
        // Validate minimum size
        if (obj.buf.size() < sizeof(Elf64_Ehdr)) {
            std::cerr << std::format("{}: file too small\n", argv[i]);
            return 1;
        }
        
        auto* e = obj.ehdr();
        if (std::memcmp(e->e_ident, ELFMAG, SELFMAG) != 0) {
            std::cerr << std::format("{}: not an ELF file\n", argv[i]); 
            return 1;
        }
        if (e->e_type != ET_REL) {
            std::cerr << std::format("{}: not relocatable (e_type={})\n",
                                     argv[i], e->e_type); 
            return 1;
        }
        if (e->e_machine != EM_X86_64) {
            std::cerr << std::format("{}: not x86-64\n", argv[i]); 
            return 1;
        }
        if (e->e_shstrndx >= e->e_shnum) {
            std::cerr << std::format("{}: invalid shstrndx\n", argv[i]);
            return 1;
        }
        objects.push_back(std::move(obj));
    }

    // ----------------------------------------------------------
    // 2. Collect chunks (SHF_ALLOC sections only)
    // ----------------------------------------------------------
    std::vector<Chunk> chunks;
    std::map<std::pair<int,int>, int> sec2chunk; // (obj,shidx)->chunk

    for (int oi = 0; oi < (int)objects.size(); ++oi) {
        auto& obj = objects[oi];
        for (int si = 1; si < obj.shnum(); ++si) {
            auto* sh = obj.shdr(si);
            if (!sh) continue;
            if (!(sh->sh_flags & SHF_ALLOC)) continue;
            if (!is_alloc_section_type(sh->sh_type)) continue;

            Chunk c;
            c.obj_idx  = oi;
            c.shdr_idx = si;
            c.name     = std::string(obj.sec_name(si));
            c.size     = sh->sh_size;
            c.align    = sh->sh_addralign ? sh->sh_addralign : 1;
            c.flags    = sh->sh_flags;
            c.type     = sh->sh_type;
            
            if (sh->sh_type == SHT_NOBITS) {
                // .bss - don't store data, just record size
                c.data.resize(sh->sh_size, 0);
            } else {
                auto* data = obj.sec_data(si);
                if (data) {
                    c.data.assign(data, data + sh->sh_size);
                } else {
                    c.data.resize(sh->sh_size, 0);
                }
            }

            sec2chunk[{oi, si}] = static_cast<int>(chunks.size());
            chunks.push_back(std::move(c));
        }
    }

    // ----------------------------------------------------------
    // 3. Create output sections (merge chunks by name)
    // ----------------------------------------------------------
    // Canonical order
    const std::vector<std::string> canonical = {
        ".text", ".rodata", ".data", ".data.rel.ro", ".bss"
    };
    std::map<std::string, int> osec_map;
    std::vector<OutputSection> osecs;

    auto get_or_create = [&](const std::string& name, uint64_t flags,
                             uint64_t al, bool is_bss) -> int {
        if (auto it = osec_map.find(name); it != osec_map.end()) {
            osecs[it->second].align = std::max(osecs[it->second].align, al);
            return it->second;
        }
        int id = static_cast<int>(osecs.size());
        osec_map[name] = id;
        OutputSection os;
        os.name = name;
        os.flags = flags;
        os.align = al;
        os.is_bss = is_bss;
        osecs.push_back(std::move(os));
        return id;
    };

    // Pre-create canonical sections so they appear in order
    for (auto& n : canonical) {
        // They'll only survive if at least one chunk uses them
    }

    for (auto& ch : chunks) {
        bool is_bss = (ch.type == SHT_NOBITS);
        int id = get_or_create(ch.name, ch.flags, ch.align, is_bss);
        osecs[id].chunk_ids.push_back(
            static_cast<int>(&ch - chunks.data()));
        if (is_bss) osecs[id].is_bss = true;
    }

    // Remove empty sections
    std::erase_if(osecs, [](auto& s){ return s.chunk_ids.empty(); });
    osec_map.clear();
    for (int i = 0; i < (int)osecs.size(); ++i)
        osec_map[osecs[i].name] = i;

    // Sort output sections: text-like (exec) first, then rodata, then data, then bss
    std::stable_sort(osecs.begin(), osecs.end(),
        [](auto& a, auto& b) {
            auto rank = [](auto& s) -> int {
                if (s.flags & SHF_EXECINSTR) return 0;
                if (!(s.flags & SHF_WRITE))  return 1;
                if (s.is_bss)                return 3;
                return 2;
            };
            return rank(a) < rank(b);
        });
    osec_map.clear();
    for (int i = 0; i < (int)osecs.size(); ++i)
        osec_map[osecs[i].name] = i;

    // Layout chunks within each output section
    for (auto& os : osecs) {
        uint64_t off = 0;
        for (int ci : os.chunk_ids) {
            off = align_up(off, chunks[ci].align);
            chunks[ci].offset_in_osec = off;
            off += chunks[ci].size;
        }
        os.size = off;
        // For .bss, filesz = 0; otherwise filesz = size
        os.filesz = os.is_bss ? 0 : off;
    }

    // ----------------------------------------------------------
    // 4. Assign virtual addresses & file offsets
    // ----------------------------------------------------------
    const uint64_t num_phdrs = 2;  // PT_LOAD for code+data, PT_LOAD for bss (or combined)
    const uint64_t hdr_size =
        sizeof(Elf64_Ehdr) + num_phdrs * sizeof(Elf64_Phdr);

    uint64_t file_cur = align_up(hdr_size, 16);
    uint64_t vaddr_cur = BASE_ADDR + file_cur;
    
    for (auto& os : osecs) {
        vaddr_cur = align_up(vaddr_cur, os.align);
        os.vaddr = vaddr_cur;
        
        if (!os.is_bss) {
            file_cur = align_up(file_cur, os.align);
            os.foff = file_cur;
            file_cur += os.filesz;
        } else {
            // .bss doesn't occupy file space, but we record the offset
            // where it would start (for memsz calculation)
            os.foff = file_cur;
        }
        vaddr_cur += os.size;
    }
    
    const uint64_t total_filesz = file_cur;
    const uint64_t total_memsz = vaddr_cur - BASE_ADDR;

    // ----------------------------------------------------------
    // 5. Build symbol table & resolve symbols
    // ----------------------------------------------------------
    // all_syms[oi][sym_idx] = resolved SymInfo
    std::vector<std::vector<SymInfo>> all_syms(objects.size());
    std::map<std::string, SymInfo> globals;

    for (int oi = 0; oi < (int)objects.size(); ++oi) {
        auto& obj = objects[oi];
        // find .symtab
        int stidx = -1;
        for (int si = 1; si < obj.shnum(); ++si) {
            auto* sh = obj.shdr(si);
            if (sh && sh->sh_type == SHT_SYMTAB) { stidx = si; break; }
        }
        if (stidx < 0) continue;

        auto* stsh = obj.shdr(stidx);
        if (!stsh) continue;
        
        // Validate sh_link
        if (stsh->sh_link >= (uint32_t)obj.shnum()) {
            std::cerr << std::format("{}: invalid symtab sh_link\n", obj.filename);
            return 1;
        }
        
        const char* strtab = reinterpret_cast<const char*>(
            obj.sec_data(static_cast<int>(stsh->sh_link)));
        if (!strtab) {
            std::cerr << std::format("{}: cannot read string table\n", obj.filename);
            return 1;
        }
        
        int nsym = static_cast<int>(stsh->sh_size / sizeof(Elf64_Sym));
        auto* syms = reinterpret_cast<const Elf64_Sym*>(obj.sec_data(stidx));
        if (!syms) continue;

        all_syms[oi].resize(nsym);

        for (int si = 0; si < nsym; ++si) {
            auto& s = syms[si];
            SymInfo info;
            info.name    = strtab + s.st_name;
            info.bind    = ELF64_ST_BIND(s.st_info);
            info.type    = ELF64_ST_TYPE(s.st_info);
            info.shndx   = s.st_shndx;
            info.obj_idx = oi;

            // Compute value for defined symbols
            if (s.st_shndx != SHN_UNDEF && s.st_shndx < SHN_LORESERVE) {
                auto key = std::make_pair(oi, (int)s.st_shndx);
                if (sec2chunk.contains(key)) {
                    int ci = sec2chunk[key];
                    auto it = osec_map.find(chunks[ci].name);
                    if (it != osec_map.end()) {
                        int osi = it->second;
                        info.value = osecs[osi].vaddr
                                   + chunks[ci].offset_in_osec
                                   + s.st_value;
                        info.resolved = true;
                    }
                } else {
                    // Symbol defined in non-ALLOC section
                    // This can happen for debug symbols, etc.
                    // We leave value as 0 but mark it as "defined"
                    info.value = s.st_value;
                    info.resolved = false;
                }
            } else if (s.st_shndx == SHN_ABS) {
                info.value = s.st_value;
                info.resolved = true;
            }

            all_syms[oi][si] = info;

            // Register globals with proper weak symbol handling
            if (info.bind == STB_GLOBAL || info.bind == STB_WEAK) {
                bool new_defined = (s.st_shndx != SHN_UNDEF);
                bool old_exists = globals.contains(info.name);
                bool old_defined = old_exists && globals[info.name].shndx != SHN_UNDEF;
                bool old_is_weak = old_exists && globals[info.name].bind == STB_WEAK;
                bool new_is_weak = (info.bind == STB_WEAK);
                
                if (new_defined) {
                    if (old_defined && !old_is_weak && !new_is_weak) {
                        // Both are strong symbols → error
                        std::cerr << std::format(
                            "Duplicate symbol: '{}' in '{}' and '{}'\n",
                            info.name,
                            objects[globals[info.name].obj_idx].filename,
                            obj.filename);
                        return 1;
                    }
                    // New symbol should override if:
                    // - No old symbol exists, OR
                    // - Old symbol is undefined, OR
                    // - Old symbol is weak and new is strong
                    if (!old_exists || !old_defined || (old_is_weak && !new_is_weak)) {
                        globals[info.name] = info;
                    }
                    // If both are weak, keep the first one (arbitrary but consistent)
                } else {
                    // New symbol is undefined
                    if (!old_exists) {
                        globals[info.name] = info;
                    }
                }
            }
        }
    }

    // Resolve undefined references
    for (auto& obj_syms : all_syms) {
        for (auto& si : obj_syms) {
            if (si.shndx == SHN_UNDEF && !si.name.empty()) {
                if (globals.contains(si.name) && 
                    globals[si.name].shndx != SHN_UNDEF) {
                    si.value = globals[si.name].value;
                    si.resolved = true;
                }
            }
        }
    }

    // Check unresolved (only non-weak undefined symbols are errors)
    for (auto& [name, info] : globals) {
        if (info.shndx == SHN_UNDEF && !name.empty()) {
            if (info.bind == STB_WEAK) {
                // Weak undefined symbols resolve to 0
                info.value = 0;
                info.resolved = true;
            } else {
                std::cerr << std::format("Undefined symbol: '{}'\n", name);
                return 1;
            }
        }
    }

    // Entry point
    uint64_t entry = 0;
    if (globals.contains("_start"))
        entry = globals["_start"].value;
    else
        std::cerr << "Warning: '_start' not found, entry=0x0\n";

    // ----------------------------------------------------------
    // 6. Apply relocations
    // ----------------------------------------------------------
    for (int oi = 0; oi < (int)objects.size(); ++oi) {
        auto& obj = objects[oi];
        for (int si = 1; si < obj.shnum(); ++si) {
            auto* sh = obj.shdr(si);
            if (!sh) continue;
            if (sh->sh_type != SHT_RELA) continue;

            int target_sec = static_cast<int>(sh->sh_info);
            auto key = std::make_pair(oi, target_sec);
            if (!sec2chunk.contains(key)) continue;

            int ci = sec2chunk[key];
            auto& chunk = chunks[ci];
            auto it = osec_map.find(chunk.name);
            if (it == osec_map.end()) continue;
            
            int osi = it->second;
            uint64_t chunk_vaddr = osecs[osi].vaddr + chunk.offset_in_osec;

            int nrel = static_cast<int>(sh->sh_size / sizeof(Elf64_Rela));
            auto* rels = reinterpret_cast<const Elf64_Rela*>(obj.sec_data(si));
            if (!rels) continue;

            for (int ri = 0; ri < nrel; ++ri) {
                auto& r = rels[ri];
                int sym_idx  = ELF64_R_SYM(r.r_info);
                int rel_type = ELF64_R_TYPE(r.r_info);

                // Bounds check for symbol index
                if (sym_idx < 0 || sym_idx >= (int)all_syms[oi].size()) {
                    std::cerr << std::format(
                        "{}: invalid symbol index {} in relocation\n",
                        obj.filename, sym_idx);
                    return 1;
                }

                uint64_t S = all_syms[oi][sym_idx].value;
                int64_t  A = r.r_addend;
                uint64_t P = chunk_vaddr + r.r_offset;
                uint64_t off = r.r_offset;

                switch (rel_type) {
                case R_X86_64_NONE:
                    // No relocation
                    break;
                    
                case R_X86_64_64: {
                    // Absolute 64-bit relocation: S + A
                    if (off + 8 > chunk.data.size()) {
                        std::cerr << std::format(
                            "{}: R_X86_64_64 relocation out of bounds\n",
                            obj.filename);
                        return 1;
                    }
                    uint64_t v = S + A;
                    std::memcpy(&chunk.data[off], &v, 8);
                    break;
                }
                
                case R_X86_64_PC32:
                case R_X86_64_PLT32: {
                    // PC-relative 32-bit relocation: S + A - P
                    if (off + 4 > chunk.data.size()) {
                        std::cerr << std::format(
                            "{}: PC32/PLT32 relocation out of bounds\n",
                            obj.filename);
                        return 1;
                    }
                    int64_t val = static_cast<int64_t>(S + A) -
                                  static_cast<int64_t>(P);
                    // Check for overflow (must fit in signed 32-bit)
                    if (val < INT32_MIN || val > INT32_MAX) {
                        std::cerr << std::format(
                            "{}: PC32/PLT32 relocation overflow for '{}': "
                            "distance {} out of range\n",
                            obj.filename, all_syms[oi][sym_idx].name, val);
                        return 1;
                    }
                    auto v = static_cast<int32_t>(val);
                    std::memcpy(&chunk.data[off], &v, 4);
                    break;
                }
                
                case R_X86_64_32: {
                    // Absolute 32-bit (unsigned) relocation: S + A
                    if (off + 4 > chunk.data.size()) {
                        std::cerr << std::format(
                            "{}: R_X86_64_32 relocation out of bounds\n",
                            obj.filename);
                        return 1;
                    }
                    uint64_t val = S + A;
                    // Check for overflow (must fit in unsigned 32-bit)
                    if (val > UINT32_MAX) {
                        std::cerr << std::format(
                            "{}: R_X86_64_32 relocation overflow for '{}': "
                            "value 0x{:X} out of range\n",
                            obj.filename, all_syms[oi][sym_idx].name, val);
                        return 1;
                    }
                    auto v = static_cast<uint32_t>(val);
                    std::memcpy(&chunk.data[off], &v, 4);
                    break;
                }
                
                case R_X86_64_32S: {
                    // Absolute 32-bit (signed) relocation: S + A
                    if (off + 4 > chunk.data.size()) {
                        std::cerr << std::format(
                            "{}: R_X86_64_32S relocation out of bounds\n",
                            obj.filename);
                        return 1;
                    }
                    int64_t val = static_cast<int64_t>(S + A);
                    // Check for overflow (must fit in signed 32-bit)
                    if (val < INT32_MIN || val > INT32_MAX) {
                        std::cerr << std::format(
                            "{}: R_X86_64_32S relocation overflow for '{}': "
                            "value {} out of range\n",
                            obj.filename, all_syms[oi][sym_idx].name, val);
                        return 1;
                    }
                    auto v = static_cast<int32_t>(val);
                    std::memcpy(&chunk.data[off], &v, 4);
                    break;
                }
                
                default:
                    std::cerr << std::format(
                        "{}: unsupported relocation type {}\n",
                        obj.filename, rel_type);
                    return 1;
                }
            }
        }
    }

    // ----------------------------------------------------------
    // 7. Write output ELF executable
    // ----------------------------------------------------------
    std::vector<uint8_t> out(total_filesz, 0);

    // ELF header
    Elf64_Ehdr ehdr{};
    std::memcpy(ehdr.e_ident, ELFMAG, SELFMAG);
    ehdr.e_ident[EI_CLASS]   = ELFCLASS64;
    ehdr.e_ident[EI_DATA]    = ELFDATA2LSB;
    ehdr.e_ident[EI_VERSION] = EV_CURRENT;
    ehdr.e_ident[EI_OSABI]   = ELFOSABI_NONE;
    ehdr.e_type      = ET_EXEC;
    ehdr.e_machine   = EM_X86_64;
    ehdr.e_version   = EV_CURRENT;
    ehdr.e_entry     = entry;
    ehdr.e_phoff     = sizeof(Elf64_Ehdr);
    ehdr.e_shoff     = 0;        // No section headers in output
    ehdr.e_flags     = 0;
    ehdr.e_ehsize    = sizeof(Elf64_Ehdr);
    ehdr.e_phentsize = sizeof(Elf64_Phdr);
    ehdr.e_phnum     = static_cast<uint16_t>(num_phdrs);
    ehdr.e_shentsize = sizeof(Elf64_Shdr);
    ehdr.e_shnum     = 0;
    ehdr.e_shstrndx  = SHN_UNDEF;
    std::memcpy(out.data(), &ehdr, sizeof(ehdr));

    // Program headers
    // Determine where BSS starts for separate segment
    uint64_t bss_vaddr = 0;
    uint64_t bss_memsz = 0;
    uint64_t load_filesz = total_filesz;
    uint64_t load_memsz = total_filesz;
    
    for (auto& os : osecs) {
        if (os.is_bss) {
            if (bss_vaddr == 0) bss_vaddr = os.vaddr;
            bss_memsz += os.size;
        }
    }
    
    // Adjust memsz to include BSS
    load_memsz = total_memsz;

    // PT_LOAD segment 1: everything up to BSS (code + data)
    Elf64_Phdr phdr1{};
    phdr1.p_type   = PT_LOAD;
    phdr1.p_flags  = PF_R | PF_W | PF_X;
    phdr1.p_offset = 0;
    phdr1.p_vaddr  = BASE_ADDR;
    phdr1.p_paddr  = BASE_ADDR;
    phdr1.p_filesz = total_filesz;
    phdr1.p_memsz  = load_memsz;
    phdr1.p_align  = PAGE_SIZE;
    std::memcpy(out.data() + sizeof(Elf64_Ehdr), &phdr1, sizeof(phdr1));

    // PT_GNU_STACK - mark stack as non-executable (security)
    Elf64_Phdr phdr2{};
    phdr2.p_type   = PT_GNU_STACK;
    phdr2.p_flags  = PF_R | PF_W;  // RW, not executable
    phdr2.p_offset = 0;
    phdr2.p_vaddr  = 0;
    phdr2.p_paddr  = 0;
    phdr2.p_filesz = 0;
    phdr2.p_memsz  = 0;
    phdr2.p_align  = 0;
    std::memcpy(out.data() + sizeof(Elf64_Ehdr) + sizeof(Elf64_Phdr), 
                &phdr2, sizeof(phdr2));

    // Copy section data (skip BSS since it's zero-initialized in memory)
    for (auto& os : osecs) {
        if (os.is_bss) continue;  // Don't write BSS to file
        for (int ci : os.chunk_ids) {
            auto& ch = chunks[ci];
            if (ch.type == SHT_NOBITS) continue;  // Skip .bss chunks
            std::memcpy(out.data() + os.foff + ch.offset_in_osec,
                        ch.data.data(), ch.data.size());
        }
    }

    // Write file
    {
        std::ofstream ofs(output_path, std::ios::binary);
        if (!ofs) {
            std::cerr << std::format("Cannot write: {}\n", output_path);
            return 1;
        }
        ofs.write(reinterpret_cast<const char*>(out.data()),
                  static_cast<std::streamsize>(out.size()));
    }

    // Set executable permission
    std::filesystem::permissions(output_path,
        std::filesystem::perms::owner_all |
        std::filesystem::perms::group_read |
        std::filesystem::perms::group_exec |
        std::filesystem::perms::others_read |
        std::filesystem::perms::others_exec);

    // ----------------------------------------------------------
    // 8. Print linker map
    // ----------------------------------------------------------
    std::cout << "=== Linker Map ===\n";
    std::cout << std::format("Entry: 0x{:X}\n\n", entry);
    std::cout << "Sections:\n";
    for (auto& os : osecs)
        std::cout << std::format("  {:16s}  vaddr=0x{:08X}  size={:6d}  "
                                 "filesz={:6d}  file_off=0x{:06X}{}\n",
                                 os.name, os.vaddr, os.size, os.filesz, 
                                 os.foff, os.is_bss ? " (BSS)" : "");
    std::cout << "\nGlobal Symbols:\n";
    for (auto& [n, si] : globals)
        std::cout << std::format("  {:24s} = 0x{:X}{}\n", n, si.value,
                                 si.bind == STB_WEAK ? " [weak]" : "");
    std::cout << std::format("\nOutput: {} ({} bytes, memsz {} bytes)\n", 
                             output_path, total_filesz, load_memsz);
    return 0;
}