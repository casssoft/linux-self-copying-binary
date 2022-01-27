#include <elf.h>


#include <elf.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#if defined(__LP64__)
#define ElfW(type) Elf64_ ## type
#else
#define ElfW(type) Elf32_ ## type
#endif


typedef struct {
  unsigned char prefix[8];
  int prefixLength;
  unsigned char matchmask[8];
  int used;
} ASMWhiteList;

#define MAX_WHITELIST 50
#define PRE(X, Y) pattern->prefix[X] = Y
#define MATCHMASK_BLANK \
  pattern->matchmask[0] = 0xff; \
  pattern->matchmask[1] = 0xff; \
  pattern->matchmask[2] = 0xff; \
  pattern->matchmask[3] = 0xff; \
  pattern->matchmask[4] = 0xff; \
  pattern->matchmask[5] = 0xff; \
  pattern->matchmask[6] = 0xff; \
  pattern->matchmask[7] = 0xff

ASMWhiteList RefASM[MAX_WHITELIST];


void fillOutWhiteList() {
  for (int i = 0; i < MAX_WHITELIST; ++i) {
    RefASM[i].used = 0;
  }

  ASMWhiteList* pattern = &RefASM[0];
  {
    PRE(0, 0x48);
    PRE(1, 0xc7);
    PRE(2, 0x44);
    PRE(3, 0x00);
    PRE(4, 0x00);
    // 2 extra bytes to blank out
    pattern->prefixLength = 5;
    MATCHMASK_BLANK;
    pattern->matchmask[3] = 0x00;
    pattern->matchmask[4] = 0x00;
    pattern->used = 1;
  }
  pattern = &RefASM[1];
  {
    PRE(0, 0x41);
    PRE(1, 0xbe);
    pattern->prefixLength = 2;
    MATCHMASK_BLANK;
    pattern->used = 1;
  }
  pattern = &RefASM[2]; // this will match with everything...
  {
    PRE(0, 0xb8);
    pattern->prefixLength = 1;
    MATCHMASK_BLANK;
    pattern->matchmask[0] = 0xf8;
    pattern->used = 1;
  }
  pattern = &RefASM[3];
  {
    //ff 24 c5 
    PRE(0, 0xff);
    PRE(1, 0x24);
    PRE(2, 0xc5);
    pattern->prefixLength = 3;
    MATCHMASK_BLANK;
    pattern->used = 1;
  }
  pattern = &RefASM[4];
  {
    //ff 24 cd
    // Hmm i dont know how the above combines with this guy...
    PRE(0, 0xff);
    PRE(1, 0x24);
    PRE(2, 0xcd);
    pattern->prefixLength = 3;
    MATCHMASK_BLANK;
    pattern->used = 1;
  }

  pattern = &RefASM[5];
  {
  // 4a 8b 04 f5
    // Hmm i dont know how the above combines with this guy...
    PRE(0, 0x4a);
    PRE(1, 0x8b);
    PRE(2, 0x04);
    PRE(3, 0xf5);
    pattern->prefixLength = 4;
    MATCHMASK_BLANK;
    pattern->used = 1;
  }
  pattern = &RefASM[6];
  {
    //c5 f8 10 80
    PRE(0, 0xc5);
    PRE(1, 0xf8);
    PRE(2, 0x10);
    PRE(3, 0x80);
    pattern->prefixLength = 4;
    MATCHMASK_BLANK;
    pattern->used = 1;
  }

};

