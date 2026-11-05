class_name OsteonSkeleton
extends RefCounted
## Runtime pose: world transforms, IK and transform constraints, slot state.

const DEG_RAD := PI / 180.0
const RAD_DEG := 180.0 / PI

class Bone:
	var index := 0
	var parent := -1
	var children: Array[int] = []
	var x := 0.0
	var y := 0.0
	var rotation := 0.0
	var scale_x := 1.0
	var scale_y := 1.0
	var shear_x := 0.0
	var shear_y := 0.0
	var inherit := 0
	var ax := 0.0
	var ay := 0.0
	var arotation := 0.0
	var ascale_x := 1.0
	var ascale_y := 1.0
	var ashear_x := 0.0
	var ashear_y := 0.0
	var a := 1.0
	var b := 0.0
	var c := 0.0
	var d := 1.0
	var world_x := 0.0
	var world_y := 0.0
	var sorted := false
	var active := true
	func world_rotation_x() -> float:
		return rad_to_deg(atan2(c, a))
	func world_rotation_y() -> float:
		return rad_to_deg(atan2(d, b))

class Slot:
	var index := 0
	var bone := 0
	var color := Color(1, 1, 1, 1)
	var dark := Color(0, 0, 0, 1)
	var has_dark := false
	var attachment := ""
	var deform := PackedFloat64Array()
	var deform_for := ""
	var sequence_index := -1

class IkState:
	var mix := 1.0
	var softness := 0.0
	var bend_direction := 1
	var compress := false
	var stretch := false
	var active := true

class TransformState:
	var mix_rotate := 1.0
	var mix_x := 1.0
	var mix_y := 1.0
	var mix_scale_x := 1.0
	var mix_scale_y := 1.0
	var mix_shear_y := 1.0
	var active := true

const CACHE_BONE := 0
const CACHE_IK := 1
const CACHE_TRANSFORM := 2

var data: OsteonData
var bones: Array[Bone] = []
var slots: Array[Slot] = []
var draw_order := PackedInt32Array()
var ik: Array[IkState] = []
var transforms: Array[TransformState] = []
var skin := 0
var x := 0.0
var y := 0.0
var scale_x := 1.0
var scale_y := 1.0
var color := Color(1, 1, 1, 1)
var _root_a := 1.0
var _root_d := 1.0
var _cache: Array = []  # [type, index]


func _init(skeleton_data: OsteonData) -> void:
	data = skeleton_data
	for i in data.bones.size():
		var b := Bone.new()
		b.index = i
		b.parent = data.bones[i].parent
		bones.append(b)
	for i in bones.size():
		if bones[i].parent >= 0:
			bones[bones[i].parent].children.append(i)
	for i in data.slots.size():
		var s := Slot.new()
		s.index = i
		s.bone = data.slots[i].bone
		slots.append(s)
	draw_order.resize(slots.size())
	for i in data.ik.size():
		ik.append(IkState.new())
	for i in data.transforms.size():
		transforms.append(TransformState.new())
	update_cache()
	set_to_setup_pose()


func set_skin_by_name(skin_name: String) -> bool:
	var i := data.find_skin(skin_name)
	if i < 0:
		push_error("Osteon2D: skin not found: " + skin_name)
		return false
	skin = i
	update_cache()
	set_slots_to_setup_pose()
	return true


# ---- constraint / bone ordering ----

