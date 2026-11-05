class_name OsteonAnim
extends RefCounted
## Applies animation timelines to a skeleton pose.

enum MixBlend { SETUP, FIRST, REPLACE, ADD }

## An event the playhead passed. `event_index` is -1 when the name is not declared in the skeleton.
class FiredEvent:
	var event_index := -1
	var name := ""
	var time := 0.0
	var int_value := 0
	var float_value := 0.0
	var string_value := ""
	var volume := 1.0
	var balance := 0.0


static func key_before(keys: Array[OsteonData.KeyFrame], time: float) -> int:
	var i := -1
	for k in keys.size():
		if keys[k].time <= time:
			i = k
		else:
			break
	return i


static func bezier_value(time: float, t0: float, v0: float, cx1: float, cy1: float, cx2: float, cy2: float, t1: float, v1: float) -> float:
	if time <= t0:
		return v0
	if time >= t1:
		return v1
	var s := (time - t0) / (t1 - t0)
	for i in 8:
		var u := 1.0 - s
		var d := 3.0 * u * u * (cx1 - t0) + 6.0 * u * s * (cx2 - cx1) + 3.0 * s * s * (t1 - cx2)
		if absf(d) < 1e-9:
			break
		var bx0 := u * u * u * t0 + 3.0 * u * u * s * cx1 + 3.0 * u * s * s * cx2 + s * s * s * t1
		var ns := s - (bx0 - time) / d
		if ns < 0.0 or ns > 1.0:
			break
		s = ns
	var u2 := 1.0 - s
	var bxs := u2 * u2 * u2 * t0 + 3.0 * u2 * u2 * s * cx1 + 3.0 * u2 * s * s * cx2 + s * s * s * t1
	if absf(bxs - time) > 1e-6:
		var lo := 0.0
		var hi := 1.0
		for i in 50:
			s = (lo + hi) / 2.0
			var uu := 1.0 - s
			var bx := uu * uu * uu * t0 + 3.0 * uu * uu * s * cx1 + 3.0 * uu * s * s * cx2 + s * s * s * t1
			if bx < time:
				lo = s
			else:
				hi = s
	var w := 1.0 - s
	return w * w * w * v0 + 3.0 * w * w * s * cy1 + 3.0 * w * s * s * cy2 + s * s * s * v1


static func value(tl: OsteonData.Timeline, vi: int, time: float) -> float:
	var keys := tl.keys
	if keys.is_empty():
		return 0.0
	var i := key_before(keys, time)
	if i < 0:
		return keys[0].v[vi]
	var k0 := keys[i]
	if i + 1 >= keys.size():
		return k0.v[vi]
	var k1 := keys[i + 1]
	if k0.curve == OsteonData.CurveType.STEPPED:
		return k0.v[vi]
	if k0.curve == OsteonData.CurveType.BEZIER and k0.bezier.size() >= vi * 4 + 4:
		var o := vi * 4
		return bezier_value(time, k0.time, k0.v[vi], k0.bezier[o], k0.bezier[o + 1], k0.bezier[o + 2], k0.bezier[o + 3], k1.time, k1.v[vi])
	var span := k1.time - k0.time
	var t := (time - k0.time) / span if span > 0.0 else 0.0
	return k0.v[vi] + (k1.v[vi] - k0.v[vi]) * t


static func _curve_percent(tl: OsteonData.Timeline, i: int, time: float) -> float:
	var k0 := tl.keys[i]
	var k1 := tl.keys[i + 1]
	if k0.curve == OsteonData.CurveType.STEPPED:
		return 0.0
	var span := k1.time - k0.time
	if span <= 0.0:
		return 0.0
	if k0.curve == OsteonData.CurveType.BEZIER and k0.bezier.size() >= 4:
		return bezier_value(time, k0.time, 0.0, k0.bezier[0], k0.bezier[1], k0.bezier[2], k0.bezier[3], k1.time, 1.0)
	return (time - k0.time) / span


