package com.axuan.vivomiuix;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

/** Reads declared methods and their actual instruction references in ordinary APK DEX files. */
final class DexIndex {
    final Map<String, MethodData> methods = new LinkedHashMap<>();
    final Map<String, String> parents = new LinkedHashMap<>();

    static final class Ref {
        final String owner;
        final String name;
        final String type;

        Ref(String owner, String name, String type) {
            this.owner = owner;
            this.name = name;
            this.type = type;
        }

        String key() { return owner + "->" + name + type; }
        @Override public String toString() { return key(); }
        @Override public int hashCode() { return key().hashCode(); }
        @Override public boolean equals(Object other) {
            return other instanceof Ref && key().equals(((Ref) other).key());
        }
    }

    static final class Facts {
        final Set<String> strings = new LinkedHashSet<>();
        final Set<Ref> calls = new LinkedHashSet<>();
        final Set<Ref> fields = new LinkedHashSet<>();
        final Set<Integer> constants = new LinkedHashSet<>();
        final Set<Integer> opcodes = new LinkedHashSet<>();
        int instructions;

        boolean text(String fragment) {
            for (String value : strings) {
                if (value.contains(fragment)) return true;
            }
            return false;
        }
    }

    static final class MethodData {
        final Ref ref;
        final int flags;
        final int code;
        private final Reader reader;
        private Facts facts;

        MethodData(Ref ref, int flags, int code, Reader reader) {
            this.ref = ref;
            this.flags = flags;
            this.code = code;
            this.reader = reader;
        }

        boolean isStatic() { return (flags & 8) != 0; }
        boolean calls(Ref target) throws IOException { return reader.hasCall(code, target); }
        int[] codeUnits() throws IOException {
            int count = reader.u32(code + 12);
            int[] units = new int[count];
            for (int i = 0; i < count; ++i) units[i] = reader.u16(code + 16 + i * 2);
            return units;
        }
        Ref invokedAt(int offset) throws IOException {
            int[] units = codeUnits();
            int opcode = units[offset] & 255;
            if (!((opcode >= 0x6e && opcode <= 0x72) || (opcode >= 0x74 && opcode <= 0x78)))
                throw new IOException("Not an ordinary invocation");
            return reader.item(reader.methodIds, units[offset + 1]);
        }
        Facts facts() throws IOException {
            if (facts == null) facts = reader.facts(code);
            return facts;
        }
    }

    static DexIndex readApks(List<File> apks) throws IOException {
        return readApks(apks, true);
    }
    static DexIndex readApks(List<File> apks, boolean requireDex) throws IOException {
        DexIndex result = new DexIndex();
        for (File apk : apks) {
            try (ZipFile zip = new ZipFile(apk)) {
                for (ZipEntry entry : Collections.list(zip.entries())) {
                    if (!entry.getName().matches("classes(?:[2-9][0-9]*|1[0-9]+)?\\.dex")) {
                        continue;
                    }
                    try (InputStream stream = zip.getInputStream(entry)) {
                        long length=entry.getSize();
                        if(length<0||length>64*1024*1024)throw new IOException("DEX exceeds analysis limit");
                        byte[] bytes=new byte[(int)length];int offset=0;
                        while(offset<bytes.length) {
                            int count=stream.read(bytes,offset,bytes.length-offset);
                            if(count<=0)throw new IOException("Truncated DEX entry");offset+=count;
                        }
                        if(stream.read()!=-1)throw new IOException("DEX entry length mismatch");
                        result.addDex(bytes);
                    }
                }
            }
        }
        if (requireDex && result.methods.isEmpty()) throw new IOException("No ordinary APK DEX found");
        return result;
    }

    void addDex(byte[] bytes) throws IOException { new Reader(bytes).addTo(this); }
    MethodData get(Ref ref) { return methods.get(ref.key()); }

    boolean extendsType(String owner, String ancestor) {
        for (int depth = 0; owner != null && depth < 32; depth++) {
            if (owner.equals(ancestor)) return true;
            owner = parents.get(owner);
        }
        return false;
    }

    private static final class Reader {
        final byte[] bytes;
        final String[] strings;
        final String[] types;
        final String[] prototypes;
        final Ref[] methodIds;
        final Ref[] fieldIds;

