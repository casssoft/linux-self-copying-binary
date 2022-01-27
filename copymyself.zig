const std = @import("std");
const fs = std.fs;
const process = std.process;

// This gets replaced with file size
const size  = [_]i64{ 0xdeadbeef,  0xdeadbeef};

pub fn main() !void {
    var arena = std.heap.ArenaAllocator.init(std.heap.page_allocator);
    defer arena.deinit();

    var arg_it = process.args();

    // skip my own exe name
    _ = arg_it.skip();

    const a = arena.allocator();

    const new_exe_name = (try arg_it.next(a)) orelse {
        std.debug.print("Expected first argument to be the new file name\n", .{});
        return error.InvalidArgs;
    };

    // 300000 is mapped to our binary ; v ;
    const elf : [*]const u8 = @intToPtr([*]const u8, 0x300000);

    //try stdout.print("GUESS WHAT IM A: {c} {c} {c} {d}\n", .{elf[1], elf[2], elf[3], size});

    const dir: fs.Dir = fs.cwd();
    const file: fs.File = try dir.createFile(
    new_exe_name,
    .{},
    );
    defer file.close();
    _ = try file.write(elf[0..size[0]]);
    // TODO set executable flags on the new file

    const stdout = std.io.getStdOut().writer();
    try stdout.print("Success!\n", .{});
}
