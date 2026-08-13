const std = @import("std");

const Allocator = std.mem.Allocator;
const Io = std.Io;

const Mode = enum {
    main,
    nbody,
    barnes_hut,
    benchmark,

    fn binaryBase(self: Mode) []const u8 {
        return switch (self) {
            .main => "blackhole",
            .nbody => "nbody",
            .barnes_hut => "barnes_hut",
            .benchmark => "benchmark",
        };
    }

    fn binaryName(self: Mode, alloc: Allocator) []const u8 {
        const base = self.binaryBase();
        if (comptime @import("builtin").os.tag == .windows) {
            return std.fmt.allocPrint(alloc, "{s}.exe", .{base}) catch @panic("OOM");
        }
        return base;
    }

    fn section(self: Mode) []const u8 {
        return switch (self) {
            .main => "main",
            .nbody => "nbody",
            .barnes_hut => "barnes_hut",
            .benchmark => "benchmark",
        };
    }

    fn label(self: Mode) []const u8 {
        return switch (self) {
            .main => "black hole lab (main)",
            .nbody => "n-body",
            .barnes_hut => "barnes-hut",
            .benchmark => "benchmark",
        };
    }

    fn fromName(name: []const u8) ?Mode {
        inline for (@typeInfo(Mode).@"enum".fields) |field| {
            if (std.mem.eql(u8, name, field.name)) {
                return @enumFromInt(field.value);
            }
        }
        return null;
    }
};

const Field = struct {
    key: []const u8,
    label: []const u8,
    default: []const u8,
};

// Settings editable for each program, in file order.
const fields_by_section = struct {
    const default = [_]Field{
        .{ .key = "particles", .label = "initial particles", .default = "10000" },
        .{ .key = "particles_per_step", .label = "particles per + / -", .default = "1000" },
        .{ .key = "g", .label = "gravity constant", .default = "5000" },
        .{ .key = "black_hole_mass", .label = "black hole mass", .default = "1000" },
        .{ .key = "event_horizon", .label = "event horizon", .default = "18" },
    };

    const nbody = [_]Field{
        .{ .key = "particles", .label = "initial particles", .default = "1000" },
        .{ .key = "particles_per_step", .label = "particles per + / -", .default = "1000" },
        .{ .key = "g", .label = "gravity constant", .default = "80" },
        .{ .key = "black_hole_mass", .label = "black hole mass", .default = "50000" },
        .{ .key = "event_horizon", .label = "event horizon", .default = "20" },
        .{ .key = "softening", .label = "softening", .default = "4" },
    };

    const barnes_hut = [_]Field{
        .{ .key = "particles", .label = "initial particles", .default = "10000" },
        .{ .key = "particles_per_step", .label = "particles per + / -", .default = "1000" },
        .{ .key = "g", .label = "gravity constant", .default = "80" },
        .{ .key = "black_hole_mass", .label = "black hole mass", .default = "50000" },
        .{ .key = "event_horizon", .label = "event horizon", .default = "20" },
        .{ .key = "softening", .label = "softening", .default = "4" },
        .{ .key = "theta", .label = "barnes-hut theta", .default = "0.5" },
    };

    const benchmark = [_]Field{
        .{ .key = "initial_particles", .label = "initial particles", .default = "100000" },
        .{ .key = "particles_per_level", .label = "particles per level", .default = "100000" },
        .{ .key = "level_seconds", .label = "level length (s)", .default = "10" },
        .{ .key = "max_levels", .label = "max levels", .default = "12" },
        .{ .key = "g", .label = "gravity constant", .default = "5000" },
        .{ .key = "black_hole_mass", .label = "black hole mass", .default = "1000" },
        .{ .key = "event_horizon", .label = "event horizon", .default = "18" },
    };

    fn get(section: []const u8) []const Field {
        if (std.mem.eql(u8, section, "main")) return &default;
        if (std.mem.eql(u8, section, "nbody")) return &nbody;
        if (std.mem.eql(u8, section, "barnes_hut")) return &barnes_hut;
        return &benchmark;
    }
};