func update_cache() -> void:
	_cache.clear()
	var active_skin: OsteonData.SkinData = data.skins[skin] if skin >= 0 and skin < data.skins.size() else null
	for b in bones:
		var needs := data.bones[b.index].skin_required
		b.active = (not needs) or (active_skin != null and active_skin.bones.has(b.index))
		if b.parent >= 0 and not bones[b.parent].active:
			b.active = false
		b.sorted = not b.active

	var sort_bone := func(i: int, self_ref: Callable) -> void:
		if i < 0:
			return
		var b: Bone = bones[i]
		if b.sorted:
			return
		if b.parent >= 0:
			self_ref.call(b.parent, self_ref)
		b.sorted = true
		_cache.append([CACHE_BONE, i])
	var sort_reset := func(list: Array[int], self_ref: Callable) -> void:
		for i in list:
			var b: Bone = bones[i]
			if not b.active:
				continue
			if b.sorted:
				self_ref.call(b.children, self_ref)
			b.sorted = false

	var order: Array = []
	for i in data.ik.size():
		order.append([data.ik[i].order, CACHE_IK, i])
	for i in data.transforms.size():
		order.append([data.transforms[i].order, CACHE_TRANSFORM, i])
	# stable insertion sort by order
	var sorted_list: Array = []
	for item in order:
		var pos := sorted_list.size()
		while pos > 0 and sorted_list[pos - 1][0] > item[0]:
			pos -= 1
		sorted_list.insert(pos, item)

	for item in sorted_list:
		var type: int = item[1]
		var idx: int = item[2]
		if type == CACHE_IK:
			var cd := data.ik[idx]
			var act := bones[cd.target].active and ((not cd.skin_required) or (active_skin != null and active_skin.constraints.has(cd.name)))
			for b in cd.bones:
				act = act and bones[b].active
			ik[idx].active = act
			if not act:
				continue
			sort_bone.call(cd.target, sort_bone)
			var parent := cd.bones[0]
			sort_bone.call(parent, sort_bone)
			if cd.bones.size() == 1:
				_cache.append([CACHE_IK, idx])
				sort_reset.call(bones[parent].children, sort_reset)
			else:
				var child := cd.bones[cd.bones.size() - 1]
				sort_bone.call(child, sort_bone)
				_cache.append([CACHE_IK, idx])
				sort_reset.call(bones[parent].children, sort_reset)
				bones[child].sorted = true
		else:
			var cd := data.transforms[idx]
			var act := bones[cd.target].active and ((not cd.skin_required) or (active_skin != null and active_skin.constraints.has(cd.name)))
			for b in cd.bones:
				act = act and bones[b].active
			transforms[idx].active = act
			if not act:
				continue
			sort_bone.call(cd.target, sort_bone)
			for b in cd.bones:
				if cd.local:
					sort_bone.call(bones[b].parent, sort_bone)
				sort_bone.call(b, sort_bone)
			_cache.append([CACHE_TRANSFORM, idx])
			for b in cd.bones:
				sort_reset.call(bones[b].children, sort_reset)
			for b in cd.bones:
				bones[b].sorted = true
	for i in bones.size():
		sort_bone.call(i, sort_bone)


# ---- setup pose ----

func set_to_setup_pose() -> void:
	set_bones_to_setup_pose()
	set_slots_to_setup_pose()

func set_bones_to_setup_pose() -> void:
	for b in bones:
		var bd := data.bones[b.index]
		b.x = bd.x
		b.y = bd.y
		b.rotation = bd.rotation
		b.scale_x = bd.scale_x
		b.scale_y = bd.scale_y
		b.shear_x = bd.shear_x
		b.shear_y = bd.shear_y
		b.inherit = bd.inherit
	for i in ik.size():
		var c := data.ik[i]
		ik[i].mix = c.mix
		ik[i].softness = c.softness
		ik[i].bend_direction = c.bend_direction
		ik[i].compress = c.compress
		ik[i].stretch = c.stretch
	for i in transforms.size():
		var c := data.transforms[i]
		var t := transforms[i]
		t.mix_rotate = c.mix_rotate
		t.mix_x = c.mix_x
		t.mix_y = c.mix_y
		t.mix_scale_x = c.mix_scale_x
		t.mix_scale_y = c.mix_scale_y
		t.mix_shear_y = c.mix_shear_y

func set_slots_to_setup_pose() -> void:
	for i in slots.size():
		var s := slots[i]
		var sd := data.slots[i]
		draw_order[i] = i
		s.color = sd.color
		s.has_dark = sd.has_dark
		s.dark = sd.dark
		if s.attachment != sd.attachment:
			s.deform = PackedFloat64Array()
			s.deform_for = ""
		s.attachment = sd.attachment
		s.sequence_index = -1


# ---- world transforms ----

static func wrap_deg(d: float) -> float:
	d = fmod(d, 360.0)
	if d > 180.0:
		d -= 360.0
	elif d <= -180.0:
		d += 360.0
	return d

static func wrap_rad(r: float) -> float:
	if r > PI:
		return r - TAU
	if r < -PI:
		return r + TAU
	return r

static func _atan2_deg(yy: float, xx: float) -> float:
	return atan2(yy, xx) * RAD_DEG


func update_world_transform() -> void:
	_root_a = scale_x
	_root_d = scale_y
	for b in bones:
		b.ax = b.x
		b.ay = b.y
		b.arotation = b.rotation
		b.ascale_x = b.scale_x
		b.ascale_y = b.scale_y
		b.ashear_x = b.shear_x
		b.ashear_y = b.shear_y
	for item in _cache:
		match item[0]:
			CACHE_BONE:
				var b: Bone = bones[item[1]]
				update_bone(b, b.x, b.y, b.rotation, b.scale_x, b.scale_y, b.shear_x, b.shear_y)
			CACHE_IK:
				_apply_ik(item[1])
			CACHE_TRANSFORM:
				_apply_transform(item[1])