        Reader(byte[] bytes) throws IOException {
            this.bytes = bytes;
            if (bytes.length < 112 || bytes[0] != 'd' || bytes[1] != 'e'
                    || bytes[2] != 'x' || bytes[3] != '\n'
                    || u32(36) != 112 || u32(40) != 0x12345678
                    || u32(32) != bytes.length) {
                throw new IOException("Unsupported or truncated DEX header");
            }
            strings = new String[count(56, 60, 4)];
            int offset = u32(60);
            for (int i = 0; i < strings.length; i++) {
                Cursor cursor = new Cursor(u32(offset + 4 * i));
                cursor.uleb(); // UTF-16 length; matched names and log fragments are ASCII.
                int end = cursor.position;
                while (end < bytes.length && bytes[end] != 0) end++;
                if (end == bytes.length) throw new IOException("Unterminated DEX string");
                strings[i] = new String(bytes, cursor.position, end - cursor.position,
                        StandardCharsets.UTF_8);
            }
            types = new String[count(64, 68, 4)];
            offset = u32(68);
            for (int i = 0; i < types.length; i++) types[i] = item(strings, u32(offset + 4 * i));
            prototypes = new String[count(72, 76, 12)];
            offset = u32(76);
            for (int i = 0; i < prototypes.length; i++) {
                int position = offset + 12 * i;
                int parameters = u32(position + 8);
                StringBuilder signature = new StringBuilder("(");
                if (parameters != 0) {
                    int size = u32(parameters);
                    range(parameters + 4, (long) size * 2);
                    for (int j = 0; j < size; j++) {
                        signature.append(item(types, u16(parameters + 4 + 2 * j)));
                    }
                }
                prototypes[i] = signature.append(')').append(
                        item(types, u32(position + 4))).toString();
            }
            methodIds = new Ref[count(88, 92, 8)];
            offset = u32(92);
            for (int i = 0; i < methodIds.length; i++) {
                int position = offset + 8 * i;
                methodIds[i] = new Ref(item(types, u16(position)),
                        item(strings, u32(position + 4)),
                        item(prototypes, u16(position + 2)));
            }
            fieldIds = new Ref[count(80, 84, 8)];
            offset = u32(84);
            for (int i = 0; i < fieldIds.length; i++) {
                int position = offset + 8 * i;
                fieldIds[i] = new Ref(item(types, u16(position)),
                        item(strings, u32(position + 4)), item(types, u16(position + 2)));
            }
        }

        void addTo(DexIndex target) throws IOException {
            int size = count(96, 100, 32);
            int offset = u32(100);
            for (int i = 0; i < size; i++) {
                int position = offset + 32 * i;
                String owner = item(types, u32(position));
                int parent = u32(position + 8);
                target.parents.put(owner, parent == -1 ? null : item(types, parent));
                int data = u32(position + 24);
                if (data == 0) continue;
                Cursor cursor = new Cursor(data);
                int staticFields = cursor.uleb(), instanceFields = cursor.uleb();
                int directMethods = cursor.uleb(), virtualMethods = cursor.uleb();
                for (int j = 0; j < staticFields + instanceFields; j++) {
                    cursor.uleb();
                    cursor.uleb();
                }
                for (int methodCount : new int[]{directMethods, virtualMethods}) {
                    int index = 0;
                    for (int j = 0; j < methodCount; j++) {
                        index += cursor.uleb();
                        int flags = cursor.uleb(), code = cursor.uleb();
                        Ref ref = item(methodIds, index);
                        if (code == 0) continue;
                        if (target.methods.containsKey(ref.key())) {
                            throw new IOException("Duplicate method across APK DEX");
                        }
                        target.methods.put(ref.key(), new MethodData(ref, flags, code, this));
                    }
                }
            }
        }

        boolean hasCall(int code, Ref target) throws IOException {
            int size = u32(code + 12), start = code + 16;
            range(start, (long) size * 2);
            for (int at = 0; at < size;) {
                int unit = u16(start + at * 2), opcode = unit & 255;
                int width = width(opcode);
                if (opcode == 0 && unit != 0) {
                    int payloadSize = u16(start + (at + 1) * 2);
                    if (unit == 0x100) width = 4 + 2 * payloadSize;
                    else if (unit == 0x200) width = 2 + 4 * payloadSize;
                    else if (unit == 0x300) {
                        long elements = Integer.toUnsignedLong(u32(start + (at + 2) * 2));
                        long count = 4 + (elements * payloadSize + 1) / 2;
                        if (count > Integer.MAX_VALUE) throw new IOException("Invalid DEX payload");
                        width = (int) count;
                    } else throw new IOException("Unknown DEX payload");
                }
                if (width == 0 || width > size - at) throw new IOException("Invalid DEX instruction");
                if (((opcode >= 0x6e && opcode <= 0x72) || (opcode >= 0x74 && opcode <= 0x78))
                        && item(methodIds, u16(start + (at + 1) * 2)).equals(target)) return true;
                at += width;
            }
            return false;
        }

