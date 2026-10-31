# Replaces the icon of an .exe (here the IExpress installer) with an .ico file, using the Win32 resource API.
# Usage: set_exe_icon.ps1 -Exe dist\OsteonSetup.exe -Icon src\editor\app.ico
param([Parameter(Mandatory)][string]$Exe, [Parameter(Mandatory)][string]$Icon)
$ErrorActionPreference = 'Stop'
Add-Type -TypeDefinition @"
using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
public static class ExeIcon {
    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)] static extern IntPtr BeginUpdateResource(string file, bool deleteExisting);
    [DllImport("kernel32.dll", SetLastError = true)] static extern bool UpdateResource(IntPtr h, IntPtr type, IntPtr name, ushort lang, byte[] data, uint size);
    [DllImport("kernel32.dll", SetLastError = true)] static extern bool EndUpdateResource(IntPtr h, bool discard);
    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)] static extern IntPtr LoadLibraryEx(string file, IntPtr h, uint flags);
    [DllImport("kernel32.dll", SetLastError = true)] static extern bool FreeLibrary(IntPtr h);
    delegate bool EnumNames(IntPtr module, IntPtr type, IntPtr name, IntPtr param);
    [DllImport("kernel32.dll", SetLastError = true)] static extern bool EnumResourceNames(IntPtr module, IntPtr type, EnumNames cb, IntPtr param);
    delegate bool EnumLangs(IntPtr module, IntPtr type, IntPtr name, ushort lang, IntPtr param);
    [DllImport("kernel32.dll", SetLastError = true)] static extern bool EnumResourceLanguages(IntPtr module, IntPtr type, IntPtr name, EnumLangs cb, IntPtr param);

    public static void Apply(string exe, string ico) {
        byte[] d = File.ReadAllBytes(ico);
        int count = BitConverter.ToUInt16(d, 4);
        // Find the exe's existing icon group (name and language) so it is replaced rather than duplicated.
        IntPtr name = (IntPtr)1; ushort lang = 1033; bool found = false;
        IntPtr mod = LoadLibraryEx(exe, IntPtr.Zero, 0x2); // LOAD_LIBRARY_AS_DATAFILE
        if (mod != IntPtr.Zero) {
            EnumResourceNames(mod, (IntPtr)14, (m, t, n, p) => {
                if (!found) {
                    name = n; found = true;
                    EnumResourceLanguages(m, t, n, (m2, t2, n2, l, p2) => { lang = l; return false; }, IntPtr.Zero);
                }
                return true;
            }, IntPtr.Zero);
            FreeLibrary(mod);
        }
        IntPtr h = BeginUpdateResource(exe, false);
        if (h == IntPtr.Zero) throw new Exception("BeginUpdateResource failed: " + Marshal.GetLastWin32Error());
        var grp = new List<byte>();
        grp.AddRange(new byte[] { 0, 0, 1, 0 });
        grp.AddRange(BitConverter.GetBytes((ushort)count));
        for (int i = 0; i < count; i++) {
            int e = 6 + i * 16;
            int bytes = BitConverter.ToInt32(d, e + 8), off = BitConverter.ToInt32(d, e + 12);
            byte[] img = new byte[bytes];
            Array.Copy(d, off, img, 0, bytes);
            ushort id = (ushort)(300 + i);
            if (!UpdateResource(h, (IntPtr)3, (IntPtr)id, lang, img, (uint)bytes)) { EndUpdateResource(h, true); throw new Exception("UpdateResource(icon) failed"); }
            for (int k = 0; k < 12; k++) grp.Add(d[e + k]); // width, height, colours, reserved, planes, bit count, size
            grp.AddRange(BitConverter.GetBytes(id));
        }
        byte[] g = grp.ToArray();
        if (!UpdateResource(h, (IntPtr)14, name, lang, g, (uint)g.Length)) { EndUpdateResource(h, true); throw new Exception("UpdateResource(group) failed"); }
        if (!EndUpdateResource(h, false)) throw new Exception("EndUpdateResource failed: " + Marshal.GetLastWin32Error());
    }
}
"@
[ExeIcon]::Apply((Resolve-Path $Exe).Path, (Resolve-Path $Icon).Path)
Write-Host "Icon set on $Exe"