## Parent matrix as [a, b, c, d, worldX, worldY]; the root's parent is the skeleton scale and offset.
func _parent_matrix(bone: Bone) -> Array:
	if bone.parent < 0:
		return [_root_a, 0.0, 0.0, _root_d, x, y]
	var p := bones[bone.parent]
	return [p.a, p.b, p.c, p.d, p.world_x, p.world_y]


func update_bone(bone: Bone, x_: float, y_: float, rot: float, sx: float, sy: float, shx: float, shy: float) -> void:
	bone.ax = x_
	bone.ay = y_
	bone.arotation = rot
	bone.ascale_x = sx
	bone.ascale_y = sy
	bone.ashear_x = shx
	bone.ashear_y = shy
	var sk_x := scale_x
	var sk_y := scale_y
	if bone.parent < 0:
		var rx0 := (rot + shx) * DEG_RAD
		var ry0 := (rot + 90.0 + shy) * DEG_RAD
		bone.a = cos(rx0) * sx * sk_x
		bone.b = cos(ry0) * sy * sk_x
		bone.c = sin(rx0) * sx * sk_y
		bone.d = sin(ry0) * sy * sk_y
		bone.world_x = x_ * sk_x + x
		bone.world_y = y_ * sk_y + y
		return
	var p := bones[bone.parent]
	var pa := p.a
	var pb := p.b
	var pc := p.c
	var pd := p.d
	bone.world_x = pa * x_ + pb * y_ + p.world_x
	bone.world_y = pc * x_ + pd * y_ + p.world_y
	match bone.inherit:
		OsteonData.Inherit.NORMAL:
			var rx := (rot + shx) * DEG_RAD
			var ry := (rot + 90.0 + shy) * DEG_RAD
			var la := cos(rx) * sx
			var lb := cos(ry) * sy
			var lc := sin(rx) * sx
			var ld := sin(ry) * sy
			bone.a = pa * la + pb * lc
			bone.b = pa * lb + pb * ld
			bone.c = pc * la + pd * lc
			bone.d = pc * lb + pd * ld
			return
		OsteonData.Inherit.ONLY_TRANSLATION:
			var rx := (rot + shx) * DEG_RAD
			var ry := (rot + 90.0 + shy) * DEG_RAD
			bone.a = cos(rx) * sx
			bone.b = cos(ry) * sy
			bone.c = sin(rx) * sx
			bone.d = sin(ry) * sy
		OsteonData.Inherit.NO_ROTATION_OR_REFLECTION:
			var isx := 1.0 / sk_x
			var isy := 1.0 / sk_y
			pa *= isx
			pc *= isy
			var s := pa * pa + pc * pc
			var prx: float
			if s > 0.0001:
				s = absf(pa * pd * isy - pb * isx * pc) / s
				pb = pc * s
				pd = pa * s
				prx = _atan2_deg(pc, pa)
			else:
				pa = 0.0
				pc = 0.0
				prx = 90.0 - _atan2_deg(pd, pb)
			var rx := (rot + shx - prx) * DEG_RAD
			var ry := (rot + shy - prx + 90.0) * DEG_RAD
			var la := cos(rx) * sx
			var lb := cos(ry) * sy
			var lc := sin(rx) * sx
			var ld := sin(ry) * sy
			bone.a = pa * la - pb * lc
			bone.b = pa * lb - pb * ld
			bone.c = pc * la + pd * lc
			bone.d = pc * lb + pd * ld
		_:  # NO_SCALE, NO_SCALE_OR_REFLECTION
			var r := rot * DEG_RAD
			var cs := cos(r)
			var sn := sin(r)
			var za := (pa * cs + pb * sn) / sk_x
			var zc := (pc * cs + pd * sn) / sk_y
			var s := sqrt(za * za + zc * zc)
			if s > 0.00001:
				s = 1.0 / s
			za *= s
			zc *= s
			s = sqrt(za * za + zc * zc)
			if bone.inherit == OsteonData.Inherit.NO_SCALE and ((pa * pd - pb * pc < 0.0) != ((sk_x < 0.0) != (sk_y < 0.0))):
				s = -s
			r = PI / 2.0 + atan2(zc, za)
			var zb := cos(r) * s
			var zd := sin(r) * s
			var la := cos(deg_to_rad(shx)) * sx
			var lb := cos(deg_to_rad(90.0 + shy)) * sy
			var lc := sin(deg_to_rad(shx)) * sx
			var ld := sin(deg_to_rad(90.0 + shy)) * sy
			bone.a = za * la + zb * lc
			bone.b = za * lb + zb * ld
			bone.c = zc * la + zd * lc
			bone.d = zc * lb + zd * ld
	bone.a *= sk_x
	bone.b *= sk_x
	bone.c *= sk_y
	bone.d *= sk_y