const default_config =
    "; Black Hole Lab / Benchmark configuration.\n" ++
    "; Each program reads the section matching its name.\n" ++
    "; Lines starting with ';' are comments.\n" ++
    "\n" ++
    "[main]\n" ++
    "particles = 10000\n" ++
    "particles_per_step = 1000\n" ++
    "g = 5000\n" ++
    "black_hole_mass = 1000\n" ++
    "event_horizon = 18\n" ++
    "\n" ++
    "[nbody]\n" ++
    "particles = 1000\n" ++
    "particles_per_step = 1000\n" ++
    "g = 80\n" ++
    "black_hole_mass = 50000\n" ++
    "event_horizon = 20\n" ++
    "softening = 4\n" ++
    "\n" ++
    "[barnes_hut]\n" ++
    "particles = 10000\n" ++
    "particles_per_step = 1000\n" ++
    "g = 80\n" ++
    "black_hole_mass = 50000\n" ++
    "event_horizon = 20\n" ++
    "softening = 4\n" ++
    "theta = 0.5\n" ++
    "\n" ++
    "[benchmark]\n" ++
    "initial_particles = 100000\n" ++
    "particles_per_level = 100000\n" ++
    "level_seconds = 10\n" ++
    "max_levels = 12\n" ++
    "g = 5000\n" ++
    "black_hole_mass = 1000\n" ++
    "event_horizon = 18\n";

const Assign = struct {
    key: []const u8,
    value: []const u8,
    /// Everything from the start of the line up to the value (inclusive of
    /// any whitespace right after '=').
    prefix: []const u8,
    /// Trailing whitespace after the value.
    suffix: []const u8,
};

const Line = struct {
    /// Original line, without the trailing '\n'.
    raw: []const u8,
    /// Set for [section] lines; lowercased for matching.
    section: ?[]const u8 = null,
    assign: ?Assign = null,
};

fn lower(alloc: Allocator, text: []const u8) ![]const u8 {
    const out = try alloc.alloc(u8, text.len);
    for (out, text) |*d, s| {
        d.* = std.ascii.toLower(s);
    }
    return out;
}

/// Parse a config file into lines. All string contents are owned by the
/// allocator and live until the process exits.
fn parseConfig(alloc: Allocator, content: []const u8) ![]Line {
    var lines = std.ArrayList(Line).empty;

    var it = std.mem.splitScalar(u8, content, '\n');

    while (it.next()) |seg| {
        const trimmed = std.mem.trim(u8, seg, " \t");

        var line: Line = .{ .raw = seg };

        if (trimmed.len > 0) {
            if (trimmed[0] == ';' or trimmed[0] == '#') {
                // comment: keep raw
            } else if (trimmed[0] == '[' and trimmed[trimmed.len - 1] == ']') {
                const name = std.mem.trim(u8, trimmed[1 .. trimmed.len - 1], " \t");
                line.section = try lower(alloc, name);
            } else if (std.mem.indexOfScalar(u8, seg, '=')) |eq| {
                var value_begin: usize = eq + 1;

                while (value_begin < seg.len and
                    (seg[value_begin] == ' ' or seg[value_begin] == '\t'))
                {
                    value_begin += 1;
                }

                var value_end = seg.len;

                while (value_end > value_begin and
                    (seg[value_end - 1] == ' ' or seg[value_end - 1] == '\t'))
                {
                    value_end -= 1;
                }

                line.assign = .{
                    .key = try lower(alloc, std.mem.trim(u8, seg[0..eq], " \t")),
                    .value = try alloc.dupe(u8, seg[value_begin..value_end]),
                    .prefix = try alloc.dupe(u8, seg[0..value_begin]),
                    .suffix = try alloc.dupe(u8, seg[value_end..]),
                };
            }
        }

        try lines.append(alloc, line);
    }

    return lines.toOwnedSlice(alloc);
}

