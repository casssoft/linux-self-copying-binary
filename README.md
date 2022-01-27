## Useful commands

```bash
readelf --all <binary> # see elf headers
objdump -d <binary> # dissasemble binary if it has valid section headers
```

## Vim hex editor setup
Open vim and then run:
```vim
:set binary
:%!xxd
```
then change stuff and when you close it run:
```vim
:%!xxd -r
:wq
```