## Returns [rotation, scaleX, scaleY, shearY] of the matrix.
static func _decompose(ra: float, rb: float, rc: float, rd: float) -> Array:
	var sx := sqrt(ra * ra + rc * rc)
	var det := ra * rd - rb * rc
	var ly := sqrt(rb * rb + rd * rd)
	if sx > 0.0001:
		var rot := _atan2_deg(rc, ra)
		var sy := -ly if det < 0.0 else ly
		var y_angle := _atan2_deg(-rd, -rb) if det < 0.0 else _atan2_deg(rd, rb)
		var shy := wrap_deg(y_angle - rot - 90.0) if ly > 0.0001 else 0.0
		return [rot, sx, sy, shy]
	return [90.0 - _atan2_deg(rd, rb), 0.0, ly, 0.0]


func _mul_inv(bone: Bone, ma: float, mb: float, mc: float, md: float) -> Array:
	var det := ma * md - mb * mc
	var id := 0.0 if absf(det) < 1e-12 else 1.0 / det
	var ia := md * id
	var ib := -mb * id
	var ic := -mc * id
	var idd := ma * id
	return [ia * bone.a + ib * bone.c, ia * bone.b + ib * bone.d, ic * bone.a + idd * bone.c, ic * bone.b + idd * bone.d]


## Recomputes the applied local pose from the bone's current world transform.
func update_applied_transform(bone: Bone) -> void:
	var pm := _parent_matrix(bone)
	var pa: float = pm[0]
	var pb: float = pm[1]
	var pc: float = pm[2]
	var pd: float = pm[3]
	var pid := pa * pd - pb * pc
	pid = 0.0 if absf(pid) < 1e-12 else 1.0 / pid
	var dx: float = bone.world_x - pm[4]
	var dy: float = bone.world_y - pm[5]
	bone.ax = (dx * pd - dy * pb) * pid
	bone.ay = (dy * pa - dx * pc) * pid
	var sk_x := scale_x
	var sk_y := scale_y
	bone.ashear_x = 0.0
	var mode: int = OsteonData.Inherit.NORMAL if bone.parent < 0 else bone.inherit
	var r: Array
	match mode:
		OsteonData.Inherit.NORMAL:
			r = _decompose_into(bone, _mul_inv(bone, pa, pb, pc, pd))
			return
		OsteonData.Inherit.ONLY_TRANSLATION:
			r = _decompose_into(bone, _mul_inv(bone, sk_x, 0.0, 0.0, sk_y))
			return
		OsteonData.Inherit.NO_ROTATION_OR_REFLECTION:
			var isx := 1.0 / sk_x
			var isy := 1.0 / sk_y
			var qa := pa * isx
			var qc := pc * isy
			var qb := pb
			var qd := pd
			var s := qa * qa + qc * qc
			var prx: float
			if s > 0.0001:
				s = absf(qa * pd * isy - pb * isx * qc) / s
				qb = qc * s
				qd = qa * s
				prx = _atan2_deg(qc, qa)
			else:
				qa = 0.0
				qc = 0.0
				prx = 90.0 - _atan2_deg(pd, pb)
			_decompose_into(bone, _mul_inv(bone, sk_x * qa, -sk_x * qb, sk_y * qc, sk_y * qd))
			bone.arotation += prx
			return
		_:
			var wa := bone.a / sk_x
			var wc := bone.c / sk_y
			var theta := atan2(wc, wa)
			var ia := pd
			var ib := -pb
			var ic := -pc
			var id := pa
			var dirx := cos(theta) * sk_x
			var diry := sin(theta) * sk_y
			var lx := ia * dirx + ib * diry
			var ly := ic * dirx + id * diry
			if pa * pd - pb * pc < 0.0:
				lx = -lx
				ly = -ly
			var rr := atan2(ly, lx)
			var cs := cos(rr)
			var sn := sin(rr)
			var za := (pa * cs + pb * sn) / sk_x
			var zc := (pc * cs + pd * sn) / sk_y
			var zl := sqrt(za * za + zc * zc)
			if zl > 0.00001:
				za /= zl
				zc /= zl
			var sgn := 1.0
			if bone.inherit == OsteonData.Inherit.NO_SCALE and ((pa * pd - pb * pc < 0.0) != ((sk_x < 0.0) != (sk_y < 0.0))):
				sgn = -1.0
			var zr := PI / 2.0 + atan2(zc, za)
			var zb := cos(zr) * sgn
			var zd := sin(zr) * sgn
			var m := _mul_inv(bone, sk_x * za, sk_x * zb, sk_y * zc, sk_y * zd)
			var dec := _decompose(m[0], m[1], m[2], m[3])
			bone.arotation = rr * RAD_DEG
			bone.ashear_x = dec[0]
			bone.ascale_x = dec[1]
			bone.ascale_y = dec[2]
			bone.ashear_y = dec[3]