static func _apply_rotate(tl: OsteonData.Timeline, sk: OsteonSkeleton, time: float, alpha: float, blend: int) -> void:
	var bone := sk.bones[tl.target]
	if not bone.active:
		return
	var bd := sk.data.bones[tl.target]
	if time < tl.keys[0].time:
		if blend == MixBlend.SETUP:
			bone.rotation = bd.rotation
		elif blend == MixBlend.FIRST:
			bone.rotation += (bd.rotation - bone.rotation) * alpha
		return
	var r := value(tl, 0, time)
	match blend:
		MixBlend.SETUP: bone.rotation = bd.rotation + r * alpha
		MixBlend.FIRST, MixBlend.REPLACE:
			r += bd.rotation - bone.rotation
			bone.rotation += r * alpha
		MixBlend.ADD: bone.rotation += r * alpha


## Translate and shear are additive offsets from the setup pose. Returns the new value.
static func _offset(cur: float, setup: float, v: float, alpha: float, blend: int, before: bool) -> float:
	if before:
		if blend == MixBlend.SETUP:
			return setup
		if blend == MixBlend.FIRST:
			return cur + (setup - cur) * alpha
		return cur
	match blend:
		MixBlend.SETUP: return setup + v * alpha
		MixBlend.FIRST, MixBlend.REPLACE: return cur + (setup + v - cur) * alpha
		_: return cur + v * alpha


## Scale is multiplicative relative to the setup pose.
static func _scale(cur: float, setup: float, v: float, alpha: float, blend: int, before: bool) -> float:
	if before:
		if blend == MixBlend.SETUP:
			return setup
		if blend == MixBlend.FIRST:
			return cur + (setup - cur) * alpha
		return cur
	var x := v * setup
	if alpha == 1.0:
		if blend == MixBlend.ADD:
			return cur + x - setup
		return x
	match blend:
		MixBlend.SETUP: return setup + (x - setup) * alpha
		MixBlend.FIRST, MixBlend.REPLACE: return cur + (x - cur) * alpha
		_: return cur + (x - setup) * alpha


static func _apply_bone(tl: OsteonData.Timeline, sk: OsteonSkeleton, time: float, alpha: float, blend: int) -> void:
	var b := sk.bones[tl.target]
	if not b.active:
		return
	var d := sk.data.bones[tl.target]
	var before := time < tl.keys[0].time
	match tl.type:
		OsteonData.TimelineType.ROTATE: _apply_rotate(tl, sk, time, alpha, blend)
		OsteonData.TimelineType.TRANSLATE:
			b.x = _offset(b.x, d.x, 0.0 if before else value(tl, 0, time), alpha, blend, before)
			b.y = _offset(b.y, d.y, 0.0 if before else value(tl, 1, time), alpha, blend, before)
		OsteonData.TimelineType.TRANSLATE_X: b.x = _offset(b.x, d.x, 0.0 if before else value(tl, 0, time), alpha, blend, before)
		OsteonData.TimelineType.TRANSLATE_Y: b.y = _offset(b.y, d.y, 0.0 if before else value(tl, 0, time), alpha, blend, before)
		OsteonData.TimelineType.SCALE:
			b.scale_x = _scale(b.scale_x, d.scale_x, 1.0 if before else value(tl, 0, time), alpha, blend, before)
			b.scale_y = _scale(b.scale_y, d.scale_y, 1.0 if before else value(tl, 1, time), alpha, blend, before)
		OsteonData.TimelineType.SCALE_X: b.scale_x = _scale(b.scale_x, d.scale_x, 1.0 if before else value(tl, 0, time), alpha, blend, before)
		OsteonData.TimelineType.SCALE_Y: b.scale_y = _scale(b.scale_y, d.scale_y, 1.0 if before else value(tl, 0, time), alpha, blend, before)
		OsteonData.TimelineType.SHEAR:
			b.shear_x = _offset(b.shear_x, d.shear_x, 0.0 if before else value(tl, 0, time), alpha, blend, before)
			b.shear_y = _offset(b.shear_y, d.shear_y, 0.0 if before else value(tl, 1, time), alpha, blend, before)
		OsteonData.TimelineType.SHEAR_X: b.shear_x = _offset(b.shear_x, d.shear_x, 0.0 if before else value(tl, 0, time), alpha, blend, before)
		OsteonData.TimelineType.SHEAR_Y: b.shear_y = _offset(b.shear_y, d.shear_y, 0.0 if before else value(tl, 0, time), alpha, blend, before)
		OsteonData.TimelineType.INHERIT:
			if before:
				if blend == MixBlend.SETUP or blend == MixBlend.FIRST:
					b.inherit = d.inherit
			else:
				b.inherit = tl.keys[key_before(tl.keys, time)].int_value


