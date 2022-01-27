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
    // Ok so first delete all section headers cause fuck em

    // get section header offset so we can blow it away
    long sectionHeaderOffset = header.e_shoff;
    header.e_shoff = 0;

    header.e_shentsize = 0;
    header.e_shnum = 0;
    header.e_shstrndx = 0;
    // in zig the entrypoint is right thar
    //header.e_entry += header.e_phentsize;



    // OK so after that index it's all crap and probably some before that...
    // Ok lets add another program header
    int originalHeaders = header.e_phnum;
    //int newProgramHeaders = 0;//2;
    //header.e_phnum += newProgramHeaders;


    // write back the e header!!!!!!!
    memcpy(filecontents, &header, sizeof(header));



    // now we need to insert some bytes my dear friend.....
    // TO insert bytes lets just write some yknow??

    // ok lets read the program headers and fix em
    int skippedPHDR = 0;
#define MAX_HEADERS 16
    for (int i = 0; i < originalHeaders; ++i) {
      ElfW(Phdr) programHeader;
      memcpy(&programHeader, filecontents + header.e_phoff + header.e_phentsize * i, sizeof(programHeader));
      // ok lets fix this effer...
      //
      //if (programHeader.p_type == PT_LOAD && programHeader.p_offset == 0) { // OK THIS IS THE SPECIAL guy... split him in half fuck it
      //  //start of original data
      //  // Remove the start will work but we dont need to!
      //  long afterInsert = header.e_phoff + header.e_phentsize * header.e_phnum;
      //  programHeader.p_offset += afterInsert;
      //  programHeader.p_vaddr += afterInsert;
      //  programHeader.p_paddr += afterInsert;
      //  programHeader.p_filesz -= afterInsert;
      //  programHeader.p_memsz -= afterInsert;
      //  programHeader.p_align = 0; // ????
      //} else if (programHeader.p_type != PT_GNU_STACK && programHeader.p_type != PT_PHDR) {
      //  This never seemed to work
      //  programHeader.p_offset += header.e_phentsize * newProgramHeaders;
      if (programHeader.p_type == PT_PHDR) {
        // skip this fucker
        skippedPHDR = 1;
        continue;
      }

      // write this dude back to filecontents
      int index = i;
      if (skippedPHDR) {
        index = i - 1;
      }
      memcpy(filecontents + header.e_phoff + header.e_phentsize * index, &programHeader, sizeof(programHeader));
    }
    // OK now we are ready to add our broooo at the end!
    ElfW(Phdr) newHeader;
    newHeader.p_type = PT_LOAD;
    newHeader.p_offset = 0; // hehe the whole damn file bro
    newHeader.p_vaddr = 0x300000; // I hope this memory isn't taken sir...
    newHeader.p_paddr = newHeader.p_vaddr; // not sure what this should be set for. on BSD it needs to be 0
    long newFileSize = fsize + header.e_phentsize; // this should be the new file size
    newHeader.p_filesz = newFileSize;
    newHeader.p_memsz = newFileSize;
    newHeader.p_flags = PF_R; // read only
    newHeader.p_align = 0x1000; // alignment value, no idea why we need this, but might as well set it the same as zig
    memcpy(filecontents + header.e_phoff + header.e_phentsize * (originalHeaders - 1), &newHeader, sizeof(newHeader)); // put it at the end!

    // OK NOW WRITE THE FILE WITH OUR NEW HEADER

    FILE*output = fopen(newfile, "w");
    if (!output) {
      fprintf(stderr, "failed to open new file...\n");
      exit(1);
    }

    fwrite(filecontents, fsize, 1, output);
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