func _decompose_into(bone: Bone, m: Array) -> Array:
	var dec := _decompose(m[0], m[1], m[2], m[3])
	bone.arotation = dec[0]
	bone.ascale_x = dec[1]
	bone.ascale_y = dec[2]
	bone.ashear_y = dec[3]
	return dec


func local_to_world(b: Bone, lx: float, ly: float) -> Vector2:
	return Vector2(lx * b.a + ly * b.b + b.world_x, lx * b.c + ly * b.d + b.world_y)


# ---- IK ----

func _apply_ik(index: int) -> void:
	var d := data.ik[index]
	var c := ik[index]
	if c.mix == 0.0:
		return
	var target := bones[d.target]
	if d.bones.size() == 1:
		_apply_ik1(bones[d.bones[0]], target.world_x, target.world_y, c.compress, c.stretch, d.uniform, c.mix)
	else:
		_apply_ik2(bones[d.bones[0]], bones[d.bones[1]], target.world_x, target.world_y, c.bend_direction, c.stretch, d.uniform, c.softness, c.mix)


func _apply_ik1(bone: Bone, target_x: float, target_y: float, compress: bool, stretch: bool, uniform: bool, alpha: float) -> void:
	var pm := _parent_matrix(bone)
	var pa: float = pm[0]
	var pb: float = pm[1]
	var pc: float = pm[2]
	var pd: float = pm[3]
	var rotation_ik := -bone.ashear_x - bone.arotation
	var tx: float
	var ty: float
	if bone.inherit == OsteonData.Inherit.ONLY_TRANSLATION:
		tx = (target_x - bone.world_x) * (-1.0 if scale_x < 0.0 else 1.0)
		ty = (target_y - bone.world_y) * (-1.0 if scale_y < 0.0 else 1.0)
	else:
		if bone.inherit == OsteonData.Inherit.NO_ROTATION_OR_REFLECTION:
			var s := absf(pa * pd - pb * pc) / maxf(0.0001, pa * pa + pc * pc)
			var sa := pa / scale_x
			var sc := pc / scale_y
			pb = -sc * s * scale_x
			pd = sa * s * scale_y
			rotation_ik += _atan2_deg(sc, sa)
		var xx: float = target_x - pm[4]
		var yy: float = target_y - pm[5]
		var dd := pa * pd - pb * pc
		if absf(dd) <= 0.0001:
			tx = 0.0
			ty = 0.0
		else:
			tx = (xx * pd - yy * pb) / dd - bone.ax
			ty = (yy * pa - xx * pc) / dd - bone.ay
	rotation_ik += _atan2_deg(ty, tx)
	if bone.ascale_x < 0.0:
		rotation_ik += 180.0
	if rotation_ik > 180.0:
		rotation_ik -= 360.0
	elif rotation_ik < -180.0:
		rotation_ik += 360.0
	var sx := bone.ascale_x
	var sy := bone.ascale_y
	if compress or stretch:
		if bone.inherit == OsteonData.Inherit.NO_SCALE or bone.inherit == OsteonData.Inherit.NO_SCALE_OR_REFLECTION:
			tx = target_x - bone.world_x
			ty = target_y - bone.world_y
		var b := data.bones[bone.index].length * sx
		if b > 0.0001:
			var dd := tx * tx + ty * ty
			if (compress and dd < b * b) or (stretch and dd > b * b):
				var s := (sqrt(dd) / b - 1.0) * alpha + 1.0
				sx *= s
				if uniform:
					sy *= s
	update_bone(bone, bone.ax, bone.ay, bone.arotation + rotation_ik * alpha, sx, sy, bone.ashear_x, bone.ashear_y)


