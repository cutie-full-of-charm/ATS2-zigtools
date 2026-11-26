const std = @import("std");

pub fn main(init: std.process.Init) !void {
    const io = init.io;
    const args = try init.minimal.args.toSlice(init.arena.allocator());
    if (args.len != 3) {
        try std.Io.File.stderr().writeStreamingAll(
            io,
            "usage: ats2-unpack <prepatched-source.tgz> <output-directory>\n",
        );
        return error.InvalidArguments;
    }

    const archive = try std.Io.Dir.cwd().openFile(io, args[1], .{});
    defer archive.close(io);

    var file_buffer: [64 * 1024]u8 = undefined;
    var file_reader: std.Io.File.Reader = .init(archive, io, &file_buffer);
    var inflate_buffer: [std.compress.flate.max_window_len]u8 = undefined;
    var inflate: std.compress.flate.Decompress = .init(
        &file_reader.interface,
        .gzip,
        &inflate_buffer,
    );

    try std.Io.Dir.cwd().createDirPath(io, args[2]);
    const out = try std.Io.Dir.cwd().openDir(io, args[2], .{});
    defer out.close(io);
    try std.tar.extract(io, out, &inflate.reader, .{
        .strip_components = 1,
        .exclude_empty_directories = true,
    });

    try out.access(io, "src/CBOOT/pats_main_dats.c", .{});
    try out.access(io, "prelude/CATS/basics.cats", .{});
}
