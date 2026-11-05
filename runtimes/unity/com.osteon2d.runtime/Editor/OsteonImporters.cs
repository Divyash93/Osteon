using System.IO;
using UnityEditor;
using UnityEditor.AssetImporters;
using UnityEngine;

namespace Osteon2D.Editor
{
    /// <summary>Imports Osteon2D project files (.osteon, which are Spine-format JSON plus an editor block) as text assets.</summary>
    [ScriptedImporter(1, "osteon")]
    public class OsteonProjectImporter : ScriptedImporter
    {
        public override void OnImportAsset(AssetImportContext ctx)
        {
            var text = new TextAsset(File.ReadAllText(ctx.assetPath));
            ctx.AddObjectToAsset("skeleton", text);
            ctx.SetMainObject(text);
        }
    }

    /// <summary>Imports libGDX / Spine style .atlas files as text assets.</summary>
    [ScriptedImporter(1, "atlas")]
    public class OsteonAtlasImporter : ScriptedImporter
    {
        public override void OnImportAsset(AssetImportContext ctx)
        {
            var text = new TextAsset(File.ReadAllText(ctx.assetPath));
            ctx.AddObjectToAsset("atlas", text);
            ctx.SetMainObject(text);
        }
    }

    /// <summary>Right click a skeleton file in the Project window: Create > Osteon2D > Skeleton Data From Selection.</summary>
    public static class OsteonCreateAssets
    {
        [MenuItem("Assets/Create/Osteon2D/Skeleton Data From Selection", true)]
        static bool Validate()
        {
            var o = Selection.activeObject as TextAsset;
            if (o == null) return false;
            string p = AssetDatabase.GetAssetPath(o);
            return p.EndsWith(".osteon") || p.EndsWith(".json");
        }

        [MenuItem("Assets/Create/Osteon2D/Skeleton Data From Selection")]
        static void Create()
        {
            var skeleton = (TextAsset)Selection.activeObject;
            string path = AssetDatabase.GetAssetPath(skeleton);
            string dir = Path.GetDirectoryName(path).Replace('\\', '/');
            string baseName = Path.GetFileNameWithoutExtension(path);

            // The atlas sits next to the skeleton: <name>.atlas, or any single .atlas in the folder.
            TextAsset atlas = AssetDatabase.LoadAssetAtPath<TextAsset>(dir + "/" + baseName + ".atlas");
            if (atlas == null)
            {
                var guids = AssetDatabase.FindAssets("t:TextAsset", new[] { dir });
                foreach (var g in guids)
                {
                    string ap = AssetDatabase.GUIDToAssetPath(g);
                    if (ap.EndsWith(".atlas")) { atlas = AssetDatabase.LoadAssetAtPath<TextAsset>(ap); break; }
                }
            }
            if (atlas == null) { EditorUtility.DisplayDialog("Osteon2D", "No .atlas file was found next to " + path + ". Export the texture atlas from Osteon2D (Tools > Texture Packer) into the same folder first.", "OK"); return; }

            var parsed = Atlas.Parse(atlas.text);
            var textures = new Texture2D[parsed.pages.Count];
            for (int i = 0; i < textures.Length; i++)
            {
                textures[i] = AssetDatabase.LoadAssetAtPath<Texture2D>(dir + "/" + parsed.pages[i].file);
                if (textures[i] == null) Debug.LogWarning("[Osteon2D] Atlas page image not found next to the atlas: " + parsed.pages[i].file);
            }

            var asset = ScriptableObject.CreateInstance<OsteonSkeletonDataAsset>();
            asset.skeletonJson = skeleton;
            asset.atlasText = atlas;
            asset.pageTextures = textures;
            string assetPath = AssetDatabase.GenerateUniqueAssetPath(dir + "/" + baseName + "_SkeletonData.asset");
            AssetDatabase.CreateAsset(asset, assetPath);

            // One material per page and blend mode so they can be tweaked or shared.
            var materials = new Material[textures.Length * 4];
            var shader = Shader.Find("Osteon2D/Default");
            for (int p = 0; p < textures.Length; p++)
                for (int b = 0; b < 4; b++)
                {
                    var m = new Material(shader) { name = baseName + "_page" + p + "_" + (BlendMode)b };
                    m.mainTexture = textures[p];
                    OsteonSkeletonDataAsset.ApplyBlend(m, (BlendMode)b);
                    AssetDatabase.AddObjectToAsset(m, asset);
                    materials[p * 4 + b] = m;
                }
            asset.materials = materials;
            EditorUtility.SetDirty(asset);
            AssetDatabase.SaveAssets();
            Selection.activeObject = asset;
        }

        [MenuItem("GameObject/Osteon2D/Skeleton Animation", false, 10)]
        static void CreateObject()
        {
            var go = new GameObject("Osteon2D Skeleton");
            go.AddComponent<OsteonSkeletonAnimation>();
            if (Selection.activeObject is OsteonSkeletonDataAsset) go.GetComponent<OsteonSkeletonAnimation>().skeletonDataAsset = (OsteonSkeletonDataAsset)Selection.activeObject;
            Undo.RegisterCreatedObjectUndo(go, "Create Osteon2D Skeleton");
            Selection.activeGameObject = go;
        }
    }
}