fn serializeConfig(alloc: Allocator, lines: []const Line) ![]const u8 {
    var out = std.ArrayList(u8).empty;

    for (lines, 0..) |line, i| {
        if (i > 0) try out.append(alloc, '\n');

        if (line.assign) |a| {
            try out.appendSlice(alloc, a.prefix);
            try out.appendSlice(alloc, a.value);
            try out.appendSlice(alloc, a.suffix);
        } else {
            try out.appendSlice(alloc, line.raw);
        }
    }

    return out.toOwnedSlice(alloc);
}

fn printHelp() void {
    std.debug.print(
        \\usage: blackhole-runner [options]
        \\
        \\interactively configure settings and run the black hole programs.
        \\
        \\options:
        \\  --mode <name>    program: main | nbody | barnes_hut | benchmark
        \\  --count <n>      number of instances to spawn (default: 1)
        \\  --config <path>  config file to read and edit (default: next to this runner)
        \\  --bin-dir <dir>  directory containing the program binaries
        \\  --no-edit        skip the settings editor
        \\  --dry-run        print what would run without launching anything
        \\  -h, --help       show this help
        \\
    , .{});
}

pub fn main(init: std.process.Init) !void {
    const io = init.io;
    // Permanent process-wide storage; cleaned up automatically on exit.
    const alloc = init.arena.allocator();

    var mode: ?Mode = null;
    var count: usize = 1;
    var config_path: ?[]const u8 = null;
    var bin_dir: ?[]const u8 = null;
    var no_edit = false;
    var dry_run = false;

    var arg_it = try std.process.Args.Iterator.initAllocator(init.minimal.args, alloc);
    defer arg_it.deinit();

    _ = arg_it.next(); // argv[0]

    while (arg_it.next()) |arg| {
        if (std.mem.eql(u8, arg, "-h") or std.mem.eql(u8, arg, "--help")) {
            printHelp();
            return;
        } else if (std.mem.eql(u8, arg, "--mode")) {
            const value = arg_it.next() orelse return error.MissingArgValue;
            mode = Mode.fromName(value) orelse {
                std.debug.print("unknown mode '{s}'\n", .{value});
                return error.InvalidMode;
            };
        } else if (std.mem.eql(u8, arg, "--count")) {
            const value = arg_it.next() orelse return error.MissingArgValue;
            count = std.fmt.parseInt(usize, value, 10) catch {
                std.debug.print("invalid count '{s}'\n", .{value});
                return error.InvalidCount;
            };
            if (count == 0) count = 1;
        } else if (std.mem.eql(u8, arg, "--config")) {
            config_path = arg_it.next() orelse return error.MissingArgValue;
        } else if (std.mem.eql(u8, arg, "--bin-dir")) {
            bin_dir = arg_it.next() orelse return error.MissingArgValue;
        } else if (std.mem.eql(u8, arg, "--no-edit")) {
            no_edit = true;
        } else if (std.mem.eql(u8, arg, "--dry-run")) {
            dry_run = true;
        } else {
            std.debug.print("unknown argument '{s}'\n\n", .{arg});
            printHelp();
            return error.InvalidArgument;
        }
    }

    // Where this runner lives; binaries and config default to this directory.
    const exe_dir = try std.process.executableDirPathAlloc(io, alloc);

    if (mode == null and dry_run) {
        std.debug.print("error: --dry-run requires --mode\n", .{});
        return error.ModeRequired;
    }

    // Single stdin reader / stdout writer shared by every prompt. Creating
    // them is lazy; no I/O happens until the first read/write.
    var stdin_buf: [4096]u8 = undefined;
    var stdout_buf: [4096]u8 = undefined;
    var stdin_reader = std.Io.File.stdin().readerStreaming(io, &stdin_buf);
    var stdout_writer = std.Io.File.stdout().writer(io, &stdout_buf);

    // Interactive program selection.
    if (mode == null) {
        while (true) {
            try stdout_writer.interface.print("\nwhich program?\n", .{});
            inline for (@typeInfo(Mode).@"enum".fields, 0..) |field, i| {
                const m: Mode = @enumFromInt(field.value);
                try stdout_writer.interface.print("  {d}) {s}\n", .{ i + 1, m.label() });
            }

            const answer = try askLine(&stdin_reader, &stdout_writer, alloc, "choice [1]: ");

            if (answer.len == 0) {
                mode = .main;
                break;
            }

            const choice = std.fmt.parseInt(usize, answer, 10) catch null;

            if (choice) |c| {
                if (c >= 1 and c <= @typeInfo(Mode).@"enum".fields.len) {
                    mode = @enumFromInt(@as(u8, @intCast(c - 1)));
                    break;
                }
            }

            try stdout_writer.interface.print("enter a number between 1 and {d}\n", .{@typeInfo(Mode).@"enum".fields.len});
        }
    }

    const selected = mode.?;

    // Resolve config path: default to exe_dir/config.ini, or the project
    // root config.ini when this runner lives in a bin/ subdirectory.
    const config_path_resolved = if (config_path) |p|
        p
    else blk: {
        const direct = try std.fs.path.join(alloc, &.{ exe_dir, "config.ini" });
        if (try fileExists(io, direct)) break :blk direct;

        const parent = std.fs.path.dirname(exe_dir) orelse ".";
        const parent_cfg = try std.fs.path.join(alloc, &.{ parent, "config.ini" });
        break :blk parent_cfg;
    };

    // Make sure a config file exists.
    const content = std.Io.Dir.cwd().readFileAlloc(io, config_path_resolved, alloc, .unlimited) catch |err| switch (err) {
        error.FileNotFound => blk: {
            try std.Io.Dir.cwd().writeFile(io, .{
                .sub_path = config_path_resolved,
                .data = default_config,
            });
            std.debug.print("created {s}\n", .{config_path_resolved});
            break :blk try alloc.dupe(u8, default_config);
        },
        else => return err,
    };

    const lines = try parseConfig(alloc, content);

    if (!no_edit) {
        const edited = try editSettings(alloc, &stdin_reader, &stdout_writer, lines, selected, config_path_resolved);
        if (edited) {
            const new_content = try serializeConfig(alloc, lines);
            try std.Io.Dir.cwd().writeFile(io, .{
                .sub_path = config_path_resolved,
                .data = new_content,
            });
            std.debug.print("wrote {s}\n", .{config_path_resolved});
        }
    }

    // Locate the binary.
    const binary = try findBinary(io, alloc, selected, bin_dir, exe_dir);

    // Summarize what will run.
    try stdout_writer.interface.print(
        "\nrunning {d}x {s}\n  binary: {s}\n  config: {s}\n",
        .{ count, selected.label(), binary, config_path_resolved },
    );

    for (fields_by_section.get(selected.section())) |field| {
        try stdout_writer.interface.print("  {s} = {s}\n", .{ field.key, currentValue(lines, selected.section(), field.key, field.default) });
    }

    try stdout_writer.interface.flush();

    if (dry_run) return;

    for (0..count) |i| {
        std.debug.print("\n[{d}/{d}] starting {s}\n", .{ i + 1, count, selected.label() });

        var child = try std.process.spawn(io, .{
            .argv = &.{ binary, "--config", config_path_resolved },
            .stdin = .inherit,
            .stdout = .inherit,
            .stderr = .inherit,
        });

        const term = try child.wait(io);

        switch (term) {
            .exited => |code| {
                if (code != 0) {
                    std.debug.print("  exited with code {d}\n", .{code});
                }
            },
            .signal => |sig| {
                std.debug.print("  killed by signal {d}\n", .{@intFromEnum(sig)});
            },
            .stopped => |sig| {
                std.debug.print("  stopped by signal {d}\n", .{@intFromEnum(sig)});
            },
            .unknown => |code| {
                std.debug.print("  terminated with unknown status {d}\n", .{code});
            },
        }
    }
}

