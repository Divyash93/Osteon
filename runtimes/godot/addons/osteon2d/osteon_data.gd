class_name OsteonData
extends RefCounted
## Setup-pose data and the Spine-format (4.x) JSON loader for Osteon2D skeletons.
## Written for Osteon2D from the public JSON format description; it contains no Esoteric Software code.

enum Inherit { NORMAL, ONLY_TRANSLATION, NO_ROTATION_OR_REFLECTION, NO_SCALE, NO_SCALE_OR_REFLECTION }
enum BlendMode { NORMAL, ADDITIVE, MULTIPLY, SCREEN }
enum AttachmentType { REGION, MESH, LINKED_MESH, BOUNDING_BOX, PATH, POINT, CLIPPING }
enum CurveType { LINEAR, STEPPED, BEZIER }
enum TimelineType {
	ROTATE, TRANSLATE, TRANSLATE_X, TRANSLATE_Y, SCALE, SCALE_X, SCALE_Y, SHEAR, SHEAR_X, SHEAR_Y, INHERIT,
	ATTACHMENT, RGBA, RGB, ALPHA, RGBA2, RGB2, DEFORM, SEQUENCE, DRAW_ORDER, EVENT, IK, TRANSFORM }

class BoneData:
	var name := ""
	var parent := -1
	var length := 0.0
	var x := 0.0
	var y := 0.0
	var rotation := 0.0
	var scale_x := 1.0
	var scale_y := 1.0
	var shear_x := 0.0
	var shear_y := 0.0
	var inherit := 0
	var skin_required := false

class SlotData:
	var name := ""
	var bone := 0
	var color := Color(1, 1, 1, 1)
	var has_dark := false
	var dark := Color(0, 0, 0, 1)
	var attachment := ""
	var blend := 0

class Attachment:
	var type := 0
	var name := ""
	var path := ""
	var color := Color(1, 1, 1, 1)
	var x := 0.0
	var y := 0.0
	var rotation := 0.0
	var scale_x := 1.0
	var scale_y := 1.0
	var width := 0.0
	var height := 0.0
	var bones := PackedInt32Array()  # weighted layout: per vertex [count, bone index * count]; empty = unweighted
	var vertices := PackedFloat64Array()
	var world_vertices_length := 0
	var uvs := PackedFloat64Array()
	var triangles := PackedInt32Array()
	var hull_length := 0
	var parent_mesh := ""
	var parent_skin := ""
	var inherit_timelines := true
	var end_slot := ""
	var seq_count := 0
	var seq_start := 1
	var seq_digits := 0
	var seq_setup := 0
	func is_weighted() -> bool:
		return bones.size() > 0
	func deform_length() -> int:
		return (vertices.size() / 3) * 2 if is_weighted() else vertices.size()

class SkinData:
	var name := ""
	var attachments := {}  # slot index -> { attachment name -> Attachment }
	var bones: Array[int] = []
	var constraints: Array[String] = []
	func get_attachment(slot: int, att_name: String) -> Attachment:
		if attachments.has(slot):
			return attachments[slot].get(att_name)
		return null
	func set_attachment(slot: int, att_name: String, a: Attachment) -> void:
		if not attachments.has(slot):
			attachments[slot] = {}
		attachments[slot][att_name] = a

class EventData:
	var name := ""
	var int_value := 0
	var float_value := 0.0
	var string_value := ""
	var audio_path := ""
	var volume := 1.0
	var balance := 0.0

class IkData:
	var name := ""
	var order := 0
	var skin_required := false
	var bones: Array[int] = []
	var target := 0
	var mix := 1.0
	var softness := 0.0
	var bend_direction := 1
	var compress := false
	var stretch := false
	var uniform := false

class TransformData:
	var name := ""
	var order := 0
	var skin_required := false
	var bones: Array[int] = []
	var target := 0
	var mix_rotate := 1.0
	var mix_x := 1.0
	var mix_y := 1.0
	var mix_scale_x := 1.0
	var mix_scale_y := 1.0
	var mix_shear_y := 1.0
	var offset_rotation := 0.0
	var offset_x := 0.0
	var offset_y := 0.0
	var offset_scale_x := 0.0
	var offset_scale_y := 0.0
	var offset_shear_y := 0.0
	var relative := false
	var local := false