func _apply_ik2(parent: Bone, child: Bone, target_x: float, target_y: float, bend_dir: int, stretch: bool, uniform: bool, softness: float, alpha: float) -> void:
	if parent.inherit != OsteonData.Inherit.NORMAL or child.inherit != OsteonData.Inherit.NORMAL:
		return
	var px := parent.ax
	var py := parent.ay
	var psx := parent.ascale_x
	var psy := parent.ascale_y
	var sx := psx
	var sy := psy
	var csx := child.ascale_x
	var os1: int
	var os2: int
	var s2: int
	if psx < 0.0:
		psx = -psx
		os1 = 180
		s2 = -1
	else:
		os1 = 0
		s2 = 1
	if psy < 0.0:
		psy = -psy
		s2 = -s2
	if csx < 0.0:
		csx = -csx
		os2 = 180
	else:
		os2 = 0
	var cx := child.ax
	var cy: float
	var cwx: float
	var cwy: float
	var a := parent.a
	var b := parent.b
	var c := parent.c
	var d := parent.d
	var u := absf(psx - psy) <= 0.0001
	if (not u) or stretch:
		cy = 0.0
		cwx = a * cx + parent.world_x
		cwy = c * cx + parent.world_y
	else:
		cy = child.ay
		cwx = a * cx + b * cy + parent.world_x
		cwy = c * cx + d * cy + parent.world_y
	var pp := _parent_matrix(parent)
	a = pp[0]
	b = pp[1]
	c = pp[2]
	d = pp[3]
	var id := a * d - b * c
	var x_: float = cwx - pp[4]
	var y_: float = cwy - pp[5]
	id = 0.0 if absf(id) <= 0.0001 else 1.0 / id
	var dx := (x_ * d - y_ * b) * id - px
	var dy := (y_ * a - x_ * c) * id - py
	var l1 := sqrt(dx * dx + dy * dy)
	var l2 := data.bones[child.index].length * csx
	var a1 := 0.0
	var a2 := 0.0
	if l1 < 0.0001:
		_apply_ik1(parent, target_x, target_y, false, stretch, false, alpha)
		update_bone(child, cx, cy, 0.0, child.ascale_x, child.ascale_y, child.ashear_x, child.ashear_y)
		return
	x_ = target_x - pp[4]
	y_ = target_y - pp[5]
	var tx := (x_ * d - y_ * b) * id - px
	var ty := (y_ * a - x_ * c) * id - py
	var dd := tx * tx + ty * ty
	if softness != 0.0:
		softness *= psx * (csx + 1.0) * 0.5
		var td := sqrt(dd)
		var sd := td - l1 - l2 * psy + softness
		if sd > 0.0:
			var p := minf(1.0, sd / (softness * 2.0)) - 1.0
			p = (sd - softness * (1.0 - p * p)) / td
			tx -= p * tx
			ty -= p * ty
			dd = tx * tx + ty * ty
	if u:
		l2 *= psx
		var cs := (dd - l1 * l1 - l2 * l2) / (2.0 * l1 * l2)
		if cs < -1.0:
			cs = -1.0
			a2 = PI * bend_dir
		elif cs > 1.0:
			cs = 1.0
			a2 = 0.0
			if stretch:
				a = (sqrt(dd) / (l1 + l2) - 1.0) * alpha + 1.0
				sx *= a
				if uniform:
					sy *= a
		else:
			a2 = acos(cs) * bend_dir
		a = l1 + l2 * cs
		b = l2 * sin(a2)
		a1 = atan2(ty * a - tx * b, tx * a + ty * b)
	else:
		a = psx * l2
		b = psy * l2
		var aa := a * a
		var bb := b * b
		var ta := atan2(ty, tx)
		c = bb * l1 * l1 + aa * dd - aa * bb
		var c1 := -2.0 * bb * l1
		var c2 := bb - aa
		d = c1 * c1 - 4.0 * c2 * c
		var solved := false
		if d >= 0.0:
			var q := sqrt(d)
			if c1 < 0.0:
				q = -q
			q = -(c1 + q) * 0.5
			var r0 := q / c2
			var r1 := c / q
			var r := r0 if absf(r0) < absf(r1) else r1
			r0 = dd - r * r
			if r0 >= 0.0:
				y_ = sqrt(r0) * bend_dir
				a1 = ta - atan2(y_, r)
				a2 = atan2(y_ / psy, (r - l1) / psx)
				solved = true
		if not solved:
			var min_angle := PI
			var min_x := l1 - a
			var min_dist := min_x * min_x
			var min_y := 0.0
			var max_angle := 0.0
			var max_x := l1 + a
			var max_dist := max_x * max_x
			var max_y := 0.0
			c = -a * l1 / (aa - bb)
			if c >= -1.0 and c <= 1.0:
				c = acos(c)
				x_ = a * cos(c) + l1
				y_ = b * sin(c)
				d = x_ * x_ + y_ * y_
				if d < min_dist:
					min_angle = c
					min_dist = d
					min_x = x_
					min_y = y_
				if d > max_dist:
					max_angle = c
					max_dist = d
					max_x = x_
					max_y = y_
			if dd <= (min_dist + max_dist) * 0.5:
				a1 = ta - atan2(min_y * bend_dir, min_x)
				a2 = min_angle * bend_dir
			else:
				a1 = ta - atan2(max_y * bend_dir, max_x)
				a2 = max_angle * bend_dir
	var os := atan2(cy, cx) * s2
	var rotation := parent.arotation
	a1 = (a1 - os) * RAD_DEG + os1 - rotation
	if a1 > 180.0:
		a1 -= 360.0
	elif a1 < -180.0:
		a1 += 360.0
	update_bone(parent, px, py, rotation + a1 * alpha, sx, sy, 0.0, 0.0)
	rotation = child.arotation
	a2 = ((a2 + os) * RAD_DEG - child.ashear_x) * s2 + os2 - rotation
	if a2 > 180.0:
		a2 -= 360.0
	elif a2 < -180.0:
		a2 += 360.0
	update_bone(child, cx, cy, rotation + a2 * alpha, child.ascale_x, child.ascale_y, child.ashear_x, child.ashear_y)