static func _mix_color(c: Color, setup: Color, v: Color, alpha: float, blend: int, rgb_only: bool) -> Color:
	var out := c
	if alpha == 1.0:
		out.r = v.r
		out.g = v.g
		out.b = v.b
		if not rgb_only:
			out.a = v.a
		return out
	if blend == MixBlend.SETUP:
		out.r = setup.r
		out.g = setup.g
		out.b = setup.b
		if not rgb_only:
			out.a = setup.a
	out.r += (v.r - out.r) * alpha
	out.g += (v.g - out.g) * alpha
	out.b += (v.b - out.b) * alpha
	if not rgb_only:
		out.a += (v.a - out.a) * alpha
	return out


static func _apply_slot(tl: OsteonData.Timeline, sk: OsteonSkeleton, time: float, alpha: float, blend: int) -> void:
	var s := sk.slots[tl.target]
	if not sk.bones[s.bone].active:
		return
	var sd := sk.data.slots[tl.target]
	var before := time < tl.keys[0].time
	if tl.type == OsteonData.TimelineType.ATTACHMENT:
		if before:
			if blend == MixBlend.SETUP or blend == MixBlend.FIRST:
				s.attachment = sd.attachment
			return
		var n := tl.keys[key_before(tl.keys, time)].sval
		if s.attachment != n:
			s.attachment = n
			s.deform = PackedFloat64Array()
			s.deform_for = ""
		return
	if before:
		if blend == MixBlend.SETUP:
			s.color = sd.color
			s.dark = sd.dark
		elif blend == MixBlend.FIRST:
			s.color = _mix_color(s.color, sd.color, sd.color, alpha, blend, false)
			s.dark = _mix_color(s.dark, sd.dark, sd.dark, alpha, blend, true)
		return
	if tl.type == OsteonData.TimelineType.ALPHA:
		var a := value(tl, 0, time)
		if alpha == 1.0:
			s.color.a = a
		else:
			if blend == MixBlend.SETUP:
				s.color.a = sd.color.a
			s.color.a += (a - s.color.a) * alpha
		return
	var has_alpha := tl.type == OsteonData.TimelineType.RGBA or tl.type == OsteonData.TimelineType.RGBA2
	var light := Color(value(tl, 0, time), value(tl, 1, time), value(tl, 2, time), value(tl, 3, time) if has_alpha else s.color.a)
	s.color = _mix_color(s.color, sd.color, light, alpha, blend, not has_alpha)
	if tl.type == OsteonData.TimelineType.RGBA2 or tl.type == OsteonData.TimelineType.RGB2:
		var o := 4 if has_alpha else 3
		s.dark = _mix_color(s.dark, sd.dark, Color(value(tl, o, time), value(tl, o + 1, time), value(tl, o + 2, time), 1.0), alpha, blend, true)


static func _apply_deform(tl: OsteonData.Timeline, sk: OsteonSkeleton, time: float, alpha: float, blend: int) -> void:
	var s := sk.slots[tl.target]
	if not sk.bones[s.bone].active:
		return
	var cur := sk.attachment_for(s)
	if cur == null:
		return
	var match_ := cur.name == tl.attachment
	if not match_ and cur.type == OsteonData.AttachmentType.LINKED_MESH and cur.inherit_timelines:
		match_ = cur.parent_mesh == tl.attachment
	if not match_:
		return
	var geom := sk.geometry(s, cur)
	if geom == null:
		return
	var n := geom.deform_length()
	if s.deform_for != cur.name:
		s.deform = PackedFloat64Array()
		s.deform_for = cur.name
	if s.deform.size() != n:
		s.deform = PackedFloat64Array()
		s.deform.resize(n)
	var target := PackedFloat64Array()
	target.resize(n)
	if time >= tl.keys[0].time:
		var i := key_before(tl.keys, time)
		if i + 1 >= tl.keys.size():
			var kd := tl.keys[i].deform
			for j in n:
				target[j] = kd[j] if j < kd.size() else 0.0
		else:
			var p := _curve_percent(tl, i, time)
			var ka := tl.keys[i].deform
			var kb := tl.keys[i + 1].deform
			for j in n:
				var av := ka[j] if j < ka.size() else 0.0
				var bv := kb[j] if j < kb.size() else 0.0
				target[j] = av + (bv - av) * p
	elif blend != MixBlend.SETUP and blend != MixBlend.FIRST:
		return
	for j in n:
		match blend:
			MixBlend.SETUP: s.deform[j] = target[j] * alpha
			MixBlend.ADD: s.deform[j] += target[j] * alpha
			_: s.deform[j] += (target[j] - s.deform[j]) * alpha