class KeyFrame:
	var time := 0.0
	var v := PackedFloat64Array([0, 0, 0, 0, 0, 0, 0])
	var curve := 0
	var bezier := PackedFloat64Array()
	var sval := ""
	var sval2 := ""
	var deform := PackedFloat64Array()
	var draw_order := PackedInt32Array()
	var flag1 := false
	var flag2 := false
	var flag3 := false
	var int_value := 0
	var volume := 1.0
	var balance := 0.0

class Timeline:
	var type := 0
	var target := -1
	var skin := ""
	var attachment := ""
	var has_attachment := false
	var keys: Array[KeyFrame] = []

class Anim:
	var name := ""
	var timelines: Array[Timeline] = []
	var duration := 0.0

var name := "skeleton"
var version := ""
var bones: Array[BoneData] = []
var slots: Array[SlotData] = []
var skins: Array[SkinData] = []
var events: Array[EventData] = []
var animations: Array[Anim] = []
var ik: Array[IkData] = []
var transforms: Array[TransformData] = []
var warnings: Array[String] = []
var fps := 30.0
var error := ""


func find_bone(n: String) -> int:
	for i in bones.size():
		if bones[i].name == n:
			return i
	return -1

func find_slot(n: String) -> int:
	for i in slots.size():
		if slots[i].name == n:
			return i
	return -1

func find_skin(n: String) -> int:
	for i in skins.size():
		if skins[i].name == n:
			return i
	return -1

func find_animation(n: String) -> int:
	for i in animations.size():
		if animations[i].name == n:
			return i
	return -1

func find_event(n: String) -> int:
	for i in events.size():
		if events[i].name == n:
			return i
	return -1

func find_attachment(skin_index: int, slot: int, att_name: String) -> Attachment:
	if att_name == "":
		return null
	if skin_index >= 0 and skin_index < skins.size():
		var a := skins[skin_index].get_attachment(slot, att_name)
		if a != null:
			return a
	if skins.size() > 0:
		return skins[0].get_attachment(slot, att_name)
	return null


## Parses skeleton JSON text. Returns null and prints an error when the data cannot be used.
static func from_json(text: String) -> OsteonData:
	var d := OsteonData.new()
	if not d._load(text):
		push_error("Osteon2D: " + d.error)
		return null
	return d


static func _num(d: Dictionary, k: String, def: float) -> float:
	var v = d.get(k)
	return float(v) if (v is float or v is int) else def

static func _int(d: Dictionary, k: String, def: int) -> int:
	var v = d.get(k)
	return int(v) if (v is float or v is int) else def

static func _bool(d: Dictionary, k: String, def: bool) -> bool:
	var v = d.get(k)
	return v if v is bool else def

static func _str(d: Dictionary, k: String, def: String) -> String:
	var v = d.get(k)
	return v if v is String else def

static func _floats(d: Dictionary, k: String) -> PackedFloat64Array:
	var out := PackedFloat64Array()
	var v = d.get(k)
	if v is Array:
		for x in v:
			out.append(float(x))
	return out

static func _ints(d: Dictionary, k: String) -> PackedInt32Array:
	var out := PackedInt32Array()
	var v = d.get(k)
	if v is Array:
		for x in v:
			out.append(int(x))
	return out

static func _color(hex: String, fallback: String) -> Color:
	if hex == "":
		hex = fallback
	if hex.begins_with("#"):
		hex = hex.substr(1)
	if hex.length() == 6:
		hex += "ff"
	if hex.length() != 8:
		hex = fallback
	return Color(hex.substr(0, 2).hex_to_int() / 255.0, hex.substr(2, 2).hex_to_int() / 255.0,
			hex.substr(4, 2).hex_to_int() / 255.0, hex.substr(6, 2).hex_to_int() / 255.0)

static func _inherit(s: String) -> int:
	match s:
		"onlyTranslation": return Inherit.ONLY_TRANSLATION
		"noRotationOrReflection": return Inherit.NO_ROTATION_OR_REFLECTION
		"noScale": return Inherit.NO_SCALE
		"noScaleOrReflection": return Inherit.NO_SCALE_OR_REFLECTION
	return Inherit.NORMAL

