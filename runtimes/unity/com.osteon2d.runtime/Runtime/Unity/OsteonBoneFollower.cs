using UnityEngine;

namespace Osteon2D
{
    /// <summary>Makes this transform follow a bone of an <see cref="OsteonSkeletonAnimation"/> (weapon in hand, particle on a hip ...).</summary>
    [ExecuteAlways]
    [AddComponentMenu("Osteon2D/Bone Follower")]
    public class OsteonBoneFollower : MonoBehaviour
    {
        public OsteonSkeletonAnimation skeletonAnimation;
        public string boneName;
        public bool followPosition = true;
        public bool followRotation = true;
        public bool followScale = false;
        public bool followZPosition = false;
        [Tooltip("Offset in the bone's own space, in skeleton units.")]
        public Vector2 localOffset;
        [Tooltip("Extra rotation in degrees.")]
        public float rotationOffset;

        int boneIndex = -1;
        OsteonSkeletonAnimation subscribed;

        void OnEnable() { Subscribe(); Apply(); }
        void OnDisable() { if (subscribed != null) subscribed.UpdateWorld -= OnUpdateWorld; subscribed = null; }
        void LateUpdate() { if (subscribed != skeletonAnimation) Subscribe(); if (!Application.isPlaying) Apply(); }

        void Subscribe()
        {
            if (subscribed != null) subscribed.UpdateWorld -= OnUpdateWorld;
            subscribed = skeletonAnimation;
            boneIndex = -1;
            if (subscribed != null) subscribed.UpdateWorld += OnUpdateWorld;
        }

        void OnUpdateWorld(OsteonSkeletonAnimation anim) { Apply(); }

        public void Apply()
        {
            if (skeletonAnimation == null || skeletonAnimation.Skeleton == null) return;
            var sk = skeletonAnimation.Skeleton;
            if (boneIndex < 0 || boneIndex >= sk.bones.Length || sk.data.bones[boneIndex].name != boneName)
            {
                boneIndex = sk.data.FindBone(boneName);
                if (boneIndex < 0) return;
            }
            var b = sk.bones[boneIndex];
            float s = skeletonAnimation.skeletonDataAsset.scale;
            var host = skeletonAnimation.transform;
            if (followPosition)
            {
                float wx, wy;
                sk.LocalToWorld(b, localOffset.x, localOffset.y, out wx, out wy);
                var p = host.TransformPoint(new Vector3(wx * s, wy * s, 0));
                if (!followZPosition) p.z = transform.position.z;
                transform.position = p;
            }
            if (followRotation)
            {
                float hostAngle = host.eulerAngles.z;
                float ang = b.WorldRotationX + rotationOffset;
                if (skeletonAnimation.flipX != skeletonAnimation.flipY) ang = -ang;
                transform.rotation = Quaternion.Euler(0, 0, hostAngle + ang);
            }
            if (followScale)
            {
                float sx = Mathf.Sqrt(b.a * b.a + b.c * b.c), sy = Mathf.Sqrt(b.b * b.b + b.d * b.d);
                transform.localScale = new Vector3(sx, sy, 1);
            }
        }
    }
}
