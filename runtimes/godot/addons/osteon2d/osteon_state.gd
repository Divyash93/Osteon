class_name OsteonState
extends RefCounted
## Track based playback with crossfades.

class Entry:
	var animation := -1
	var loop := true
	var track_time := 0.0
	var last_time := -1.0
	var time_scale := 1.0
	var alpha := 1.0
	var mix_time := 0.0
	var mix_duration := 0.0
	var delay := 0.0
	var animation_start := 0.0
	var animation_end := 0.0
	var from: Entry = null
	var next: Entry = null

## Emitted for every animation event the playhead passes: (track, OsteonAnim.FiredEvent).
signal event(track: int, fired: OsteonAnim.FiredEvent)

var data: OsteonData
var time_scale := 1.0
var default_mix := 0.2
var mixes := {}  # "from>to" -> duration
var tracks: Array = []  # of Entry or null


func _init(skeleton_data: OsteonData) -> void:
	data = skeleton_data


func set_mix(from_name: String, to_name: String, duration: float) -> void:
	mixes["%d>%d" % [data.find_animation(from_name), data.find_animation(to_name)]] = duration

func mix_duration(from_anim: int, to_anim: int) -> float:
	return mixes.get("%d>%d" % [from_anim, to_anim], default_mix)


func set_animation_by_name(track: int, anim_name: String, loop: bool) -> Entry:
	var a := data.find_animation(anim_name)
	if a < 0:
		push_error("Osteon2D: animation not found: " + anim_name)
		return null
	return set_animation(track, a, loop)


func set_animation(track: int, animation: int, loop: bool) -> Entry:
	while tracks.size() <= track:
		tracks.append(null)
	var e := Entry.new()
	e.animation = animation
	e.loop = loop
	if animation >= 0:
		e.animation_end = data.animations[animation].duration
	var cur: Entry = tracks[track]
	if cur != null and cur.animation >= 0:
		e.from = cur
		cur.next = null
		e.mix_duration = mix_duration(cur.animation, animation)
	tracks[track] = e
	return e


func add_animation_by_name(track: int, anim_name: String, loop: bool, delay: float) -> Entry:
	var animation := data.find_animation(anim_name)
	if animation < 0:
		push_error("Osteon2D: animation not found: " + anim_name)
		return null
	if tracks.size() <= track or tracks[track] == null:
		return set_animation(track, animation, loop)
	var last: Entry = tracks[track]
	while last.next != null:
		last = last.next
	var e := Entry.new()
	e.animation = animation
	e.loop = loop
	e.animation_end = data.animations[animation].duration
	if delay <= 0.0:
		var d := last.animation_end - last.animation_start
		delay = maxf(0.0, d + delay - mix_duration(last.animation, animation))
	e.delay = delay
	last.next = e
	return e


func clear_track(track: int) -> void:
	if track < tracks.size():
		tracks[track] = null

func clear_tracks() -> void:
	tracks.clear()


func update(delta: float) -> void:
	delta *= time_scale
	for i in tracks.size():
		var t: Entry = tracks[i]
		if t == null:
			continue
		var d := delta * t.time_scale
		t.last_time = t.track_time
		t.track_time += d
		if t.from != null:
			t.mix_time += d
			if t.mix_time >= t.mix_duration:
				t.from = null
			else:
				t.from.last_time = t.from.track_time
				t.from.track_time += d
		if t.next != null:
			var dur := t.animation_end - t.animation_start
			if t.track_time >= t.next.delay or ((not t.loop) and t.track_time >= dur and t.next.delay <= t.track_time):
				var n := t.next
				n.from = t
				n.mix_duration = mix_duration(t.animation, n.animation)
				t.next = null
				tracks[i] = n


func apply(sk: OsteonSkeleton) -> void:
	var events: Array = []
	for ti in tracks.size():
		var t: Entry = tracks[ti]
		if t == null or t.animation < 0 or t.animation >= data.animations.size():
			continue
		var blend := OsteonAnim.MixBlend.FIRST if ti == 0 else OsteonAnim.MixBlend.REPLACE
		var alpha := t.alpha
		if t.from != null and t.from.animation >= 0 and t.from.animation < data.animations.size():
			OsteonAnim.apply(data.animations[t.from.animation], sk, t.from.last_time, t.from.track_time, t.from.loop, [], 1.0, blend)
			blend = OsteonAnim.MixBlend.REPLACE
			alpha *= minf(1.0, t.mix_time / t.mix_duration) if t.mix_duration > 0.0 else 1.0
		var a := data.animations[t.animation]
		var dur := a.duration
		var time := t.track_time if t.loop else minf(t.track_time, dur)
		var last := -1.0 if t.last_time < 0.0 else (t.last_time if t.loop else minf(t.last_time, dur))
		var before := events.size()
		OsteonAnim.apply(a, sk, last, time, t.loop, events, alpha, blend)
		for i in range(before, events.size()):
			event.emit(ti, events[i])