        Facts facts(int code) throws IOException {
            Facts result = new Facts();
            int size = u32(code + 12), start = code + 16;
            range(start, (long) size * 2);
            for (int at = 0; at < size;) {
                int unit = u16(start + at * 2), opcode = unit & 255;
                int width = width(opcode);
                if (opcode == 0 && unit != 0) {
                    int payloadSize = u16(start + (at + 1) * 2);
                    if (unit == 0x100) width = 4 + 2 * payloadSize;
                    else if (unit == 0x200) width = 2 + 4 * payloadSize;
                    else if (unit == 0x300) {
                        long elements = Integer.toUnsignedLong(u32(start + (at + 2) * 2));
                        long length = 4 + (elements * payloadSize + 1) / 2;
                        if (length > Integer.MAX_VALUE) throw new IOException("Invalid DEX payload");
                        width = (int) length;
                    } else throw new IOException("Unknown DEX payload");
                }
                if (width == 0 || width > size - at) throw new IOException("Invalid DEX instruction");
                result.instructions++;
                result.opcodes.add(opcode);
                if (opcode == 0x1a || opcode == 0x1b) {
                    int index = opcode == 0x1a ? u16(start + (at + 1) * 2)
                            : u32(start + (at + 1) * 2);
                    result.strings.add(item(strings, index));
                } else if ((opcode >= 0x6e && opcode <= 0x72)
                        || (opcode >= 0x74 && opcode <= 0x78)
                        || opcode == 0xfa || opcode == 0xfb) {
                    result.calls.add(item(methodIds, u16(start + (at + 1) * 2)));
                } else if (opcode >= 0x52 && opcode <= 0x6d) {
                    result.fields.add(item(fieldIds, u16(start + (at + 1) * 2)));
                } else if (opcode == 0x12) {
                    result.constants.add((unit >> 12) >= 8 ? (unit >> 12) - 16 : unit >> 12);
                } else if (opcode == 0x13) {
                    result.constants.add((int) (short) u16(start + (at + 1) * 2));
                } else if (opcode == 0x14) {
                    result.constants.add(u32(start + (at + 1) * 2));
                } else if (opcode == 0x15) {
                    result.constants.add(u16(start + (at + 1) * 2) << 16);
                } else if (opcode >= 0xd0 && opcode <= 0xd7) {
                    result.constants.add((int) (short) u16(start + (at + 1) * 2));
                } else if (opcode >= 0xd8 && opcode <= 0xe2) {
                    result.constants.add((int) (byte) (u16(start + (at + 1) * 2) >> 8));
                }
                at += width;
            }
            return result;
        }

        int count(int sizeOffset, int tableOffset, int stride) throws IOException {
            int size = u32(sizeOffset);
            range(u32(tableOffset), (long) size * stride);
            return size;
        }
        void range(int position, long length) throws IOException {
            if (position < 0 || length < 0 || (long) position + length > bytes.length) {
                throw new IOException("DEX range out of bounds");
            }
        }
        int u16(int position) throws IOException {
            range(position, 2);
            return (bytes[position] & 255) | ((bytes[position + 1] & 255) << 8);
        }
        int u32(int position) throws IOException {
            range(position, 4);
            return (bytes[position] & 255) | ((bytes[position + 1] & 255) << 8)
                    | ((bytes[position + 2] & 255) << 16) | (bytes[position + 3] << 24);
        }
        <T> T item(T[] array, int index) throws IOException {
            if (index < 0 || index >= array.length) throw new IOException("Invalid DEX index");
            return array[index];
        }
        final class Cursor {
            int position;
            Cursor(int position) { this.position = position; }
            int uleb() throws IOException {
                int value = 0;
                for (int shift = 0; shift <= 28; shift += 7) {
                    range(position, 1);
                    int unit = bytes[position++] & 255;
                    value |= (unit & 127) << shift;
                    if ((unit & 128) == 0) {
                        if (value < 0) throw new IOException("Overflowing DEX ULEB128");
                        return value;
                    }
                }
                throw new IOException("Invalid DEX ULEB128");
            }
        }
    }

    private static final int[] WIDTHS = new int[256];
    static {
        WIDTHS[0] = 1;
        for (int start : new int[]{1, 4, 7}) {
            WIDTHS[start] = 1; WIDTHS[start + 1] = 2; WIDTHS[start + 2] = 3;
        }
        Arrays.fill(WIDTHS, 0x0a, 0x13, 1);
        for (int op : new int[]{0x13, 0x15, 0x16, 0x19, 0x1a, 0x1c, 0x1f,
                0x20, 0x22, 0x23, 0x29, 0xfe, 0xff}) WIDTHS[op] = 2;
        for (int op : new int[]{0x14, 0x17, 0x1b, 0x24, 0x25, 0x26, 0x2a,
                0x2b, 0x2c, 0xfc, 0xfd}) WIDTHS[op] = 3;
        WIDTHS[0x18] = 5;
        for (int op : new int[]{0x1d, 0x1e, 0x21, 0x27, 0x28}) WIDTHS[op] = 1;
        Arrays.fill(WIDTHS, 0x2d, 0x3e, 2);
        Arrays.fill(WIDTHS, 0x44, 0x6e, 2);
        Arrays.fill(WIDTHS, 0x6e, 0x73, 3);
        Arrays.fill(WIDTHS, 0x74, 0x79, 3);
        Arrays.fill(WIDTHS, 0x7b, 0x90, 1);
        Arrays.fill(WIDTHS, 0x90, 0xb0, 2);
        Arrays.fill(WIDTHS, 0xb0, 0xd0, 1);
        Arrays.fill(WIDTHS, 0xd0, 0xe3, 2);
        WIDTHS[0xfa] = 4; WIDTHS[0xfb] = 4;
    }
    static int width(int opcode) { return WIDTHS[opcode]; }
}