fn currentValue(lines: []const Line, section: []const u8, key: []const u8, fallback: []const u8) []const u8 {
    var in_section = false;

    for (lines) |line| {
        if (line.section) |s| {
            in_section = std.mem.eql(u8, s, section);
        } else if (in_section) {
            if (line.assign) |a| {
                if (std.mem.eql(u8, a.key, key)) return a.value;
            }
        }
    }

    return fallback;
}

fn setValue(lines: []const Line, section: []const u8, key: []const u8, value: []const u8) void {
    var in_section = false;

    for (@constCast(lines)) |*line| {
        if (line.section) |s| {
            in_section = std.mem.eql(u8, s, section);
        } else if (in_section) {
            if (line.assign) |*a| {
                if (std.mem.eql(u8, a.key, key)) {
                    a.value = value;
                    return;
                }
            }
        }
    }
}

fn fileExists(io: Io, path: []const u8) !bool {
    std.Io.Dir.cwd().access(io, path, .{}) catch |err| {
        if (err == error.FileNotFound) return false;
        return err;
    };
    return true;
}

fn findBinary(io: Io, gpa: Allocator, mode: Mode, bin_dir: ?[]const u8, exe_dir: []const u8) ![]const u8 {
    const exe_name = mode.binaryName(gpa);

    if (bin_dir) |dir| {
        const path = try std.fs.path.join(gpa, &.{ dir, exe_name });
        if (try fileExists(io, path)) return path;
        std.debug.print("warning: no {s} in {s}\n", .{ exe_name, dir });
    }

    const exe_dir_bin = try std.fs.path.join(gpa, &.{ exe_dir, "bin" });

    const dirs = [_][]const u8{ exe_dir, exe_dir_bin, ".", "./bin" };

    for (dirs) |dir| {
        const path = try std.fs.path.join(gpa, &.{ dir, exe_name });

        if (try fileExists(io, path)) return path;
    }

    std.debug.print("error: could not find {s} next to this runner, in bin/, or in the current directory\n", .{exe_name});
    return error.BinaryNotFound;
}

