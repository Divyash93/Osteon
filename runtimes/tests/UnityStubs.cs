// Minimal stand-ins for the parts of UnityEngine / UnityEditor that the Osteon2D Unity layer uses.
// They exist only so CI-less machines without Unity can still type-check runtimes/unity (see run_unity_compile_check.bat).
using System;
using System.Collections.Generic;
namespace UnityEngine
{
    public class Object { public string name; public HideFlags hideFlags; public static void Destroy(Object o) { } public static void DestroyImmediate(Object o) { } public static implicit operator bool(Object o) { return !ReferenceEquals(o, null); } }
    public enum HideFlags { None, DontSave }
    public class Component : Object { public Transform transform; public T GetComponent<T>() where T : class { return null; } public T AddComponent<T>() where T : Component, new() { return new T(); } }
    public class Behaviour : Component { }
    public class MonoBehaviour : Behaviour { }
    public class ScriptableObject : Object { public static T CreateInstance<T>() where T : ScriptableObject, new() { return new T(); } }
    public class TextAsset : Object { public string text; public TextAsset(string t) { text = t; } }
    public class Texture : Object { }
    public class Texture2D : Texture { }
    public class Shader : Object { public static Shader Find(string n) { return null; } }
    public class Material : Object { public Texture mainTexture; public Material(Shader s) { } public void SetFloat(string n, float v) { } }
    public class GameObject : Object { public GameObject(string n) { } public T AddComponent<T>() where T : Component, new() { return new T(); } public T GetComponent<T>() where T : class { return null; } }
    public class Mesh : Object
    {
        public UnityEngine.Rendering.IndexFormat indexFormat; public int subMeshCount;
        public void MarkDynamic() { } public void Clear() { } public void RecalculateBounds() { }
        public void SetVertices(List<Vector3> v) { } public void SetUVs(int ch, List<Vector2> v) { } public void SetColors(List<Color32> c) { }
        public void SetTriangles(List<int> t, int sub, bool calc = true) { }
    }
    public class MeshFilter : Component { public Mesh sharedMesh; }
    public class MeshRenderer : Component { public string sortingLayerName; public int sortingOrder; public Material[] sharedMaterials; }
    public class Transform : Component { public Vector3 position, eulerAngles, localScale; public Quaternion rotation; public Vector3 TransformPoint(Vector3 p) { return p; } }
    public struct Vector2 { public float x, y; public Vector2(float x, float y) { this.x = x; this.y = y; } }
    public struct Vector3 { public float x, y, z; public Vector3(float x, float y, float z) { this.x = x; this.y = y; this.z = z; } }
    public struct Quaternion { public static Quaternion Euler(float x, float y, float z) { return new Quaternion(); } }
    public struct Color32 { public byte r, g, b, a; public Color32(byte r, byte g, byte b, byte a) { this.r = r; this.g = g; this.b = b; this.a = a; } }
    public static class Mathf { public static float Clamp(float v, float a, float b) { return v < a ? a : v > b ? b : v; } public static int Clamp(int v, int a, int b) { return v < a ? a : v > b ? b : v; } public static int RoundToInt(float v) { return (int)Math.Round(v); } public static float Sqrt(float v) { return (float)Math.Sqrt(v); } }
    public static class Debug { public static void Log(object o) { } public static void LogWarning(object o) { } public static void LogError(object o, Object ctx = null) { } }
    public static class Application { public static bool isPlaying; }
    public static class Time { public static float deltaTime, fixedDeltaTime; }
    [AttributeUsage(AttributeTargets.Class)] public class ExecuteAlwaysAttribute : Attribute { }
    [AttributeUsage(AttributeTargets.Class)] public class DisallowMultipleComponentAttribute : Attribute { }
    [AttributeUsage(AttributeTargets.Class)] public class RequireComponent : Attribute { public RequireComponent(params Type[] t) { } }
    [AttributeUsage(AttributeTargets.Class)] public class AddComponentMenuAttribute : Attribute { public AddComponentMenuAttribute(string s) { } }
    [AttributeUsage(AttributeTargets.Class)] public class CreateAssetMenuAttribute : Attribute { public string menuName, fileName; }
    [AttributeUsage(AttributeTargets.Field)] public class HeaderAttribute : Attribute { public HeaderAttribute(string s) { } }
    [AttributeUsage(AttributeTargets.Field)] public class TooltipAttribute : Attribute { public TooltipAttribute(string s) { } }
}
namespace UnityEngine.Rendering { public enum IndexFormat { UInt16, UInt32 } }
namespace UnityEditor
{
    public class MenuItem : Attribute { public MenuItem(string p) { } public MenuItem(string p, bool v) { } public MenuItem(string p, bool v, int pr) { } }
    public static class AssetDatabase
    {
        public static string GetAssetPath(UnityEngine.Object o) { return ""; } public static T LoadAssetAtPath<T>(string p) where T : UnityEngine.Object { return null; }
        public static string[] FindAssets(string f, string[] d) { return new string[0]; } public static string GUIDToAssetPath(string g) { return ""; }
        public static void CreateAsset(UnityEngine.Object o, string p) { } public static string GenerateUniqueAssetPath(string p) { return p; }
        public static void AddObjectToAsset(UnityEngine.Object o, UnityEngine.Object a) { } public static void SaveAssets() { }
    }
    public static class Selection { public static UnityEngine.Object activeObject; public static UnityEngine.GameObject activeGameObject; }
    public static class EditorUtility { public static bool DisplayDialog(string a, string b, string c) { return true; } public static void SetDirty(UnityEngine.Object o) { } }
    public static class Undo { public static void RegisterCreatedObjectUndo(UnityEngine.Object o, string n) { } }
}
namespace UnityEditor.AssetImporters
{
    public class AssetImportContext { public string assetPath; public void AddObjectToAsset(string id, UnityEngine.Object o) { } public void SetMainObject(UnityEngine.Object o) { } }
    [AttributeUsage(AttributeTargets.Class)] public class ScriptedImporterAttribute : Attribute { public ScriptedImporterAttribute(int v, string ext) { } }
    public abstract class ScriptedImporter { public abstract void OnImportAsset(AssetImportContext ctx); }
}
