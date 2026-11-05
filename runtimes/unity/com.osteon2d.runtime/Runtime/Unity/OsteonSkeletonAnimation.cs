using System;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.Rendering;

namespace Osteon2D
{
    /// <summary>
    /// Draws and animates an Osteon2D skeleton with a MeshRenderer. Add it to a GameObject, assign a Skeleton Data asset and an
    /// initial animation, or drive it from code through <see cref="State"/> and <see cref="Skeleton"/>.
    /// </summary>
    [ExecuteAlways]
    [DisallowMultipleComponent]
    [RequireComponent(typeof(MeshFilter), typeof(MeshRenderer))]
    [AddComponentMenu("Osteon2D/Skeleton Animation")]
    public class OsteonSkeletonAnimation : MonoBehaviour
    {
        public enum UpdateMode { Update, LateUpdate, FixedUpdate, Manual }

        [Header("Skeleton")]
        public OsteonSkeletonDataAsset skeletonDataAsset;
        public string initialSkin = "default";
        public string animationName;
        public bool loop = true;
        public float timeScale = 1f;
        [Tooltip("When playing in the editor outside play mode the pose is built from the setup pose or the chosen animation at time 0.")]
        public bool previewInEditMode = true;

        [Header("Rendering")]
        public UpdateMode updateMode = UpdateMode.Update;
        public bool tintBlack = false;
        public bool clipping = true;
        public bool flipX, flipY;
        [Tooltip("Sorting layer / order of the MeshRenderer.")]
        public string sortingLayerName = "Default";
        public int sortingOrder = 0;

        /// <summary>Fired for every animation event the playhead passes: (track index, event).</summary>
        public event Action<int, FiredEvent> AnimationEvent;
        /// <summary>Called after the pose and world transforms were updated, before the mesh is built. Use it to move bones.</summary>
        public event Action<OsteonSkeletonAnimation> UpdateWorld;
        /// <summary>Called after the mesh was rebuilt.</summary>
        public event Action<OsteonSkeletonAnimation> MeshRebuilt;

        public Skeleton Skeleton { get; private set; }
        public AnimationState State { get; private set; }
        public SkeletonData Data { get { return Skeleton != null ? Skeleton.data : null; } }

        Mesh mesh;
        MeshFilter meshFilter;
        MeshRenderer meshRenderer;
        MeshBuilder builder;
        readonly List<Vector3> verts = new List<Vector3>();
        readonly List<Vector2> uvList = new List<Vector2>();
        readonly List<Vector2> dark1 = new List<Vector2>();
        readonly List<Vector2> dark2 = new List<Vector2>();
        readonly List<Color32> cols = new List<Color32>();
        readonly List<int> tris = new List<int>();
        Material[] materialCache = new Material[0];

        void OnEnable() { Initialize(false); }
        void Start() { if (Application.isPlaying && Skeleton == null) Initialize(true); }

        void OnDestroy()
        {
            if (mesh != null) { if (Application.isPlaying) Destroy(mesh); else DestroyImmediate(mesh); }
        }

        /// <summary>(Re)builds the skeleton, animation state and mesh from the data asset.</summary>
        public void Initialize(bool overwrite)
        {
            if (!overwrite && Skeleton != null) return;
            if (skeletonDataAsset == null || skeletonDataAsset.skeletonJson == null || skeletonDataAsset.atlasText == null) return;
            try
            {
                var data = skeletonDataAsset.GetSkeletonData();
                Skeleton = new Skeleton(data) { scaleX = flipX ? -1 : 1, scaleY = flipY ? -1 : 1 };
                if (!string.IsNullOrEmpty(initialSkin) && data.FindSkin(initialSkin) >= 0) Skeleton.SetSkin(initialSkin);
                State = new AnimationState(data) { defaultMix = skeletonDataAsset.defaultMix };
                State.onEvent = (track, e) => { if (AnimationEvent != null) AnimationEvent(track, e); };
                if (!string.IsNullOrEmpty(animationName) && data.FindAnimation(animationName) >= 0) State.SetAnimation(0, animationName, loop);
                builder = new MeshBuilder(skeletonDataAsset.GetAtlas()) { tintBlack = tintBlack, useClipping = clipping };
                meshFilter = GetComponent<MeshFilter>();
                meshRenderer = GetComponent<MeshRenderer>();
                if (mesh == null) { mesh = new Mesh { name = "Osteon2D Mesh" }; mesh.MarkDynamic(); mesh.indexFormat = IndexFormat.UInt32; }
                meshFilter.sharedMesh = mesh;
                meshRenderer.sortingLayerName = sortingLayerName;
                meshRenderer.sortingOrder = sortingOrder;
                Skeleton.SetToSetupPose();
                if (State.tracks.Count > 0) { State.Update(0); State.Apply(Skeleton); }
                Skeleton.UpdateWorldTransform();
                RebuildMesh();
            }
            catch (Exception e)
            {
                Debug.LogError("[Osteon2D] Could not load '" + skeletonDataAsset.name + "': " + e.Message, this);
                Skeleton = null;
            }
        }

