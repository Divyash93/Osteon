using System.Collections.Generic;
using UnityEngine;

namespace Osteon2D
{
    /// <summary>
    /// Everything a skeleton needs at run time: the skeleton JSON, the atlas text, the atlas page textures and one material per
    /// (page, blend mode). Create it with Assets > Create > Osteon2D > Skeleton Data from an .osteon / .json file.
    /// </summary>
    [CreateAssetMenu(menuName = "Osteon2D/Skeleton Data", fileName = "SkeletonData")]
    public class OsteonSkeletonDataAsset : ScriptableObject
    {
        [Tooltip("Skeleton JSON exported from Osteon2D (File > Export > JSON), or the .osteon project itself.")]
        public TextAsset skeletonJson;
        [Tooltip("libGDX / Spine style .atlas text written by Osteon2D's texture packer.")]
        public TextAsset atlasText;
        [Tooltip("The atlas page images, in page order.")]
        public Texture2D[] pageTextures;
        [Tooltip("Pixels-to-units factor. 0.01 means 100 skeleton units are 1 Unity unit.")]
        public float scale = 0.01f;
        [Tooltip("Default crossfade duration between animations on a track, in seconds.")]
        public float defaultMix = 0.2f;
        [Tooltip("Optional: one material per page and blend mode, in the order page0 Normal, Additive, Multiply, Screen, page1 ... Leave empty to create them at run time.")]
        public Material[] materials;

        SkeletonData data;
        Atlas atlas;
        readonly Dictionary<int, Material> runtimeMaterials = new Dictionary<int, Material>();

        public SkeletonData GetSkeletonData(bool forceReload = false)
        {
            if (data == null || forceReload)
            {
                if (skeletonJson == null) throw new System.InvalidOperationException("OsteonSkeletonDataAsset '" + name + "' has no skeleton JSON.");
                data = SkeletonData.FromJson(skeletonJson.text);
                foreach (var w in data.warnings) Debug.LogWarning("[Osteon2D] " + name + ": " + w);
            }
            return data;
        }

        public Atlas GetAtlas(bool forceReload = false)
        {
            if (atlas == null || forceReload)
            {
                if (atlasText == null) throw new System.InvalidOperationException("OsteonSkeletonDataAsset '" + name + "' has no atlas text.");
                atlas = Atlas.Parse(atlasText.text);
            }
            return atlas;
        }

        /// <summary>The material for an atlas page and blend mode; created on demand when none was assigned.</summary>
        public Material GetMaterial(int page, BlendMode blend, bool tintBlack)
        {
            int slot = page * 4 + (int)blend;
            if (!tintBlack && materials != null && slot < materials.Length && materials[slot] != null) return materials[slot];
            int key = slot * 2 + (tintBlack ? 1 : 0);
            Material m;
            if (runtimeMaterials.TryGetValue(key, out m) && m != null) return m;
            Shader shader = Shader.Find(tintBlack ? "Osteon2D/Tint Black" : "Osteon2D/Default");
            if (shader == null) shader = Shader.Find("Sprites/Default");
            m = new Material(shader) { name = name + "_page" + page + "_" + blend };
            if (pageTextures != null && page < pageTextures.Length) m.mainTexture = pageTextures[page];
            ApplyBlend(m, blend);
            m.hideFlags = HideFlags.DontSave;
            runtimeMaterials[key] = m;
            return m;
        }

        /// <summary>Sets the material's blend factors for a slot blend mode (straight alpha atlas).</summary>
        public static void ApplyBlend(Material m, BlendMode blend)
        {
            // UnityEngine.Rendering.BlendMode: One = 1, OneMinusSrcAlpha = 10, SrcAlpha = 5, DstColor = 2, OneMinusSrcColor = 6
            int src, dst;
            switch (blend)
            {
                case BlendMode.Additive: src = 5; dst = 1; break;
                case BlendMode.Multiply: src = 2; dst = 10; break;
                case BlendMode.Screen: src = 1; dst = 6; break;
                default: src = 5; dst = 10; break;
            }
            m.SetFloat("_SrcBlend", src);
            m.SetFloat("_DstBlend", dst);
        }
    }
}
