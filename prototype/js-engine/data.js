// Setup-pose data. This is the document's source of truth; runtime Skeleton instances read it.
import { Color } from './math.js';

export const Inherit = Object.freeze({
  normal: 'normal',
  onlyTranslation: 'onlyTranslation',
  noRotationOrReflection: 'noRotationOrReflection',
  noScale: 'noScale',
  noScaleOrReflection: 'noScaleOrReflection',
});

export const BlendMode = Object.freeze({ normal: 'normal', additive: 'additive', multiply: 'multiply', screen: 'screen' });

export class BoneData {
  constructor(index, name, parent = null) {
    this.index = index;
    this.name = name;
    this.parent = parent;
    this.length = 0;
    this.x = 0; this.y = 0; this.rotation = 0;
    this.scaleX = 1; this.scaleY = 1; this.shearX = 0; this.shearY = 0;
    this.inherit = Inherit.normal;
    this.skinRequired = false;
    this.color = new Color(0.61, 0.61, 0.61, 1);
    this.icon = '';
    this.visible = true; // editor-only
  }
}

export class SlotData {
  constructor(index, name, boneData) {
    this.index = index;
    this.name = name;
    this.boneData = boneData;
    this.color = new Color();
    this.darkColor = null; // Color or null (tint black)
    this.attachmentName = null;
    this.blendMode = BlendMode.normal;
    this.visible = true; // editor-only
  }
}

export class Skin {
  constructor(name) {
    this.name = name;
    /** Map<slotIndex, Map<attachmentName, Attachment>> */
    this.attachments = new Map();
    this.bones = [];
    this.constraints = [];
  }
  setAttachment(slotIndex, name, attachment) {
    if (!this.attachments.has(slotIndex)) this.attachments.set(slotIndex, new Map());
    this.attachments.get(slotIndex).set(name, attachment);
  }
  getAttachment(slotIndex, name) {
    const m = this.attachments.get(slotIndex);
    return m ? m.get(name) || null : null;
  }
  removeAttachment(slotIndex, name) {
    const m = this.attachments.get(slotIndex);
    if (m) m.delete(name);
  }
  /** Iterates [slotIndex, name, attachment]. */
  *entries() {
    for (const [slotIndex, m] of this.attachments) for (const [name, att] of m) yield [slotIndex, name, att];
  }
}

export class EventData {
  constructor(name) {
    this.name = name;
    this.intValue = 0; this.floatValue = 0; this.stringValue = '';
    this.audioPath = ''; this.volume = 1; this.balance = 0;
  }
}

export class IkConstraintData {
  constructor(name) {
    this.name = name;
    this.order = 0;
    this.skinRequired = false;
    this.bones = []; // BoneData, 1 or 2
    this.target = null;
    this.mix = 1;
    this.softness = 0;
    this.bendDirection = 1;
    this.compress = false;
    this.stretch = false;
    this.uniform = false;
  }
}

export class TransformConstraintData {
  constructor(name) {
    this.name = name;
    this.order = 0;
    this.skinRequired = false;
    this.bones = [];
    this.target = null;
    this.mixRotate = 1; this.mixX = 1; this.mixY = 1;
    this.mixScaleX = 1; this.mixScaleY = 1; this.mixShearY = 1;
    this.offsetRotation = 0; this.offsetX = 0; this.offsetY = 0;
    this.offsetScaleX = 0; this.offsetScaleY = 0; this.offsetShearY = 0;
    this.relative = false;
    this.local = false;
  }
}

export class SkeletonData {
  constructor(name = 'skeleton') {
    this.name = name;
    this.bones = [];
    this.slots = [];
    this.skins = [];
    this.defaultSkin = null;
    this.events = [];
    this.animations = [];
    this.ikConstraints = [];
    this.transformConstraints = [];
    this.x = 0; this.y = 0; this.width = 0; this.height = 0;
    this.imagesPath = '';
    this.audioPath = '';
    this.fps = 30;
    this.hash = '';
    this.version = '4.2.00';
  }
  findBone(name) { return this.bones.find((b) => b.name === name) || null; }
  findSlot(name) { return this.slots.find((s) => s.name === name) || null; }
  findSkin(name) { return this.skins.find((s) => s.name === name) || null; }
  findEvent(name) { return this.events.find((e) => e.name === name) || null; }
  findAnimation(name) { return this.animations.find((a) => a.name === name) || null; }
  findIk(name) { return this.ikConstraints.find((c) => c.name === name) || null; }
  findTransform(name) { return this.transformConstraints.find((c) => c.name === name) || null; }

  /** Ensures the default skin exists and returns it. */
  getDefaultSkin() {
    if (!this.defaultSkin) {
      this.defaultSkin = new Skin('default');
      this.skins.unshift(this.defaultSkin);
    }
    return this.defaultSkin;
  }

  /** Re-numbers bone and slot indices after structural edits. */
  reindex() {
    this.bones.forEach((b, i) => (b.index = i));
    this.slots.forEach((s, i) => (s.index = i));
  }

  /** Children of a bone in data order. */
  childrenOf(boneData) { return this.bones.filter((b) => b.parent === boneData); }

  /** True if `bone` is `ancestor` or a descendant of it. */
  isDescendant(bone, ancestor) {
    for (let b = bone; b; b = b.parent) if (b === ancestor) return true;
    return false;
  }

  uniqueName(base, list) {
    const names = new Set(list.map((x) => x.name));
    if (!names.has(base)) return base;
    for (let i = 2; ; i++) if (!names.has(`${base}${i}`)) return `${base}${i}`;
  }
}
