const std = @import("std");

const manifest = @import("tools/source_manifest.zig");

const archive_path = "ATS2-Postiats-0.4.2-zigtools.tgz";

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});
    const bootstrap_only = b.option(bool, "bootstrap-only", "Build and install patsopt only") orelse false;
    const windows_legacy_sockets = b.option(bool, "windows-legacy-sockets", "Enable native legacy int-based ATS2 socket modules via virtual descriptors") orelse true;

    const upstream_driver = b.option(bool, "upstream-driver", "Also install original shell-based patscc as patscc-upstream") orelse false;
    const ats2 = prepareAts2(b);
    const ats2_tool_runner = addAts2ToolRunner(b);
    const patsopt = addPatsopt(b, ats2, target, optimize);
    b.installArtifact(patsopt);
    for ([_][]const u8{ "prelude", "libats", "share", "ccomp/runtime", "contrib" }) |dir| {
        b.installDirectory(.{
            .source_dir = ats2.path(b, dir),
            .install_dir = .prefix,
            .install_subdir = b.fmt("share/ats2/{s}", .{dir}),
        });
    }
    for ([_][]const u8{ "COPYING", "AUTHORS", "VERSION" }) |file| {
        const install = b.addInstallFile(ats2.path(b, file), b.fmt("share/ats2/{s}", .{file}));
        b.getInstallStep().dependOn(&install.step);
    }
    b.installFile("compat/net/ats2_net_socket.h", "share/ats2/ccomp/runtime/ats2_net_socket.h");
    b.installFile("compat/windows/ats2_legacy_socket.h", "share/ats2/ccomp/runtime/ats2_legacy_socket.h");
    b.installFile("compat/ats2/SATS/net_socket.sats", "share/ats2/compat/ats2/SATS/net_socket.sats");
    b.installFile("compat/ats2/SATS/net_socket_typed.sats", "share/ats2/compat/ats2/SATS/net_socket_typed.sats");

    b.installFile("compat/windows/ats2_win_dirent.h", "share/ats2/ccomp/runtime/ats2_win_dirent.h");
    b.installFile("compat/windows/ats2_platform.h", "share/ats2/ccomp/runtime/ats2_platform.h");
    b.installFile("compat/windows/ats2_win_crt_invalid_parameter.h", "share/ats2/ccomp/runtime/ats2_win_crt_invalid_parameter.h");

    if (bootstrap_only) return;

    const cross = target.result.cpu.arch != b.graph.host.result.cpu.arch or
        target.result.os.tag != b.graph.host.result.os.tag or
        target.result.abi != b.graph.host.result.abi;
    const generator = if (cross)
        addPatsopt(b, ats2, b.graph.host, optimize)
    else
        patsopt;

    const patscc = addNativeTool(b, "patscc", "tools/patscc.c", target, optimize);
    b.installArtifact(patscc);
    if (upstream_driver) {
        const cc_mod = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libc = true,
        });
        cc_mod.addIncludePath(ats2);
        cc_mod.addIncludePath(ats2.path(b, "ccomp/runtime"));

        const patscc_units = [_][]const u8{ "patscc", "atscc_main", "atscc_print" };
        for (patscc_units) |name| {
            const generated = atsToC(
                b,
                ats2_tool_runner,
                generator,
                ats2,
                b.fmt("{s}_dats.c", .{name}),
                ats2.path(b, b.fmt("utils/atscc/DATS/{s}.dats", .{name})),
            );
            cc_mod.addCSourceFile(.{
                .file = generated,
                .flags = &.{ "-std=c99", "-D_XOPEN_SOURCE", "-DATS_MEMALLOC_LIBC" },
            });
        }

        const original = b.addExecutable(.{
            .name = "patscc-upstream",
            .root_module = cc_mod,
        });
        b.installArtifact(original);
    }

    const lib_mod = b.createModule(.{
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    lib_mod.addIncludePath(ats2);
    lib_mod.addIncludePath(ats2.path(b, "ccomp/runtime"));
    lib_mod.addIncludePath(b.path("compat/net"));
    lib_mod.addCSourceFile(.{ .file = b.path("compat/net/ats2_net_socket.c"), .flags = &.{"-std=gnu11"} });
    if (target.result.os.tag == .windows) lib_mod.linkSystemLibrary("ws2_32", .{});
    if (target.result.os.tag == .windows) lib_mod.addIncludePath(b.path("compat/windows"));

    if (target.result.os.tag == .windows and windows_legacy_sockets) {
        lib_mod.addCSourceFile(.{
            .file = b.path("compat/windows/ats2_legacy_socket.c"),
            .flags = &.{"-std=gnu11"},
        });
    }
    addAtslibUnits(b, ats2_tool_runner, generator, ats2, lib_mod,
        target.result.os.tag == .windows and !windows_legacy_sockets);
    if (target.result.os.tag == .windows) {
        for ([_][]const u8{ "compat/windows/ats2_win_rand48.c", "compat/windows/ats2_win_fd.c", "compat/windows/ats2_win_links.c", "compat/windows/ats2_win_dirent.c" }) |path| {
            lib_mod.addCSourceFile(.{
                .file = b.path(path),
                .flags = &.{"-std=gnu11"},
            });
        }
        lib_mod.linkSystemLibrary("kernel32", .{});
        lib_mod.linkSystemLibrary("advapi32", .{});
    }
    const atslib = b.addLibrary(.{
        .name = "atslib",
        .linkage = .static,
        .root_module = lib_mod,
    });

    const lib_step = b.step("atslib", "Build and install libatslib.a");
    const install_lib = b.addInstallArtifact(atslib, .{});
    b.getInstallStep().dependOn(&install_lib.step);
    lib_step.dependOn(&install_lib.step);

    const core_step = b.step("atslib-core", "Install Windows-compatible libatslib.a");
    core_step.dependOn(&install_lib.step);
    const stdlib_for_tests = atslib;

    const stdlib_test_compile = b.step("stdlib-test-bin", "Build ATS2 stdlib smoke tests");
    const runnable = !cross;
    const test_step: ?*std.Build.Step = if (runnable)
        b.step("test-stdlib", "Run ATS2 stdlib smoke tests")
    else
        null;

    const all_tests = b.step("test", "Build and run the native ATS2 suite (cross targets: compile only)");
    if (test_step) |step| {
        all_tests.dependOn(step);
    } else {
        all_tests.dependOn(stdlib_test_compile);
        all_tests.dependOn(b.getInstallStep());
    }

    const cases = [_]struct {
        name: []const u8,
        source: []const u8,
        expected: []const u8,
        ats: bool,
    }{
        .{
            .name = "bsearch_macro",
            .source = "tests/stdlib/bsearch_macro.c",
            .expected = "bsearch macro ok",
            .ats = false,
        },
        .{
            .name = "strerror_smoke",
            .source = "tests/stdlib/strerror_smoke.dats",
            .expected = "strerror smoke ok",
            .ats = true,
        },
        .{
            .name = "drand48_sequence",
            .source = "tests/stdlib/drand48_sequence.c",
            .expected = "drand48 sequence ok",
            .ats = false,
        },
        .{
            .name = "skiplist_smoke",
            .source = "tests/stdlib/skiplist_smoke.dats",
            .expected = "skiplist init ok",
            .ats = true,
        },
        .{
            .name = "unistd_portable",
            .source = "tests/stdlib/unistd_portable.c",
            .expected = "unistd portable ok",
            .ats = false,
        },
        .{
            .name = "unistd_ats_smoke",
            .source = "tests/stdlib/unistd_ats_smoke.dats",
            .expected = "unistd ATS ok",
            .ats = true,
        },
        .{
            .name = "fcntl_descriptor",
            .source = "tests/stdlib/fcntl_descriptor.c",
            .expected = "fcntl descriptor ok",
            .ats = false,
        },
        .{
            .name = "unistd_links",
            .source = "tests/stdlib/unistd_links.c",
            .expected = "unistd links ok",
            .ats = false,
        },
        .{
            .name = "dirent_portable",
            .source = "tests/stdlib/dirent_portable.c",
            .expected = "dirent portable ok",
            .ats = false,
        },
        .{
            .name = "socket_portable",
            .source = "tests/stdlib/socket_portable.c",
            .expected = "socket portable ok",
            .ats = false,
        },
        .{
            .name = "socket_ats_smoke",
            .source = "tests/stdlib/socket_ats_smoke.dats",
            .expected = "socket ATS ok",
            .ats = true,
        },
        .{
            .name = "socket_typed_portable",
            .source = "tests/stdlib/socket_typed_portable.c",
            .expected = "socket typed portable ok",
            .ats = false,
        },
        .{
            .name = "socket_typed_smoke",
            .source = "tests/stdlib/socket_typed_smoke.dats",
            .expected = "socket typed ok",
            .ats = true,
        },
        .{
            .name = "socket_contracts",
            .source = "tests/stdlib/socket_contracts.c",
            .expected = "socket contracts ok",
            .ats = false,
        },
        .{
            .name = "socket_faults",
            .source = "tests/stdlib/socket_faults.c",
            .expected = "socket faults ok",
            .ats = false,
        },
        .{
            .name = "fd_guard",
            .source = "tests/stdlib/fd_guard.c",
            .expected = "fd guard ok",
            .ats = false,
        },
        .{
            .name = "socket_abi_layout",
            .source = "tests/stdlib/socket_abi_layout.c",
            .expected = "socket ABI ok",
            .ats = false,
        },
        .{
            .name = "socket_legacy_adapter",
            .source = "tests/stdlib/socket_legacy_adapter.c",
            .expected = "socket legacy adapter ok",
            .ats = false,
        },
        .{
            .name = "socket_legacy_ats_smoke",
            .source = "tests/stdlib/socket_legacy_ats_smoke.dats",
            .expected = "socket legacy ATS ok",
            .ats = true,
        },
    };
    for (cases) |test_case| {
        // The original socket_v ATS API is linked only when its virtual-fd
        // Windows implementation is included in libatslib.
        if (target.result.os.tag == .windows and !windows_legacy_sockets and
            std.mem.eql(u8, test_case.name, "socket_legacy_ats_smoke")) continue;
        const test_mod = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libc = true,
        });
        test_mod.addIncludePath(ats2);
        test_mod.addIncludePath(ats2.path(b, "ccomp/runtime"));
        test_mod.addIncludePath(b.path("compat/net"));
        if (target.result.os.tag == .windows) test_mod.linkSystemLibrary("ws2_32", .{});
        if (target.result.os.tag == .windows) test_mod.addIncludePath(b.path("compat/windows"));
        if (std.mem.eql(u8, test_case.name, "socket_legacy_adapter")) {
            test_mod.addIncludePath(b.path("compat/windows"));
            test_mod.addCSourceFile(.{
                .file = b.path("compat/windows/ats2_legacy_socket.c"),
                .flags = &.{"-std=gnu11"},
            });
        }
        test_mod.addCSourceFile(.{
            .file = if (test_case.ats)
                atsToC(
                    b,
                    ats2_tool_runner,
                    generator,
                    ats2,
                    b.fmt("{s}_dats.c", .{test_case.name}),
                    b.path(test_case.source),
                )
            else
                b.path(test_case.source),
            .flags = &.{ "-std=gnu11", "-D_XOPEN_SOURCE=700", "-DATS_MEMALLOC_LIBC" },
        });
        if (!std.mem.eql(u8, test_case.name, "socket_faults") and
            !std.mem.eql(u8, test_case.name, "socket_abi_layout") and
            !std.mem.eql(u8, test_case.name, "socket_legacy_adapter"))
        {
            test_mod.linkLibrary(stdlib_for_tests);
        }
        const test_exe = b.addExecutable(.{
            .name = test_case.name,
            .root_module = test_mod,
        });
        const install_test = b.addInstallArtifact(test_exe, .{});
        stdlib_test_compile.dependOn(&install_test.step);
        if (test_step) |step| {
            const run_test = b.addRunArtifact(test_exe);
            run_test.setCwd(b.tmpPath());
            run_test.has_side_effects = true;
            run_test.setEnvironmentVariable("LC_ALL", "C");
            run_test.expectStdOutMatch(test_case.expected);
            step.dependOn(&run_test.step);
        }
    }

    if (cross) return;

    const check = b.step("check", "Check native patsopt/patscc version commands");
    const run_opt = b.addRunArtifact(ats2_tool_runner);
    run_opt.addArtifactArg2(patsopt, .{ .make_absolute = true });
    run_opt.addDirectoryArg2(ats2, .{ .make_absolute = true });
    run_opt.addArg("--version");
    check.dependOn(&run_opt.step);
    const run_cc = b.addRunArtifact(ats2_tool_runner);
    run_cc.addArtifactArg2(patscc, .{ .make_absolute = true });
    run_cc.addDirectoryArg2(ats2, .{ .make_absolute = true });
    run_cc.setEnvironmentVariable("PATSCCOMP", "zig cc");
    run_cc.addArg("--zig-exe");
    run_cc.addFileArg2(std.Build.LazyPath.zig_exe, .{ .make_absolute = true });
    run_cc.addArg("--version");
    check.dependOn(&run_cc.step);
    all_tests.dependOn(check);
    // End-to-end checks use the installed tools and runtime, not cache paths.
    const integration = addNativeTool(b, "ats2-integration-test", "tools/test_installed.c", b.graph.host, .ReleaseSafe);
    const installed_test = b.addRunArtifact(integration);
    installed_test.step.dependOn(b.getInstallStep());
    installed_test.has_side_effects = true;
    installed_test.addDirectoryArg2(.{ .relative = .{ .base = .install_prefix } }, .{ .make_absolute = true });
    installed_test.addDirectoryArg2(b.path("tests"), .{ .make_absolute = true });
    installed_test.addDirectoryArg2(b.tmpPath(), .{ .make_absolute = true });
    installed_test.addFileArg2(std.Build.LazyPath.zig_exe, .{ .make_absolute = true });
    installed_test.addArg(if (target.result.os.tag == .windows and !windows_legacy_sockets) "core" else "legacy");
    all_tests.dependOn(&installed_test.step);
}