static func _blend(s: String) -> int:
	match s:
		"additive": return BlendMode.ADDITIVE
		"multiply": return BlendMode.MULTIPLY
		"screen": return BlendMode.SCREEN
	return BlendMode.NORMAL


func _bone_list(cj: Dictionary, key: String) -> Array[int]:
	var r: Array[int] = []
	var arr = cj.get(key)
	if arr is Array:
		for n in arr:
			var bi := find_bone(str(n))
			if bi >= 0:
				r.append(bi)
	return r


func _load(text: String) -> bool:
	var root = JSON.parse_string(text)
	if not (root is Dictionary):
		error = "Not a skeleton JSON object"
		return false
	var sk: Dictionary = root.get("skeleton", {}) if root.get("skeleton") is Dictionary else {}
	version = _str(sk, "spine", "")
	fps = _num(sk, "fps", 30)
	if version.begins_with("3"):
		error = "Osteon2D runtimes read 4.x skeleton data; re-export this skeleton from Osteon2D."
		return false

	for bj in root.get("bones", []):
		var b := BoneData.new()
		b.name = _str(bj, "name", "")
		var parent := _str(bj, "parent", "")
		if parent != "":
			b.parent = find_bone(parent)
			if b.parent < 0:
				error = "Parent bone not found: " + parent
				return false
		b.length = _num(bj, "length", 0)
		b.x = _num(bj, "x", 0)
		b.y = _num(bj, "y", 0)
		b.rotation = _num(bj, "rotation", 0)
		b.scale_x = _num(bj, "scaleX", 1)
		b.scale_y = _num(bj, "scaleY", 1)
		b.shear_x = _num(bj, "shearX", 0)
		b.shear_y = _num(bj, "shearY", 0)
		b.inherit = _inherit(_str(bj, "inherit", _str(bj, "transform", "normal")))
		b.skin_required = _bool(bj, "skin", false)
		bones.append(b)
	if bones.is_empty():
		var rb := BoneData.new()
		rb.name = "root"
		bones.append(rb)

	for sj in root.get("slots", []):
		var s := SlotData.new()
		s.name = _str(sj, "name", "")
		s.bone = find_bone(_str(sj, "bone", ""))
		if s.bone < 0:
			error = "Slot bone not found for slot " + s.name
			return false
		s.color = _color(_str(sj, "color", ""), "ffffffff")
		if sj.has("dark"):
			s.has_dark = true
			s.dark = _color(_str(sj, "dark", ""), "000000ff")
		s.attachment = _str(sj, "attachment", "")
		s.blend = _blend(_str(sj, "blend", "normal"))
		slots.append(s)

	for cj in root.get("ik", []):
		var c := IkData.new()
		c.name = _str(cj, "name", "")
		c.order = _int(cj, "order", 0)
		c.skin_required = _bool(cj, "skin", false)
		c.bones = _bone_list(cj, "bones")
		c.target = find_bone(_str(cj, "target", ""))
		c.mix = _num(cj, "mix", 1)
		c.softness = _num(cj, "softness", 0)
		c.bend_direction = 1 if _bool(cj, "bendPositive", true) else -1
		c.compress = _bool(cj, "compress", false)
		c.stretch = _bool(cj, "stretch", false)
		c.uniform = _bool(cj, "uniform", false)
		if c.bones.is_empty() or c.target < 0:
			warnings.append("IK constraint ignored: " + c.name)
			continue
		ik.append(c)
	for cj in root.get("transform", []):
		var c := TransformData.new()
		c.name = _str(cj, "name", "")
		c.order = _int(cj, "order", 0)
		c.skin_required = _bool(cj, "skin", false)
		c.bones = _bone_list(cj, "bones")
		c.target = find_bone(_str(cj, "target", ""))
		c.local = _bool(cj, "local", false)
		c.relative = _bool(cj, "relative", false) or _bool(cj, "additive", false)
		c.offset_rotation = _num(cj, "rotation", 0)
		c.offset_x = _num(cj, "x", 0)
		c.offset_y = _num(cj, "y", 0)
		c.offset_scale_x = _num(cj, "scaleX", 0)
		c.offset_scale_y = _num(cj, "scaleY", 0)
		c.offset_shear_y = _num(cj, "shearY", 0)
		c.mix_rotate = _num(cj, "mixRotate", 1)
		c.mix_x = _num(cj, "mixX", 1)
		c.mix_y = _num(cj, "mixY", c.mix_x)
		c.mix_scale_x = _num(cj, "mixScaleX", 1)
		c.mix_scale_y = _num(cj, "mixScaleY", c.mix_scale_x)
		c.mix_shear_y = _num(cj, "mixShearY", 1)
		if cj.has("properties"):
			warnings.append("Transform constraint '%s' uses 4.3 property links, which this runtime does not apply yet" % c.name)
		if c.bones.is_empty() or c.target < 0:
			warnings.append("Transform constraint ignored: " + c.name)
			continue
		transforms.append(c)
	for k in ["path", "physics", "slider"]:
		if root.get(k) is Array and root[k].size() > 0:
			warnings.append("'%s' constraints are not supported by this runtime yet; their bones keep the animated pose" % k)

	var skins_json = root.get("skins")
	if skins_json is Array:
		for sj in skins_json:
			var skin := SkinData.new()
			skin.name = _str(sj, "name", "")
			skin.bones.assign(_bone_list(sj, "bones"))
			for key in ["ik", "transform", "path", "physics", "slider", "constraints"]:
				if sj.get(key) is Array:
					for n in sj[key]:
						skin.constraints.append(str(n))
			_read_skin_attachments(sj.get("attachments", {}), skin)
			skins.append(skin)
	var def := find_skin("default")
	if def > 0:
		var s0 := skins[def]
		skins.remove_at(def)
		skins.insert(0, s0)
	if skins.is_empty():
		var ds := SkinData.new()
		ds.name = "default"
		skins.append(ds)

	var ev = root.get("events")
	if ev is Dictionary:
		for k in ev:
			var ej: Dictionary = ev[k]
			var e := EventData.new()
			e.name = k
			e.int_value = _int(ej, "int", 0)
			e.float_value = _num(ej, "float", 0)
			e.string_value = _str(ej, "string", "")
			e.audio_path = _str(ej, "audio", "")
			e.volume = _num(ej, "volume", 1)
			e.balance = _num(ej, "balance", 0)
			events.append(e)
	var anims = root.get("animations")
	if anims is Dictionary:
		for k in anims:
			animations.append(_read_animation(k, anims[k]))
	return true