fn askLine(
    reader: *std.Io.File.Reader,
    writer: *std.Io.File.Writer,
    gpa: Allocator,
    prompt: []const u8,
) ![]const u8 {
    try writer.interface.print("{s}", .{prompt});
    try writer.interface.flush();

    const line = reader.interface.takeDelimiterInclusive('\n') catch |err| switch (err) {
        error.EndOfStream => return gpa.dupe(u8, ""),
        else => return err,
    };

    return gpa.dupe(u8, std.mem.trim(u8, line, " \t\r\n"));
}

fn editSettings(
    gpa: Allocator,
    reader: *std.Io.File.Reader,
    writer: *std.Io.File.Writer,
    lines: []const Line,
    mode: Mode,
    config_path: []const u8,
) !bool {
    try writer.interface.print("\nsettings in {s}\n", .{config_path});

    const fields = fields_by_section.get(mode.section());

    for (fields) |field| {
        const current = currentValue(lines, mode.section(), field.key, field.default);
        try writer.interface.print("  {s} = {s}\n", .{ field.key, current });
    }

    const answer = try askLine(reader, writer, gpa, "\nedit settings? (y/N) ");

    if (answer.len == 0 or std.ascii.toLower(answer[0]) != 'y') {
        return false;
    }

    var changed = false;

    for (fields) |field| {
        const current = currentValue(lines, mode.section(), field.key, field.default);
        const prompt = try std.fmt.allocPrint(gpa, "  {s} ({s}) [{s}]: ", .{ field.key, field.label, current });

        const input = try askLine(reader, writer, gpa, prompt);

        if (input.len == 0) continue;

        setValue(lines, mode.section(), field.key, input);
        changed = true;
    }

    return changed;
}