fn addNativeTool(b: *std.Build, name: []const u8, source: []const u8, target: std.Build.ResolvedTarget, optimize: std.builtin.OptimizeMode) *std.Build.Step.Compile {
    const mod = b.createModule(.{ .target = target, .optimize = optimize, .link_libc = true });
    mod.addCSourceFile(.{ .file = b.path(source), .flags = &.{"-std=gnu11"} });
    return b.addExecutable(.{ .name = name, .root_module = mod });
}

// The host helper runs as a tracked build step. The source archive is a
// declared input and the extracted tree is a single generated LazyPath.
fn prepareAts2(b: *std.Build) std.Build.LazyPath {
    const unpack = b.addExecutable(.{
        .name = "ats2-unpack",
        .root_module = b.createModule(.{
            .root_source_file = b.path("tools/unpack.zig"),
            .target = b.graph.host,
            .optimize = .ReleaseSafe,
        }),
    });
    const cmd = b.addRunArtifact(unpack);
    cmd.addFileArg2(b.path(archive_path), .{ .make_absolute = true });
    return cmd.addOutputDirectoryArg2("ats2", .{ .make_absolute = true });
}

fn addPatsopt(
    b: *std.Build,
    ats2: std.Build.LazyPath,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
) *std.Build.Step.Compile {
    const opt_mod = b.createModule(.{
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    opt_mod.addIncludePath(ats2.path(b, "src/CBOOT"));
    opt_mod.addIncludePath(ats2.path(b, "src/CBOOT/ccomp/runtime"));

    var sources: std.ArrayList([]const u8) = .empty;
    for (manifest.cboot) |source| {
        if (std.mem.eql(u8, source, "src/CBOOT/pats_main_dats.c")) continue;
        append(b, &sources, source);
    }
    // Rename only upstream's entry point; avoid editing the archived CBOOT.
    opt_mod.addCSourceFile(.{
        .file = ats2.path(b, "src/CBOOT/pats_main_dats.c"),
        .flags = &.{ "-std=gnu11", "-Dmain=ats2_patsopt_main" },
    });
    opt_mod.addCSourceFile(.{ .file = b.path("tools/patsopt_entry.c"), .flags = &.{"-std=gnu11"} });

    if (manifest.cboot_gmp) {
        opt_mod.linkSystemLibrary("gmp", .{ .use_pkg_config = .no });
    }
    if (target.result.os.tag == .windows) {
        for (sources.items, 0..) |path, i| {
            if (std.mem.eql(u8, path, "src/CBOOT/libc/DATS_unistd_dats.c")) {
                _ = sources.orderedRemove(i);
                break;
            }
        }
        opt_mod.addCSourceFile(.{
            .file = b.path("compat/windows/ats2_win_compat.c"),
            .flags = &.{"-std=gnu11"},
        });
    }
    opt_mod.addCSourceFiles(.{
        .root = ats2,
        .files = sources.items,
        .flags = &.{"-std=gnu11"},
    });
    return b.addExecutable(.{
        .name = "patsopt",
        .root_module = opt_mod,
    });
}

// Build a tiny native runner that converts Zig's generated ATS2 LazyPath into
// the absolute PATSHOME required by patsopt/patscc. It is a host tool even
// when cross-compiling ATS2 for Windows from Linux.
fn addAts2ToolRunner(b: *std.Build) *std.Build.Step.Compile {
    const mod = b.createModule(.{
        .target = b.graph.host,
        .optimize = .ReleaseSafe,
        .link_libc = true,
    });
    mod.addCSourceFile(.{
        .file = b.path("tools/run_ats2_tool.c"),
        .flags = &.{"-std=gnu11"},
    });
    return b.addExecutable(.{
        .name = "ats2-run-tool",
        .root_module = mod,
    });
}

fn atsToC(
    b: *std.Build,
    runner: *std.Build.Step.Compile,
    patsopt: *std.Build.Step.Compile,
    ats2: std.Build.LazyPath,
    output_name: []const u8,
    input: std.Build.LazyPath,
) std.Build.LazyPath {
    const cmd = b.addRunArtifact(runner);
    // ATS2 expands $PATSHOME/prelude while loading sources, including nested
    // #staloads. A relative value (".") fails even if we set the child cwd
    // to the ATS2 root. Have a native host helper set an absolute PATSHOME.
    // These args declare the executable, unpacked tree and source as tracked
    // build dependencies; no shell or bootstrap script is involved.
    cmd.addArtifactArg2(patsopt, .{ .make_absolute = true });
    cmd.addDirectoryArg2(ats2, .{ .make_absolute = true });
    cmd.addArg("--output");
    const output = cmd.addOutputFileArg2(output_name, .{ .make_absolute = true });
    cmd.addArg("--dynamic");
    cmd.addFileArg2(input, .{ .make_absolute = true });
    return output;
}

fn addAtslibUnits(
    b: *std.Build,
    runner: *std.Build.Step.Compile,
    patsopt: *std.Build.Step.Compile,
    ats2: std.Build.LazyPath,
    lib_mod: *std.Build.Module,
    windows_core_only: bool,
) void {
    for (manifest.atslib) |unit| {
        if (windows_core_only and excludedFromWindowsCore(unit.output)) continue;
        const generated = atsToC(
            b,
            runner,
            patsopt,
            ats2,
            unit.output,
            ats2.path(b, unit.source),
        );
        lib_mod.addCSourceFile(.{
            .file = generated,
            .flags = &.{ "-std=gnu11", "-fPIC", "-DATS_MEMALLOC_LIBC" },
        });
    }
}

fn excludedFromWindowsCore(output_name: []const u8) bool {
    const unsupported = [_][]const u8{
        "atslib_libats_libc_DATS_sys_socket_dats.c",
        "atslib_libats_libc_DATS_sys_socket_in_dats.c",
    };
    for (unsupported) |name| {
        if (std.mem.eql(u8, output_name, name)) return true;
    }
    return false;
}

fn append(b: *std.Build, list: *std.ArrayList([]const u8), path: []const u8) void {
    list.append(b.allocator, path) catch @panic("out of memory");
}