func _read_skin_attachments(atts: Dictionary, skin: SkinData) -> void:
	for slot_name in atts:
		var slot := find_slot(slot_name)
		if slot < 0:
			warnings.append("Skin '%s': unknown slot %s" % [skin.name, slot_name])
			continue
		for att_name in atts[slot_name]:
			var a := _read_attachment(atts[slot_name][att_name], att_name)
			if a != null:
				a.name = att_name
				skin.set_attachment(slot, att_name, a)


func _read_vertices(j: Dictionary, a: Attachment, length: int) -> void:
	var v := _floats(j, "vertices")
	a.world_vertices_length = length
	if v.size() == length:
		a.vertices = v
		a.bones = PackedInt32Array()
		return
	var bone_list := PackedInt32Array()
	var verts := PackedFloat64Array()
	var i := 0
	while i < v.size():
		var count := int(v[i])
		i += 1
		bone_list.append(count)
		for k in count:
			var b := int(v[i])
			if b < 0 or b >= bones.size():
				warnings.append("Weighted vertex references a missing bone")
				b = 0
			bone_list.append(b)
			verts.append(v[i + 1])
			verts.append(v[i + 2])
			verts.append(v[i + 3])
			i += 4
	a.bones = bone_list
	a.vertices = verts


func _read_attachment(j: Dictionary, att_name: String) -> Attachment:
	var a := Attachment.new()
	a.name = _str(j, "name", att_name)
	a.path = _str(j, "path", "")
	a.color = _color(_str(j, "color", ""), "ffffffff")
	if j.get("sequence") is Dictionary:
		var s: Dictionary = j["sequence"]
		a.seq_count = _int(s, "count", 0)
		a.seq_start = _int(s, "start", 1)
		a.seq_digits = _int(s, "digits", 0)
		a.seq_setup = _int(s, "setup", 0)
	match _str(j, "type", "region"):
		"region":
			a.type = AttachmentType.REGION
			a.x = _num(j, "x", 0)
			a.y = _num(j, "y", 0)
			a.scale_x = _num(j, "scaleX", 1)
			a.scale_y = _num(j, "scaleY", 1)
			a.rotation = _num(j, "rotation", 0)
			a.width = _num(j, "width", 32)
			a.height = _num(j, "height", 32)
		"mesh":
			a.type = AttachmentType.MESH
			a.uvs = _floats(j, "uvs")
			a.triangles = _ints(j, "triangles")
			_read_vertices(j, a, a.uvs.size())
			a.hull_length = _int(j, "hull", 0)
			a.width = _num(j, "width", 0)
			a.height = _num(j, "height", 0)
		"linkedmesh":
			a.type = AttachmentType.LINKED_MESH
			a.parent_mesh = _str(j, "parent", "")
			a.parent_skin = _str(j, "skin", "")
			a.inherit_timelines = _bool(j, "timelines", _bool(j, "deform", true))
			a.width = _num(j, "width", 0)
			a.height = _num(j, "height", 0)
		"boundingbox":
			a.type = AttachmentType.BOUNDING_BOX
			_read_vertices(j, a, _int(j, "vertexCount", 0) * 2)
		"path":
			a.type = AttachmentType.PATH
			_read_vertices(j, a, _int(j, "vertexCount", 0) * 2)
		"point":
			a.type = AttachmentType.POINT
			a.x = _num(j, "x", 0)
			a.y = _num(j, "y", 0)
			a.rotation = _num(j, "rotation", 0)
		"clipping":
			a.type = AttachmentType.CLIPPING
			a.end_slot = _str(j, "end", "")
			_read_vertices(j, a, _int(j, "vertexCount", 0) * 2)
		var other:
			warnings.append("Unknown attachment type " + other)
			return null
	return a


