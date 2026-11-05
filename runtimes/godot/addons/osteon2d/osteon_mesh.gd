class_name OsteonMeshBuilder
extends RefCounted
## Turns a posed skeleton into batches of triangles (one batch per run of the same atlas page and blend mode), with clipping.
## Positions are in skeleton space with y UP; the sprite node flips y for Godot's y-down canvas.

class Batch:
	var page := 0
	var blend := 0
	var points := PackedVector2Array()
	var uvs := PackedVector2Array()
	var colors := PackedColorArray()
	var indices := PackedInt32Array()

var atlas: OsteonAtlas
var use_clipping := true
var batches: Array[Batch] = []

var _clip_convex: Array = []  # of PackedFloat64Array(6)
var _clip_end := ""
var _clipping := false
var _cur: Batch = null


func _init(a: OsteonAtlas) -> void:
	atlas = a


static func _frame_name(a: OsteonData.Attachment, frame: int) -> String:
	var path := a.path if a.path != "" else a.name
	if a.seq_count <= 0:
		return path
	var digits := str(a.seq_start + frame)
	while digits.length() < a.seq_digits:
		digits = "0" + digits
	return path + digits


func build(sk: OsteonSkeleton) -> void:
	batches.clear()
	_cur = null
	_clipping = false
	var data := sk.data
	for si in sk.draw_order:
		var slot := sk.slots[si]
		var att := sk.attachment_for(slot)
		if not sk.bones[slot.bone].active:
			if _clipping and slot.index == _end_index(sk):
				_clipping = false
			continue
		if att != null and att.type == OsteonData.AttachmentType.CLIPPING and use_clipping:
			_start_clip(sk, slot, att)
			continue
		if att != null and (att.type == OsteonData.AttachmentType.REGION or att.type == OsteonData.AttachmentType.MESH or att.type == OsteonData.AttachmentType.LINKED_MESH):
			_add_attachment(sk, slot, att)
		if _clipping and slot.index == _end_index(sk):
			_clipping = false


func _end_index(sk: OsteonSkeleton) -> int:
	return -1 if _clip_end == "" else sk.data.find_slot(_clip_end)


func _start_clip(sk: OsteonSkeleton, slot: OsteonSkeleton.Slot, att: OsteonData.Attachment) -> void:
	var poly := _world_polygon(sk, slot, att)
	_clip_convex = _convexes(poly)
	_clip_end = att.end_slot
	_clipping = _clip_convex.size() > 0


static func _world_polygon(sk: OsteonSkeleton, slot: OsteonSkeleton.Slot, att: OsteonData.Attachment) -> PackedFloat64Array:
	var out := PackedFloat64Array()
	var bone := sk.bones[slot.bone]
	var n := att.world_vertices_length
	out.resize(n)
	if not att.is_weighted():
		var i := 0
		while i + 1 < n:
			var vx := att.vertices[i]
			var vy := att.vertices[i + 1]
			out[i] = vx * bone.a + vy * bone.b + bone.world_x
			out[i + 1] = vx * bone.c + vy * bone.d + bone.world_y
			i += 2
		return out
	var b := 0
	var vi := 0
	var w := 0
	while w < n:
		var wx := 0.0
		var wy := 0.0
		var cnt := att.bones[b]
		b += 1
		for k in cnt:
			var bn := sk.bones[att.bones[b]]
			b += 1
			var vx := att.vertices[vi]
			var vy := att.vertices[vi + 1]
			var weight := att.vertices[vi + 2]
			wx += (vx * bn.a + vy * bn.b + bn.world_x) * weight
			wy += (vx * bn.c + vy * bn.d + bn.world_y) * weight
			vi += 3
		out[w] = wx
		out[w + 1] = wy
		w += 2
	return out


func _add_attachment(sk: OsteonSkeleton, slot: OsteonSkeleton.Slot, att: OsteonData.Attachment) -> void:
	var sd := sk.data.slots[slot.index]
	var frame := slot.sequence_index if slot.sequence_index >= 0 else att.seq_setup
	var region := atlas.find(_frame_name(att, frame))
	if region == null:
		return
	var page := atlas.pages[region.page]
	var vcount: int
	var src_uv: PackedFloat64Array
	var tris: PackedInt32Array
	if att.type == OsteonData.AttachmentType.REGION:
		vcount = 4
		tris = PackedInt32Array([0, 1, 2, 2, 3, 0])
		src_uv = PackedFloat64Array([0, 1, 0, 0, 1, 0, 1, 1])
	else:
		var geom := sk.geometry(slot, att)
		if geom == null or geom.uvs.is_empty():
			return
		vcount = geom.world_vertices_length / 2
		src_uv = geom.uvs
		tris = geom.triangles
	var world := sk.world_vertices(slot, att)
	if world.size() < vcount * 2:
		return
	var col := Color(sk.color.r * slot.color.r * att.color.r, sk.color.g * slot.color.g * att.color.g,
			sk.color.b * slot.color.b * att.color.b, sk.color.a * slot.color.a * att.color.a)
	var uv := PackedVector2Array()
	uv.resize(vcount)
	for i in vcount:
		uv[i] = region.page_uv(page, src_uv[i * 2], src_uv[i * 2 + 1])  # page v points down, like Godot textures
	var batch := _batch_for(region.page, sd.blend)
	if _clipping:
		var t := 0
		while t + 2 < tris.size():
			_clip_triangle(batch, world, uv, tris[t], tris[t + 1], tris[t + 2], col)
			t += 3
	else:
		var base := batch.points.size()
		for i in vcount:
			batch.points.append(Vector2(world[i * 2], world[i * 2 + 1]))
			batch.uvs.append(uv[i])
			batch.colors.append(col)
		for t in tris:
			batch.indices.append(base + t)