static func _apply_sequence(tl: OsteonData.Timeline, sk: OsteonSkeleton, time: float, blend: int) -> void:
	var s := sk.slots[tl.target]
	if not sk.bones[s.bone].active:
		return
	var att := sk.attachment_for(s)
	if att == null or att.seq_count <= 0:
		return
	if time < tl.keys[0].time:
		if blend == MixBlend.SETUP or blend == MixBlend.FIRST:
			s.sequence_index = -1
		return
	var k := tl.keys[key_before(tl.keys, time)]
	var count := att.seq_count
	var index := k.int_value
	var mode := k.sval
	if mode != "hold" and mode != "" and k.v[0] > 0.0:
		index += int((time - k.time) / k.v[0] + 0.0001)
		var n := count * 2 - 2
		match mode:
			"once": index = mini(count - 1, index)
			"loop": index = index % count
			"pingpong":
				index = 0 if n == 0 else index % n
				if index >= count:
					index = n - index
			"onceReverse": index = maxi(count - 1 - index, 0)
			"loopReverse": index = count - 1 - (index % count)
			"pingpongReverse":
				index = 0 if n == 0 else (index + count - 1) % n
				if index >= count:
					index = n - index
	s.sequence_index = maxi(0, mini(count - 1, index))


static func _apply_draw_order(tl: OsteonData.Timeline, sk: OsteonSkeleton, time: float, blend: int) -> void:
	if time < tl.keys[0].time:
		if blend == MixBlend.SETUP or blend == MixBlend.FIRST:
			for i in sk.draw_order.size():
				sk.draw_order[i] = i
		return
	var k := tl.keys[key_before(tl.keys, time)]
	if k.draw_order.size() != sk.draw_order.size():
		for i in sk.draw_order.size():
			sk.draw_order[i] = i
	else:
		sk.draw_order = k.draw_order.duplicate()


static func _fire_events(tl: OsteonData.Timeline, data: OsteonData, last_time: float, time: float, out: Array) -> void:
	for k in tl.keys:
		if not (k.time > last_time and k.time <= time):
			continue
		var e := FiredEvent.new()
		e.event_index = data.find_event(k.sval)
		e.name = k.sval
		e.time = k.time
		e.int_value = k.int_value
		e.float_value = k.v[0]
		e.volume = k.volume
		e.balance = k.balance
		if e.event_index >= 0:
			var ed := data.events[e.event_index]
			if not k.flag1:
				e.int_value = ed.int_value
			if not k.flag2:
				e.float_value = ed.float_value
			e.string_value = k.sval2 if k.flag3 else ed.string_value
		out.append(e)


static func _mix_value(cur: float, setup: float, target: float, before: bool, alpha: float, blend: int) -> float:
	if before:
		if blend == MixBlend.SETUP:
			return setup
		if blend == MixBlend.FIRST:
			return cur + (setup - cur) * alpha
		return cur
	if blend == MixBlend.SETUP:
		return setup + (target - setup) * alpha
	return cur + (target - cur) * alpha