        void Update()
        {
            if (!Application.isPlaying) { if (previewInEditMode && Skeleton == null) Initialize(true); return; }
            if (updateMode == UpdateMode.Update) Advance(Time.deltaTime);
        }
        void LateUpdate() { if (Application.isPlaying && updateMode == UpdateMode.LateUpdate) Advance(Time.deltaTime); }
        void FixedUpdate() { if (Application.isPlaying && updateMode == UpdateMode.FixedUpdate) Advance(Time.fixedDeltaTime); }

        /// <summary>Advances the animation by <paramref name="delta"/> seconds and refreshes the mesh.</summary>
        public void Advance(float delta)
        {
            if (Skeleton == null) { Initialize(false); if (Skeleton == null) return; }
            delta *= timeScale;
            State.Update(delta);
            Skeleton.SetBonesToSetupPose();
            Skeleton.SetSlotsToSetupPose();
            State.Apply(Skeleton);
            foreach (var kv in attachmentOverrides) Skeleton.slots[kv.Key].attachment = kv.Value;
            Skeleton.UpdateWorldTransform();
            if (UpdateWorld != null) UpdateWorld(this);
            RebuildMesh();
        }

        /// <summary>Plays <paramref name="name"/> on a track, crossfading from what was playing there.</summary>
        public AnimationState.Entry Play(string name, bool loopAnimation = true, int track = 0)
        {
            if (Skeleton == null) Initialize(true);
            return State.SetAnimation(track, name, loopAnimation);
        }

        public void SetSkin(string name) { if (Skeleton != null) { Skeleton.SetSkin(name); } }
        /// <summary>Forces a slot's attachment (null or empty clears the override and lets the animation decide again).</summary>
        public void SetAttachment(string slotName, string attachmentName)
        {
            int i = Skeleton.data.FindSlot(slotName);
            if (i < 0) throw new ArgumentException("Slot not found: " + slotName);
            if (string.IsNullOrEmpty(attachmentName)) attachmentOverrides.Remove(i); else attachmentOverrides[i] = attachmentName;
        }
        readonly Dictionary<int, string> attachmentOverrides = new Dictionary<int, string>();

        /// <summary>Re-reads flip and scale options (call after changing flipX / flipY).</summary>
        public void ApplyFlip() { if (Skeleton != null) { Skeleton.scaleX = flipX ? -1 : 1; Skeleton.scaleY = flipY ? -1 : 1; } }

        /// <summary>Position of a bone's origin in world space.</summary>
        public Vector3 BoneWorldPosition(string boneName)
        {
            int i = Skeleton.data.FindBone(boneName);
            if (i < 0) throw new ArgumentException("Bone not found: " + boneName);
            var b = Skeleton.bones[i];
            float s = skeletonDataAsset.scale;
            return transform.TransformPoint(new Vector3(b.worldX * s, b.worldY * s, 0));
        }

        public void RebuildMesh()
        {
            if (builder == null || mesh == null) return;
            builder.tintBlack = tintBlack; builder.useClipping = clipping;
            builder.Build(Skeleton);
            float s = skeletonDataAsset.scale;
            int vc = builder.vertexCount;
            verts.Clear(); uvList.Clear(); cols.Clear(); dark1.Clear(); dark2.Clear();
            for (int i = 0; i < vc; i++)
            {
                verts.Add(new Vector3(builder.positions[i * 2] * s, builder.positions[i * 2 + 1] * s, 0f));
                uvList.Add(new Vector2(builder.uvs[i * 2], builder.uvs[i * 2 + 1]));
                var c = builder.colors[i];
                cols.Add(new Color32(ToByte(c.r), ToByte(c.g), ToByte(c.b), ToByte(c.a)));
                if (tintBlack) { var d = builder.darks[i]; dark1.Add(new Vector2(d.r, d.g)); dark2.Add(new Vector2(d.b, 1f)); }
            }
            mesh.Clear();
            mesh.SetVertices(verts);
            mesh.SetUVs(0, uvList);
            mesh.SetColors(cols);
            if (tintBlack) { mesh.SetUVs(1, dark1); mesh.SetUVs(2, dark2); }
            int sc = Math.Max(1, builder.subMeshes.Count);
            mesh.subMeshCount = sc;
            if (materialCache.Length != sc) materialCache = new Material[sc];
            for (int m = 0; m < builder.subMeshes.Count; m++)
            {
                var sm = builder.subMeshes[m];
                tris.Clear();
                for (int k = 0; k < sm.indexCount; k++) tris.Add(builder.indices[sm.startIndex + k]);
                mesh.SetTriangles(tris, m, false);
                materialCache[m] = skeletonDataAsset.GetMaterial(sm.page, sm.blend, tintBlack);
            }
            if (builder.subMeshes.Count == 0) { mesh.SetTriangles(new List<int>(), 0, false); materialCache[0] = skeletonDataAsset.GetMaterial(0, BlendMode.Normal, tintBlack); }
            mesh.RecalculateBounds();
            meshRenderer.sharedMaterials = materialCache;
            if (MeshRebuilt != null) MeshRebuilt(this);
        }

        static byte ToByte(float v) { return (byte)Mathf.Clamp(Mathf.RoundToInt(v * 255f), 0, 255); }
    }
}