# ---- animations ----

static func _read_curve(kj: Dictionary, key: KeyFrame) -> void:
	var c = kj.get("curve")
	if c is String:
		if c == "stepped":
			key.curve = CurveType.STEPPED
	elif c is Array:
		key.curve = CurveType.BEZIER
		key.bezier = PackedFloat64Array()
		for x in c:
			key.bezier.append(float(x))

static func _curve_timeline(type: int, target: int, arr: Array, fields: Array, defs: Array) -> Timeline:
	var tl := Timeline.new()
	tl.type = type
	tl.target = target
	for kj in arr:
		var k := KeyFrame.new()
		k.time = _num(kj, "time", 0)
		for f in fields.size():
			k.v[f] = _num(kj, fields[f], defs[f])
		_read_curve(kj, k)
		tl.keys.append(k)
	return tl

static func _color_timeline(type: int, target: int, arr: Array) -> Timeline:
	var alpha := type == TimelineType.RGBA or type == TimelineType.RGBA2
	var two := type == TimelineType.RGBA2 or type == TimelineType.RGB2
	var tl := Timeline.new()
	tl.type = type
	tl.target = target
	for kj in arr:
		var k := KeyFrame.new()
		k.time = _num(kj, "time", 0)
		var light := _color(_str(kj, "light" if two else "color", ""), "ffffffff")
		k.v[0] = light.r
		k.v[1] = light.g
		k.v[2] = light.b
		var o := 3
		if alpha:
			k.v[3] = light.a
			o = 4
		if two:
			var dark := _color(_str(kj, "dark", ""), "000000ff")
			k.v[o] = dark.r
			k.v[o + 1] = dark.g
			k.v[o + 2] = dark.b
		_read_curve(kj, k)
		tl.keys.append(k)
	return tl


