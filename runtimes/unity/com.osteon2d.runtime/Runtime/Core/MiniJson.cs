// Minimal JSON reader used by the Osteon2D runtime so it has no dependency on any JSON package.
// Objects become Dictionary<string, object>, arrays List<object>, numbers double, plus string, bool and null.
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Text;

namespace Osteon2D
{
    public static class MiniJson
    {
        public static object Parse(string text)
        {
            var p = new Parser(text);
            p.SkipWs();
            object v = p.ReadValue();
            return v;
        }

        sealed class Parser
        {
            readonly string s;
            int i;
            public Parser(string text) { s = text; if (s.Length > 0 && s[0] == '﻿') i = 1; }

            public void SkipWs() { while (i < s.Length && char.IsWhiteSpace(s[i])) i++; }

            public object ReadValue()
            {
                SkipWs();
                if (i >= s.Length) throw new FormatException("Unexpected end of JSON");
                char c = s[i];
                switch (c)
                {
                    case '{': return ReadObject();
                    case '[': return ReadArray();
                    case '"': return ReadString();
                    case 't': Expect("true"); return true;
                    case 'f': Expect("false"); return false;
                    case 'n': Expect("null"); return null;
                    default: return ReadNumber();
                }
            }

            void Expect(string w)
            {
                if (string.CompareOrdinal(s, i, w, 0, w.Length) != 0) throw new FormatException("Bad JSON token at " + i);
                i += w.Length;
            }

            Dictionary<string, object> ReadObject()
            {
                var d = new Dictionary<string, object>();
                i++;
                SkipWs();
                if (s[i] == '}') { i++; return d; }
                while (true)
                {
                    SkipWs();
                    string k = ReadString();
                    SkipWs();
                    if (s[i++] != ':') throw new FormatException("Expected ':' at " + i);
                    d[k] = ReadValue();
                    SkipWs();
                    char c = s[i++];
                    if (c == ',') continue;
                    if (c == '}') return d;
                    throw new FormatException("Expected ',' or '}' at " + i);
                }
            }

            List<object> ReadArray()
            {
                var l = new List<object>();
                i++;
                SkipWs();
                if (s[i] == ']') { i++; return l; }
                while (true)
                {
                    l.Add(ReadValue());
                    SkipWs();
                    char c = s[i++];
                    if (c == ',') continue;
                    if (c == ']') return l;
                    throw new FormatException("Expected ',' or ']' at " + i);
                }
            }

            string ReadString()
            {
                if (s[i] != '"') throw new FormatException("Expected string at " + i);
                i++;
                var sb = new StringBuilder();
                while (true)
                {
                    char c = s[i++];
                    if (c == '"') break;
                    if (c != '\\') { sb.Append(c); continue; }
                    char e = s[i++];
                    switch (e)
                    {
                        case 'n': sb.Append('\n'); break;
                        case 't': sb.Append('\t'); break;
                        case 'r': sb.Append('\r'); break;
                        case 'b': sb.Append('\b'); break;
                        case 'f': sb.Append('\f'); break;
                        case 'u': sb.Append((char)Convert.ToInt32(s.Substring(i, 4), 16)); i += 4; break;
                        default: sb.Append(e); break;
                    }
                }
                return sb.ToString();
            }

            double ReadNumber()
            {
                int st = i;
                while (i < s.Length && "+-0123456789.eE".IndexOf(s[i]) >= 0) i++;
                if (st == i) throw new FormatException("Unexpected character '" + s[i] + "' at " + i);
                return double.Parse(s.Substring(st, i - st), CultureInfo.InvariantCulture);
            }
        }

        // Typed accessors with defaults.
        public static Dictionary<string, object> Obj(object o) { return o as Dictionary<string, object>; }
        public static List<object> Arr(object o) { return o as List<object>; }
        public static bool Has(Dictionary<string, object> d, string k) { return d != null && d.ContainsKey(k); }
        public static float Num(Dictionary<string, object> d, string k, float def)
        {
            object v; return d != null && d.TryGetValue(k, out v) && v is double ? (float)(double)v : def;
        }
        public static int Int(Dictionary<string, object> d, string k, int def)
        {
            object v; return d != null && d.TryGetValue(k, out v) && v is double ? (int)(double)v : def;
        }
        public static bool Bool(Dictionary<string, object> d, string k, bool def)
        {
            object v; return d != null && d.TryGetValue(k, out v) && v is bool ? (bool)v : def;
        }
        public static string Str(Dictionary<string, object> d, string k, string def)
        {
            object v; return d != null && d.TryGetValue(k, out v) && v is string ? (string)v : def;
        }
        public static float[] Floats(Dictionary<string, object> d, string k)
        {
            object v;
            if (d == null || !d.TryGetValue(k, out v) || !(v is List<object>)) return new float[0];
            var l = (List<object>)v;
            var r = new float[l.Count];
            for (int n = 0; n < r.Length; n++) r[n] = l[n] is double ? (float)(double)l[n] : 0f;
            return r;
        }
        public static int[] Ints(Dictionary<string, object> d, string k)
        {
            var f = Floats(d, k);
            var r = new int[f.Length];
            for (int n = 0; n < r.Length; n++) r[n] = (int)f[n];
            return r;
        }
    }
}