void replacePHDRHeaderInZigWithOurOwn(const char* elfFile, const char* newfile) {

  FILE* f = fopen(elfFile, "rb");

  if(f) {
    // read the whole file into memory----------------
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);  /* same as rewind(f); */

    unsigned char *filecontents = malloc(fsize + 1);
    fread(filecontents, fsize, 1, f);
    fclose(f);

    filecontents[fsize] = 0;

    // FIND DEADBEEF and replace with file size
    // but it's lil endian so find efbeadde
    // might not be aligned even right?
    int foundSize = 0;
    for (int i = 0; i < fsize - 4; ++i) {
      if (
          filecontents[i] == 0xef &&
          filecontents[i+ 1] == 0xbe &&
          filecontents[i + 2] == 0xad &&
          filecontents[i + 3] == 0xde) {
        // WOoo we found it hopefully its the right place
          *((uint64_t*)(&filecontents[i])) = fsize;
          foundSize = 1;
          printf("FOUND size at %d\n", i);
          //break;
      }
    }
    if (!foundSize) {
      fprintf(stderr, "Didnt find a deadbeef to replace...\n");
      exit(1);
    }

    // read the header--------------------
    //
    // Either Elf64_Ehdr or Elf32_Ehdr depending on architecture.
    ElfW(Ehdr) header;
    memcpy(&header, filecontents, sizeof(header));

    // check so its really an elf file
    if (memcmp(header.e_ident, ELFMAG, SELFMAG) == 0) {
      // this is a valid elf file
      printf("yay valid\n");
    }
    // HMM OK SO WE NEED TO ADD ANOTHER PROGRAM HEADER! which means rewriting all the offsetsssss

    // OK so after that index it's all crap and probably some before that...
    // Ok lets add another program header
    int originalHeaders = header.e_phnum;
    int newProgramHeaders = 1;//2;
    header.e_phnum += newProgramHeaders;

    // in zig the entrypoint is right thar
    header.e_entry += header.e_phentsize * newProgramHeaders;

    // Ok so first delete all section headers cause fuck em

    // get section header offset so we can blow it away
    long sectionHeaderOffset = header.e_shoff;
    header.e_shoff += header.e_phentsize * newProgramHeaders;

    //header.e_shentsize = 0;
    //header.e_shnum = 0;
    //header.e_shstrndx = 0;



    // write back the e header!!!!!!!
    memcpy(filecontents, &header, sizeof(header));



    // now we need to insert some bytes my dear friend.....
    // TO insert bytes lets just write some yknow??

    long originalExecutableSectionEndVirtual;
    long originalExecutableSectionBegin;
    long originalExecutableSectionEnd;

    // ok lets read the program headers and fix em
    for (int i = 0; i < originalHeaders; ++i) {
      ElfW(Phdr) programHeader;
      memcpy(&programHeader, filecontents + header.e_phoff + header.e_phentsize * i, sizeof(programHeader));
      // ok lets fix this effer...
      //
      if (programHeader.p_type == PT_LOAD && programHeader.p_offset == 0 && programHeader.p_flags == PF_R) { // OK THIS IS THE SPECIAL guy...
        //start of original data
        // Remove the start will work but we dont need to!
        programHeader.p_offset = 0;
        programHeader.p_filesz += newProgramHeaders * header.e_phentsize;
        programHeader.p_memsz += newProgramHeaders * header.e_phentsize;
      } else if (programHeader.p_type == PT_LOAD && programHeader.p_flags == (PF_R | PF_X)) { // executable section!
        originalExecutableSectionBegin = programHeader.p_offset;
        originalExecutableSectionEnd = programHeader.p_offset + programHeader.p_filesz;
        originalExecutableSectionEndVirtual = programHeader.p_vaddr + programHeader.p_memsz;

        programHeader.p_offset += newProgramHeaders * header.e_phentsize;
        programHeader.p_vaddr += newProgramHeaders * header.e_phentsize;
        programHeader.p_paddr += newProgramHeaders * header.e_phentsize;
        // Ok if they ar
      } else if (programHeader.p_type == PT_LOAD && programHeader.p_flags == (PF_R | PF_W)) { // bss section
        programHeader.p_offset += newProgramHeaders * header.e_phentsize;
        programHeader.p_vaddr += newProgramHeaders * header.e_phentsize;
        programHeader.p_paddr += newProgramHeaders * header.e_phentsize;
      }


      memcpy(filecontents + header.e_phoff + header.e_phentsize * i, &programHeader, sizeof(programHeader));
    }

    //long roOnly, roOnlySize, textSection, textSectionSize;

    // Fix all the section headers too cause why not
    for (int i = 0; i < header.e_shnum; ++i) {
      ElfW(Shdr) sectionHeader;
      memcpy(&sectionHeader, filecontents + sectionHeaderOffset + header.e_shentsize * i, sizeof(sectionHeader));
      // ok lets fix this too
      if (sectionHeader.sh_addr != 0) {// && sectionHeader.sh_addr != 0x0000000000203000) {
        sectionHeader.sh_addr += newProgramHeaders * header.e_phentsize;
      }
      if (sectionHeader.sh_offset != 0) {// && sectionHeader.sh_offset != 0x1000) {
        sectionHeader.sh_offset += newProgramHeaders * header.e_phentsize;
      }
      fprintf(stderr, "Section header: %d, new addr: %x new offset: %x\n", i, sectionHeader.sh_addr, sectionHeader.sh_offset);

      memcpy(filecontents + sectionHeaderOffset + header.e_shentsize * i, &sectionHeader, sizeof(sectionHeader));
    }
    // Weeeeeeeeeeeee now let's fix up any references :/
    // everything between the end of program headers and the beginning of section headers is either:
    // data, code, string table... So lets do it
    // And sadly the references in code are only aligned to the byte.... :<
    // lets hope this dont fuck shizzz up LOL
    fillOutWhiteList();
    // First fix up data section: (this should be aligned.... TODO)
    for (int i = header.e_phoff + header.e_phentsize * originalHeaders; i < originalExecutableSectionBegin - 4; ++i) {
      if (
          filecontents[i + 2] == 0x20 &&
          filecontents[i + 3] == 0x00) {
        //TODO this might fuck up alignment issues??? well whatevers
        uint32_t value = *(uint32_t*)&filecontents[i];
        // Lets see if we need to update it lol
        if (value - 0x200000 >= header.e_phoff + header.e_phentsize * originalHeaders) {//  && value < originalExecutableSectionEndVirtual) {
          printf("Changing Data pointer value: originally: %x at %x\n", value, i);
          value += header.e_phentsize * newProgramHeaders;
          *(uint32_t*)&filecontents[i] = value;
        } else {
          //printf("%x < %x or %x > %x\n", value - 0x200000, header.e_phoff + header.e_phentsize * originalHeaders, value, originalExecutableSectionEndVirtual);
        }
      }
    }

    printf("Scanning %x to %x\n in text section size: %x\n", originalExecutableSectionBegin, originalExecutableSectionEnd, originalExecutableSectionEnd - originalExecutableSectionBegin);
    // now try to fix up text section oh man..
    for (int i = originalExecutableSectionBegin; i < originalExecutableSectionEnd - 4; ++i) {
      if (
          filecontents[i + 2] == 0x20 &&
          filecontents[i + 3] == 0x00) {
        // OK lets check the value first...
        int replaced = 0;
        uint32_t value = *(uint32_t*)&filecontents[i];
        if (value - 0x200000 >= header.e_phoff + header.e_phentsize * originalHeaders) {// && value < originalExecutableSectionEndVirtual) {
          /// ok lets check the asm whitelist
          for (int a = 0; a < MAX_WHITELIST && RefASM[a].used; ++a) {
            ASMWhiteList* pattern = &RefASM[a];
            int matches = 1;
            for (int b = 0; b < pattern->prefixLength; ++b) {
              //if (a == 2) {
              //  printf("checking against be pattern: %x match: %x \n", pattern->prefix[b], filecontents[i - pattern->prefixLength + b] & pattern->matchmask[b]);
              //}
              if (!(pattern->prefix[b] == (filecontents[i - pattern->prefixLength + b] & pattern->matchmask[b]))) {
                matches = 0;
                break;
              }
            }
            if (matches) {
                printf("Changing text pointer value: originally: %x at %x\n", value, i);
                value += header.e_phentsize * newProgramHeaders;
                *(uint32_t*)&filecontents[i] = value;
                replaced = 1;
                break;
            }
          }
        }
        if (!replaced) {
          printf("text pointer value left alone: originally: %x at %x\n", value, i);
        }
      }
    }

    // OK now we are ready to add our broooo at the end!
    ElfW(Phdr) newHeader;
    newHeader.p_type = PT_LOAD;
    newHeader.p_offset = 0; // hehe the whole damn file bro
    newHeader.p_vaddr = 0x300000; // I hope this memory isn't taken sir...
    newHeader.p_paddr = newHeader.p_vaddr; // not sure what this should be set for. on BSD it needs to be 0
    long newFileSize = fsize + newProgramHeaders * header.e_phentsize;
    newHeader.p_filesz = newFileSize;
    newHeader.p_memsz = newFileSize;
    newHeader.p_flags = PF_R; // read only
    newHeader.p_align = 0x1000; // alignment value, no idea why we need this, but might as well set it the same as zig

    // OK NOW WRITE THE FILE WITH OUR NEW HEADER

    FILE*output = fopen(newfile, "w");
    if (!output) {
      fprintf(stderr, "failed to open new file...\n");
      exit(1);
    }

    long beforeNewHeader = header.e_phoff + header.e_phentsize * originalHeaders;
    fwrite(filecontents, beforeNewHeader, 1, output);
    // Write new header
    fwrite(&newHeader, sizeof(newHeader), 1, output);
    // Write rest of shit
    fwrite(filecontents + beforeNewHeader, fsize - beforeNewHeader, 1, output);



    printf("wrote to output\n");

  } else {
    fprintf(stderr, "failed to read file\n");
    exit(1);
  }
}

int main(int argc, char**argv) {
  if (argc < 3) {
    fprintf(stderr, "Need 2 args, read file name and output name\n");
    exit(1);
  }
  char* filename = argv[1];
  replacePHDRHeaderInZigWithOurOwn(filename, argv[2]);
}