func _read_animation(anim_name: String, aj: Dictionary) -> Anim:
	var list: Array[Timeline] = []
	var bones_json = aj.get("bones")
	if bones_json is Dictionary:
		for bone_name in bones_json:
			var bone := find_bone(bone_name)
			if bone < 0:
				warnings.append("Animation %s: unknown bone %s" % [anim_name, bone_name])
				continue
			var tls: Dictionary = bones_json[bone_name]
			for tn in tls:
				var arr: Array = tls[tn]
				match tn:
					"rotate": list.append(_curve_timeline(TimelineType.ROTATE, bone, arr, ["value"], [0.0]))
					"translate": list.append(_curve_timeline(TimelineType.TRANSLATE, bone, arr, ["x", "y"], [0.0, 0.0]))
					"translatex": list.append(_curve_timeline(TimelineType.TRANSLATE_X, bone, arr, ["value"], [0.0]))
					"translatey": list.append(_curve_timeline(TimelineType.TRANSLATE_Y, bone, arr, ["value"], [0.0]))
					"scale": list.append(_curve_timeline(TimelineType.SCALE, bone, arr, ["x", "y"], [1.0, 1.0]))
					"scalex": list.append(_curve_timeline(TimelineType.SCALE_X, bone, arr, ["value"], [1.0]))
					"scaley": list.append(_curve_timeline(TimelineType.SCALE_Y, bone, arr, ["value"], [1.0]))
					"shear": list.append(_curve_timeline(TimelineType.SHEAR, bone, arr, ["x", "y"], [0.0, 0.0]))
					"shearx": list.append(_curve_timeline(TimelineType.SHEAR_X, bone, arr, ["value"], [0.0]))
					"sheary": list.append(_curve_timeline(TimelineType.SHEAR_Y, bone, arr, ["value"], [0.0]))
					"inherit":
						var tl := Timeline.new()
						tl.type = TimelineType.INHERIT
						tl.target = bone
						for kj in arr:
							var k := KeyFrame.new()
							k.time = _num(kj, "time", 0)
							k.int_value = _inherit(_str(kj, "inherit", "normal"))
							tl.keys.append(k)
						list.append(tl)
					_: warnings.append("Unsupported bone timeline " + tn)
	var slots_json = aj.get("slots")
	if slots_json is Dictionary:
		for slot_name in slots_json:
			var slot := find_slot(slot_name)
			if slot < 0:
				warnings.append("Animation %s: unknown slot %s" % [anim_name, slot_name])
				continue
			var tls: Dictionary = slots_json[slot_name]
			for tn in tls:
				var arr: Array = tls[tn]
				match tn:
					"attachment":
						var tl := Timeline.new()
						tl.type = TimelineType.ATTACHMENT
						tl.target = slot
						for kj in arr:
							var k := KeyFrame.new()
							k.time = _num(kj, "time", 0)
							k.sval = _str(kj, "name", "")
							tl.keys.append(k)
						list.append(tl)
					"rgba": list.append(_color_timeline(TimelineType.RGBA, slot, arr))
					"rgb": list.append(_color_timeline(TimelineType.RGB, slot, arr))
					"alpha": list.append(_curve_timeline(TimelineType.ALPHA, slot, arr, ["value"], [1.0]))
					"rgba2": list.append(_color_timeline(TimelineType.RGBA2, slot, arr))
					"rgb2": list.append(_color_timeline(TimelineType.RGB2, slot, arr))
					"sequence":
						var tl := Timeline.new()
						tl.type = TimelineType.SEQUENCE
						tl.target = slot
						for kj in arr:
							var k := KeyFrame.new()
							k.time = _num(kj, "time", 0)
							k.int_value = _int(kj, "index", 0)
							k.sval = _str(kj, "mode", "hold")
							k.v[0] = _num(kj, "delay", 0)
							tl.keys.append(k)
						list.append(tl)
					_: warnings.append("Unsupported slot timeline " + tn)
	if aj.get("ik") is Dictionary:
		for cn in aj["ik"]:
			var idx := -1
			for i in ik.size():
				if ik[i].name == cn:
					idx = i
			if idx < 0:
				continue
			var arr: Array = aj["ik"][cn]
			var tl := _curve_timeline(TimelineType.IK, idx, arr, ["mix", "softness"], [1.0, 0.0])
			for i in arr.size():
				tl.keys[i].flag1 = _bool(arr[i], "bendPositive", true)
				tl.keys[i].flag2 = _bool(arr[i], "compress", false)
				tl.keys[i].flag3 = _bool(arr[i], "stretch", false)
			list.append(tl)
	if aj.get("transform") is Dictionary:
		for cn in aj["transform"]:
			var idx := -1
			for i in transforms.size():
				if transforms[i].name == cn:
					idx = i
			if idx < 0:
				continue
			var arr: Array = aj["transform"][cn]
			var tl := _curve_timeline(TimelineType.TRANSFORM, idx, arr, ["mixRotate", "mixX", "mixY", "mixScaleX", "mixScaleY", "mixShearY"], [1.0, 1.0, 1.0, 1.0, 1.0, 1.0])
			for i in arr.size():
				if not arr[i].has("mixY"):
					tl.keys[i].v[2] = tl.keys[i].v[1]
				if not arr[i].has("mixScaleY"):
					tl.keys[i].v[4] = tl.keys[i].v[3]
			list.append(tl)
	# Deform lives under "attachments" (4.2) or "deform" (4.0/4.1): skin -> slot -> attachment.
	for dk in ["attachments", "deform"]:
		var dm = aj.get(dk)
		if not (dm is Dictionary):
			continue
		for skin_name in dm:
			var si := find_skin(skin_name)
			if si < 0:
				continue
			for slot_name in dm[skin_name]:
				var slot := find_slot(slot_name)
				if slot < 0:
					continue
				for att_name in dm[skin_name][slot_name]:
					var arr_obj = dm[skin_name][slot_name][att_name]
					if dk == "attachments":
						if not (arr_obj is Dictionary) or not arr_obj.has("deform"):
							continue
						arr_obj = arr_obj["deform"]
					if not (arr_obj is Array):
						continue
					var att := skins[si].get_attachment(slot, att_name)
					if att == null:
						continue
					var geom := att
					if att.type == AttachmentType.LINKED_MESH:
						var ps := 0 if att.parent_skin == "" else find_skin(att.parent_skin)
						geom = find_attachment(ps, slot, att.parent_mesh)
						if geom == null:
							continue
					var length := geom.deform_length()
					var tl := Timeline.new()
					tl.type = TimelineType.DEFORM
					tl.target = slot
					tl.skin = skin_name
					tl.attachment = att_name
					tl.has_attachment = true
					for kj in arr_obj:
						var k := KeyFrame.new()
						k.time = _num(kj, "time", 0)
						k.deform = PackedFloat64Array()
						k.deform.resize(length)
						var v := _floats(kj, "vertices")
						var off := maxi(0, _int(kj, "offset", 0))
						for n in v.size():
							if off + n < length:
								k.deform[off + n] = v[n]
						_read_curve(kj, k)
						tl.keys.append(k)
					list.append(tl)
	if aj.get("drawOrder") is Array:
		var n := slots.size()
		var tl := Timeline.new()
		tl.type = TimelineType.DRAW_ORDER
		for kj in aj["drawOrder"]:
			var k := KeyFrame.new()
			k.time = _num(kj, "time", 0)
			if kj.get("offsets") is Array:
				var order: Array[int] = []
				order.resize(n)
				order.fill(-1)
				var unchanged: Array[int] = []
				var orig := 0
				for oj in kj["offsets"]:
					var si := find_slot(_str(oj, "slot", ""))
					if si < 0:
						continue
					while orig != si and orig < n:
						unchanged.append(orig)
						orig += 1
					var pos := orig + _int(oj, "offset", 0)
					if pos >= 0 and pos < n:
						order[pos] = orig
					orig += 1
				while orig < n:
					unchanged.append(orig)
					orig += 1
				for i in range(n - 1, -1, -1):
					if order[i] == -1 and unchanged.size() > 0:
						order[i] = unchanged.pop_back()
				if not order.has(-1):
					k.draw_order = PackedInt32Array(order)
			tl.keys.append(k)
		list.append(tl)
	if aj.get("events") is Array:
		var tl := Timeline.new()
		tl.type = TimelineType.EVENT
		for kj in aj["events"]:
			var k := KeyFrame.new()
			k.time = _num(kj, "time", 0)
			k.sval = _str(kj, "name", "")
			if kj.has("int"):
				k.flag1 = true
				k.int_value = _int(kj, "int", 0)
			if kj.has("float"):
				k.flag2 = true
				k.v[0] = _num(kj, "float", 0)
			if kj.has("string"):
				k.flag3 = true
				k.sval2 = _str(kj, "string", "")
			k.volume = _num(kj, "volume", 1)
			k.balance = _num(kj, "balance", 0)
			tl.keys.append(k)
		list.append(tl)
	var anim := Anim.new()
	anim.name = anim_name
	anim.timelines = list
	for tl in anim.timelines:
		tl.keys.sort_custom(func(a: KeyFrame, b: KeyFrame) -> bool: return a.time < b.time)
		for k in tl.keys:
			anim.duration = maxf(anim.duration, k.time)
	return anim
