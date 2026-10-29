// Attachment types and their world-vertex computation.
import { Color, cosDeg, sinDeg } from './math.js';

let nextVertexId = 0;

export class Attachment {
  constructor(name, type) {
    this.name = name;
    this.type = type;
  }
}

export class RegionAttachment extends Attachment {
  constructor(name, path = name) {
    super(name, 'region');
    this.path = path;
    this.x = 0; this.y = 0; this.rotation = 0;
    this.scaleX = 1; this.scaleY = 1;
    this.width = 0; this.height = 0;
    this.color = new Color();
  }
  /** Four corner positions in bone-local space: BL, TL, TR, BR (Spine order). */
  localCorners() {
    const hw = (this.width / 2) * this.scaleX, hh = (this.height / 2) * this.scaleY;
    const cos = cosDeg(this.rotation), sin = sinDeg(this.rotation);
    const pts = [[-hw, -hh], [-hw, hh], [hw, hh], [hw, -hh]];
    const out = new Float32Array(8);
    pts.forEach(([x, y], i) => {
      out[i * 2] = x * cos - y * sin + this.x;
      out[i * 2 + 1] = x * sin + y * cos + this.y;
    });
    return out;
  }
  computeWorldVertices(slot, out = new Float32Array(8)) {
    const local = this.localCorners();
    const b = slot.bone;
    for (let i = 0; i < 8; i += 2) {
      const x = local[i], y = local[i + 1];
      out[i] = x * b.a + y * b.b + b.worldX;
      out[i + 1] = x * b.c + y * b.d + b.worldY;
    }
    return out;
  }
  /** UVs for BL, TL, TR, BR with v down (texture space). */
  get uvs() { return new Float32Array([0, 1, 0, 0, 1, 0, 1, 1]); }
  get triangles() { return [0, 1, 2, 2, 3, 0]; }
}

/**
 * Base for attachments defined by vertices. `bones` is null for unweighted vertices
 * ([x,y,...] in bone-local space); otherwise `bones` is the Spine-format list
 * [count, boneIndex...] per vertex and `vertices` holds [x,y,weight] per influence.
 */
export class VertexAttachment extends Attachment {
  constructor(name, type) {
    super(name, type);
    this.id = nextVertexId++;
    this.bones = null;
    this.vertices = [];
    this.worldVerticesLength = 0;
    this.timelineAttachment = this;
  }

  /** Writes world positions for all vertices. `deform` (optional) offsets local/weighted positions. */
  computeWorldVertices(slot, out, deform = slot.deform) {
    const count = this.worldVerticesLength;
    out = out || new Float32Array(count);
    const skeleton = slot.bone.skeleton;
    const hasDeform = deform && deform.length > 0;
    if (!this.bones) {
      const v = hasDeform ? deform : this.vertices;
      const b = slot.bone;
      for (let i = 0; i < count; i += 2) {
        const x = v[i], y = v[i + 1];
        out[i] = x * b.a + y * b.b + b.worldX;
        out[i + 1] = x * b.c + y * b.d + b.worldY;
      }
      return out;
    }
    const bones = this.bones, verts = this.vertices, sb = skeleton.bones;
    let vi = 0, bi = 0, fi = 0;
    for (let w = 0; w < count; w += 2) {
      let wx = 0, wy = 0;
      const n = bones[bi++];
      for (let k = 0; k < n; k++, vi += 3, fi += 2) {
        const bone = sb[bones[bi++]];
        let vx = verts[vi], vy = verts[vi + 1];
        if (hasDeform) { vx += deform[fi]; vy += deform[fi + 1]; }
        const weight = verts[vi + 2];
        wx += (vx * bone.a + vy * bone.b + bone.worldX) * weight;
        wy += (vx * bone.c + vy * bone.d + bone.worldY) * weight;
      }
      out[w] = wx; out[w + 1] = wy;
    }
    return out;
  }

  /** Length of a deform array for this attachment (one x,y per vertex influence when weighted). */
  get deformLength() {
    return this.bones ? (this.vertices.length / 3) * 2 : this.vertices.length;
  }

  get vertexCount() { return this.worldVerticesLength / 2; }

  copyVerticesTo(other) {
    other.bones = this.bones ? this.bones.slice() : null;
    other.vertices = this.vertices.slice();
    other.worldVerticesLength = this.worldVerticesLength;
  }
}

export class MeshAttachment extends VertexAttachment {
  constructor(name, path = name) {
    super(name, 'mesh');
    this.path = path;
    this.regionUVs = []; // normalised 0..1, v down
    this.triangles = [];
    this.hullLength = 0; // in floats (vertex count * 2)
    this.edges = [];
    this.width = 0; this.height = 0;
    this.color = new Color();
    this.parentMesh = null; // for linked meshes
    this.inheritTimelines = true;
  }
  get uvs() { return this.regionUVs; }
}

export class BoundingBoxAttachment extends VertexAttachment {
  constructor(name) { super(name, 'boundingbox'); this.color = new Color(0.38, 0.94, 0, 1); }
}

export class ClippingAttachment extends VertexAttachment {
  constructor(name) { super(name, 'clipping'); this.endSlot = null; this.color = new Color(0.8, 0.2, 0.2, 1); }
}

export class PathAttachment extends VertexAttachment {
  constructor(name) {
    super(name, 'path');
    this.lengths = [];
    this.closed = false;
    this.constantSpeed = true;
    this.color = new Color(1, 0.5, 0, 1);
  }
}

export class PointAttachment extends Attachment {
  constructor(name) {
    super(name, 'point');
    this.x = 0; this.y = 0; this.rotation = 0;
    this.color = new Color(0.94, 0.94, 0, 1);
  }
  computeWorldPosition(bone) {
    return { x: this.x * bone.a + this.y * bone.b + bone.worldX, y: this.x * bone.c + this.y * bone.d + bone.worldY };
  }
  computeWorldRotation(bone) {
    const cos = cosDeg(this.rotation), sin = sinDeg(this.rotation);
    const x = cos * bone.a + sin * bone.b, y = cos * bone.c + sin * bone.d;
    return Math.atan2(y, x) * (180 / Math.PI);
  }
}

/** Builds the unweighted vertex list for a mesh covering a whole image as a quad. */
export function meshFromRegion(region, name = region.name) {
  const mesh = new MeshAttachment(name, region.path);
  const corners = region.localCorners(); // BL, TL, TR, BR
  mesh.vertices = Array.from(corners);
  mesh.worldVerticesLength = 8;
  mesh.regionUVs = [0, 1, 0, 0, 1, 0, 1, 1];
  mesh.triangles = [0, 1, 2, 2, 3, 0];
  mesh.hullLength = 8;
  mesh.edges = [0, 2, 2, 4, 4, 6, 6, 0];
  mesh.width = region.width; mesh.height = region.height;
  mesh.color = region.color.clone();
  return mesh;
}
