
all: copymyself_done

copymyself_done: copymyself addprogramheader
	./addprogramheader copymyself copymyself_done

copymyself: copymyself.zig
	zig build-exe -O ReleaseSmall copymyself.zig --strip

addprogramheader:
	gcc addprogramheader.c -o addprogramheader

clean:
	rm -f copymyself
	rm -f addprogramheader
	rm -f copymyself_done
	rm -rf zig-cache