func _batch_for(page: int, blend: int) -> Batch:
	if _cur != null and _cur.page == page and _cur.blend == blend:
		return _cur
	_cur = Batch.new()
	_cur.page = page
	_cur.blend = blend
	batches.append(_cur)
	return _cur


# ---- clipping (Sutherland-Hodgman against the convex pieces of the clip polygon) ----

static func _area(p: PackedFloat64Array) -> float:
	var a := 0.0
	var n := p.size() / 2
	for i in n:
		var j := (i + 1) % n
		a += p[i * 2] * p[j * 2 + 1] - p[j * 2] * p[i * 2 + 1]
	return a / 2.0


static func _point_in_tri(px: float, py: float, ax: float, ay: float, bx: float, by: float, cx: float, cy: float) -> bool:
	var d1 := (px - bx) * (ay - by) - (ax - bx) * (py - by)
	var d2 := (px - cx) * (by - cy) - (bx - cx) * (py - cy)
	var d3 := (px - ax) * (cy - ay) - (cx - ax) * (py - ay)
	var neg := d1 < 0.0 or d2 < 0.0 or d3 < 0.0
	var pos := d1 > 0.0 or d2 > 0.0 or d3 > 0.0
	return not (neg and pos)


## Ear clipping into counter-clockwise triangles; every triangle is convex.
static func _convexes(poly: PackedFloat64Array) -> Array:
	var result: Array = []
	var n := poly.size() / 2
	if n < 3:
		return result
	var idx: Array[int] = []
	for i in n:
		idx.append(i)
	if _area(poly) < 0.0:
		idx.reverse()
	var guard := 0
	while idx.size() > 3 and guard < 10000:
		guard += 1
		var clipped := false
		for i in idx.size():
			var i0: int = idx[(i + idx.size() - 1) % idx.size()]
			var i1: int = idx[i]
			var i2: int = idx[(i + 1) % idx.size()]
			var ax := poly[i0 * 2]
			var ay := poly[i0 * 2 + 1]
			var bx := poly[i1 * 2]
			var by := poly[i1 * 2 + 1]
			var cx := poly[i2 * 2]
			var cy := poly[i2 * 2 + 1]
			if (bx - ax) * (cy - ay) - (by - ay) * (cx - ax) <= 0.0:
				continue  # reflex
			var inside := false
			for j in idx:
				if j == i0 or j == i1 or j == i2:
					continue
				if _point_in_tri(poly[j * 2], poly[j * 2 + 1], ax, ay, bx, by, cx, cy):
					inside = true
					break
			if inside:
				continue
			result.append(PackedFloat64Array([ax, ay, bx, by, cx, cy]))
			idx.remove_at(i)
			clipped = true
			break
		if not clipped:
			break
	if idx.size() == 3:
		var i0: int = idx[0]
		var i1: int = idx[1]
		var i2: int = idx[2]
		result.append(PackedFloat64Array([poly[i0 * 2], poly[i0 * 2 + 1], poly[i1 * 2], poly[i1 * 2 + 1], poly[i2 * 2], poly[i2 * 2 + 1]]))
	return result


static func _side(ax: float, ay: float, bx: float, by: float, px: float, py: float) -> float:
	return (bx - ax) * (py - ay) - (by - ay) * (px - ax)


## A polygon vertex being clipped: [x, y, u, v]
static func _lerp_v(a: Array, b: Array, t: float) -> Array:
	return [a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t, a[3] + (b[3] - a[3]) * t]


func _clip_triangle(batch: Batch, w: PackedFloat64Array, uv: PackedVector2Array, i0: int, i1: int, i2: int, col: Color) -> void:
	for convex in _clip_convex:
		var poly: Array = [
			[w[i0 * 2], w[i0 * 2 + 1], uv[i0].x, uv[i0].y],
			[w[i1 * 2], w[i1 * 2 + 1], uv[i1].x, uv[i1].y],
			[w[i2 * 2], w[i2 * 2 + 1], uv[i2].x, uv[i2].y]]
		for e in 3:
			if poly.is_empty():
				break
			var ex: float = convex[e * 2]
			var ey: float = convex[e * 2 + 1]
			var fx: float = convex[((e + 1) % 3) * 2]
			var fy: float = convex[((e + 1) % 3) * 2 + 1]
			var next: Array = []
			for i in poly.size():
				var cur: Array = poly[i]
				var prev: Array = poly[(i + poly.size() - 1) % poly.size()]
				var sc := _side(ex, ey, fx, fy, cur[0], cur[1])
				var sp := _side(ex, ey, fx, fy, prev[0], prev[1])
				if sc >= 0.0:
					if sp < 0.0:
						next.append(_lerp_v(prev, cur, sp / (sp - sc)))
					next.append(cur)
				elif sp >= 0.0:
					next.append(_lerp_v(prev, cur, sp / (sp - sc)))
			poly = next
		if poly.size() < 3:
			continue
		var base := batch.points.size()
		for p in poly:
			batch.points.append(Vector2(p[0], p[1]))
			batch.uvs.append(Vector2(p[2], p[3]))
			batch.colors.append(col)
		for k in range(1, poly.size() - 1):
			batch.indices.append(base)
			batch.indices.append(base + k)
			batch.indices.append(base + k + 1)