# ---- transform constraints ----

func _apply_transform(index: int) -> void:
	var d := data.transforms[index]
	var c := transforms[index]
	if c.mix_rotate == 0.0 and c.mix_x == 0.0 and c.mix_y == 0.0 and c.mix_scale_x == 0.0 and c.mix_scale_y == 0.0 and c.mix_shear_y == 0.0:
		return
	if d.local:
		_transform_local(d, c, d.relative)
	else:
		_transform_world(d, c, d.relative)


func _transform_world(d: OsteonData.TransformData, c: TransformState, relative: bool) -> void:
	var t := bones[d.target]
	var ta := t.a
	var tb := t.b
	var tc := t.c
	var td := t.d
	var deg_rad_reflect := DEG_RAD if ta * td - tb * tc > 0.0 else -DEG_RAD
	var offset_rotation := d.offset_rotation * deg_rad_reflect
	var offset_shear_y := d.offset_shear_y * deg_rad_reflect
	for bi in d.bones:
		var b := bones[bi]
		if c.mix_rotate != 0.0:
			var a := b.a
			var bb := b.b
			var cc := b.c
			var dd := b.d
			var r: float
			if relative:
				r = wrap_rad(atan2(tc, ta) + offset_rotation) * c.mix_rotate
			else:
				r = wrap_rad(atan2(tc, ta) - atan2(cc, a) + offset_rotation) * c.mix_rotate
			var cs := cos(r)
			var sn := sin(r)
			b.a = cs * a - sn * cc
			b.b = cs * bb - sn * dd
			b.c = sn * a + cs * cc
			b.d = sn * bb + cs * dd
		if c.mix_x != 0.0 or c.mix_y != 0.0:
			var w := local_to_world(t, d.offset_x, d.offset_y)
			if relative:
				b.world_x += w.x * c.mix_x
				b.world_y += w.y * c.mix_y
			else:
				b.world_x += (w.x - b.world_x) * c.mix_x
				b.world_y += (w.y - b.world_y) * c.mix_y
		if c.mix_scale_x != 0.0:
			var s: float
			if relative:
				s = (sqrt(ta * ta + tc * tc) - 1.0 + d.offset_scale_x) * c.mix_scale_x + 1.0
			else:
				s = sqrt(b.a * b.a + b.c * b.c)
				if s != 0.0:
					s = (s + (sqrt(ta * ta + tc * tc) - s + d.offset_scale_x) * c.mix_scale_x) / s
			b.a *= s
			b.c *= s
		if c.mix_scale_y != 0.0:
			var s: float
			if relative:
				s = (sqrt(tb * tb + td * td) - 1.0 + d.offset_scale_y) * c.mix_scale_y + 1.0
			else:
				s = sqrt(b.b * b.b + b.d * b.d)
				if s != 0.0:
					s = (s + (sqrt(tb * tb + td * td) - s + d.offset_scale_y) * c.mix_scale_y) / s
			b.b *= s
			b.d *= s
		if c.mix_shear_y > 0.0:
			var by := atan2(b.d, b.b)
			var r: float
			if relative:
				r = wrap_rad(atan2(td, tb) - atan2(tc, ta))
				r = by + (r - PI / 2.0 + offset_shear_y) * c.mix_shear_y
			else:
				r = atan2(td, tb) - atan2(tc, ta) - (by - atan2(b.c, b.a))
				r = wrap_rad(r)
				r = by + (r + offset_shear_y) * c.mix_shear_y
			var s := sqrt(b.b * b.b + b.d * b.d)
			b.b = cos(r) * s
			b.d = sin(r) * s
		update_applied_transform(b)