static func _apply_constraint(tl: OsteonData.Timeline, sk: OsteonSkeleton, time: float, alpha: float, blend: int) -> void:
	var before := time < tl.keys[0].time
	if tl.type == OsteonData.TimelineType.IK:
		if tl.target >= sk.ik.size():
			return
		var c := sk.ik[tl.target]
		var d := sk.data.ik[tl.target]
		c.mix = _mix_value(c.mix, d.mix, 0.0 if before else value(tl, 0, time), before, alpha, blend)
		c.softness = _mix_value(c.softness, d.softness, 0.0 if before else value(tl, 1, time), before, alpha, blend)
		if before:
			if blend == MixBlend.SETUP or blend == MixBlend.FIRST:
				c.bend_direction = d.bend_direction
				c.compress = d.compress
				c.stretch = d.stretch
		else:
			var k := tl.keys[key_before(tl.keys, time)]
			c.bend_direction = 1 if k.flag1 else -1
			c.compress = k.flag2
			c.stretch = k.flag3
	elif tl.type == OsteonData.TimelineType.TRANSFORM:
		if tl.target >= sk.transforms.size():
			return
		var c := sk.transforms[tl.target]
		var d := sk.data.transforms[tl.target]
		c.mix_rotate = _mix_value(c.mix_rotate, d.mix_rotate, 0.0 if before else value(tl, 0, time), before, alpha, blend)
		c.mix_x = _mix_value(c.mix_x, d.mix_x, 0.0 if before else value(tl, 1, time), before, alpha, blend)
		c.mix_y = _mix_value(c.mix_y, d.mix_y, 0.0 if before else value(tl, 2, time), before, alpha, blend)
		c.mix_scale_x = _mix_value(c.mix_scale_x, d.mix_scale_x, 0.0 if before else value(tl, 3, time), before, alpha, blend)
		c.mix_scale_y = _mix_value(c.mix_scale_y, d.mix_scale_y, 0.0 if before else value(tl, 4, time), before, alpha, blend)
		c.mix_shear_y = _mix_value(c.mix_shear_y, d.mix_shear_y, 0.0 if before else value(tl, 5, time), before, alpha, blend)


## Applies `anim` to the skeleton's local pose. Events the playhead passed are appended to `events` (may be null).
static func apply(anim: OsteonData.Anim, sk: OsteonSkeleton, last_time: float, time: float, loop: bool, events: Array, alpha: float, blend: int) -> void:
	var duration := anim.duration
	if loop and duration > 0.0:
		time = fmod(time, duration)
		if last_time > 0.0:
			last_time = fmod(last_time, duration)
	for tl in anim.timelines:
		if tl.keys.is_empty():
			continue
		match tl.type:
			OsteonData.TimelineType.EVENT:
				if events != null:
					if last_time > time:
						_fire_events(tl, sk.data, last_time, duration, events)
						_fire_events(tl, sk.data, -1.0, time, events)
					else:
						_fire_events(tl, sk.data, last_time, time, events)
			OsteonData.TimelineType.ROTATE, OsteonData.TimelineType.TRANSLATE, OsteonData.TimelineType.TRANSLATE_X, \
			OsteonData.TimelineType.TRANSLATE_Y, OsteonData.TimelineType.SCALE, OsteonData.TimelineType.SCALE_X, \
			OsteonData.TimelineType.SCALE_Y, OsteonData.TimelineType.SHEAR, OsteonData.TimelineType.SHEAR_X, \
			OsteonData.TimelineType.SHEAR_Y, OsteonData.TimelineType.INHERIT:
				if tl.target >= 0 and tl.target < sk.bones.size():
					_apply_bone(tl, sk, time, alpha, blend)
			OsteonData.TimelineType.ATTACHMENT, OsteonData.TimelineType.RGBA, OsteonData.TimelineType.RGB, \
			OsteonData.TimelineType.ALPHA, OsteonData.TimelineType.RGBA2, OsteonData.TimelineType.RGB2:
				if tl.target >= 0 and tl.target < sk.slots.size():
					_apply_slot(tl, sk, time, alpha, blend)
			OsteonData.TimelineType.DEFORM:
				if tl.target >= 0 and tl.target < sk.slots.size():
					_apply_deform(tl, sk, time, alpha, blend)
			OsteonData.TimelineType.SEQUENCE:
				if tl.target >= 0 and tl.target < sk.slots.size():
					_apply_sequence(tl, sk, time, blend)
			OsteonData.TimelineType.DRAW_ORDER:
				if alpha >= 0.5 or blend == MixBlend.SETUP:
					_apply_draw_order(tl, sk, time, blend)
			_:
				_apply_constraint(tl, sk, time, alpha, blend)
