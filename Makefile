
all: copymyself_replace copymyself_add

copymyself_add: copymyself addprogramheader
	./addprogramheader copymyself copymyself_add

copymyself_replace: copymyself replaceprogramheader
	./replaceprogramheader copymyself copymyself_replace

copymyself: copymyself.zig
	zig build-exe -O ReleaseSmall copymyself.zig --strip

replaceprogramheader:
	gcc replaceprogramheader.c -o replaceprogramheader

addprogramheader:
	gcc addprogramheader.c -o addprogramheader

clean:
	rm -f copymyself
	rm -f addprogramheader
	rm -f replaceprogramheader
	rm -f copymyself_add
	rm -f copymyself_replace
	rm -rf zig-cache