func _transform_local(d: OsteonData.TransformData, c: TransformState, relative: bool) -> void:
	var t := bones[d.target]
	for bi in d.bones:
		var b := bones[bi]
		var rotation := b.arotation
		var xx := b.ax
		var yy := b.ay
		var sx := b.ascale_x
		var sy := b.ascale_y
		var shy := b.ashear_y
		if relative:
			rotation += (t.arotation + d.offset_rotation) * c.mix_rotate
			xx += (t.ax + d.offset_x) * c.mix_x
			yy += (t.ay + d.offset_y) * c.mix_y
			sx *= ((t.ascale_x - 1.0 + d.offset_scale_x) * c.mix_scale_x) + 1.0
			sy *= ((t.ascale_y - 1.0 + d.offset_scale_y) * c.mix_scale_y) + 1.0
			shy += (t.ashear_y + d.offset_shear_y) * c.mix_shear_y
		else:
			if c.mix_rotate != 0.0:
				rotation += wrap_deg(t.arotation - rotation + d.offset_rotation) * c.mix_rotate
			xx += (t.ax - xx + d.offset_x) * c.mix_x
			yy += (t.ay - yy + d.offset_y) * c.mix_y
			if c.mix_scale_x != 0.0 and sx != 0.0:
				sx = (sx + (t.ascale_x - sx + d.offset_scale_x) * c.mix_scale_x) / sx * sx
			if c.mix_scale_y != 0.0 and sy != 0.0:
				sy = (sy + (t.ascale_y - sy + d.offset_scale_y) * c.mix_scale_y) / sy * sy
			if c.mix_shear_y != 0.0:
				shy += wrap_deg(t.ashear_y - shy + d.offset_shear_y) * c.mix_shear_y
		update_bone(b, xx, yy, rotation, sx, sy, b.ashear_x, shy)


# ---- attachments / geometry ----

func attachment_for(slot: Slot) -> OsteonData.Attachment:
	return data.find_attachment(skin, slot.index, slot.attachment)

## A linked mesh uses the geometry of its parent mesh.
func geometry(slot: Slot, att: OsteonData.Attachment) -> OsteonData.Attachment:
	if att.type != OsteonData.AttachmentType.LINKED_MESH:
		return att
	var ps := 0 if att.parent_skin == "" else data.find_skin(att.parent_skin)
	return data.find_attachment(ps, slot.index, att.parent_mesh)


## World vertices (x,y pairs) of a region (4 corners BL,TL,TR,BR), mesh or linked mesh. Returns an empty array if not drawable.
func world_vertices(slot: Slot, att: OsteonData.Attachment) -> PackedFloat64Array:
	var out := PackedFloat64Array()
	var bone := bones[slot.bone]
	if att.type == OsteonData.AttachmentType.REGION:
		var hw := att.width / 2.0 * att.scale_x
		var hh := att.height / 2.0 * att.scale_y
		var c := cos(deg_to_rad(att.rotation))
		var s := sin(deg_to_rad(att.rotation))
		var pts := [-hw, -hh, -hw, hh, hw, hh, hw, -hh]
		out.resize(8)
		for i in range(0, 8, 2):
			var lx: float = pts[i] * c - pts[i + 1] * s + att.x
			var ly: float = pts[i] * s + pts[i + 1] * c + att.y
			out[i] = lx * bone.a + ly * bone.b + bone.world_x
			out[i + 1] = lx * bone.c + ly * bone.d + bone.world_y
		return out
	if att.type != OsteonData.AttachmentType.MESH and att.type != OsteonData.AttachmentType.LINKED_MESH:
		return out
	var geom := geometry(slot, att)
	if geom == null:
		return out
	var use_deform := slot.deform.size() > 0 and slot.deform_for == att.name and slot.deform.size() == geom.deform_length()
	var count := geom.world_vertices_length
	out.resize(count)
	if not geom.is_weighted():
		var v := geom.vertices
		var i := 0
		while i + 1 < count and i + 1 < v.size():
			var vx := v[i]
			var vy := v[i + 1]
			if use_deform:
				vx += slot.deform[i]
				vy += slot.deform[i + 1]
			out[i] = vx * bone.a + vy * bone.b + bone.world_x
			out[i + 1] = vx * bone.c + vy * bone.d + bone.world_y
			i += 2
		return out
	var b := 0
	var vi := 0
	var fi := 0
	var w := 0
	while w < count:
		var wx := 0.0
		var wy := 0.0
		var n := geom.bones[b]
		b += 1
		for k in n:
			var bn := bones[geom.bones[b]]
			b += 1
			var vx := geom.vertices[vi]
			var vy := geom.vertices[vi + 1]
			var weight := geom.vertices[vi + 2]
			if use_deform:
				vx += slot.deform[fi]
				vy += slot.deform[fi + 1]
			wx += (vx * bn.a + vy * bn.b + bn.world_x) * weight
			wy += (vx * bn.c + vy * bn.d + bn.world_y) * weight
			vi += 3
			fi += 2
		out[w] = wx
		out[w + 1] = wy
		w += 2
	return out
